#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "env.h"

// NAPT / routing (ESP32 Arduino core with lwIP NAPT enabled)
extern "C" {
	#include "lwip/lwip_napt.h"
}

#ifndef STATION_IF
	#define STATION_IF 0
#endif
#ifndef SOFTAP_IF
	#define SOFTAP_IF 1
#endif
#ifndef ETH_IF
	#define ETH_IF 2
#endif

// =================================================
// State
// =================================================
static bool eth_up = false;
static bool ap_up  = false;

// =================================================
// Debug helpers
// =================================================
void debugPrint(const char* msg) {
#if DEBUG_ENABLED
	Serial.println(msg);
#endif
}

void debugPrintf(const char* fmt, ...) {
#if DEBUG_ENABLED
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	Serial.println(buf);
#endif
}

// =================================================
// AP subnet defaults (separate from Ethernet LAN)
// If you want to override, define these in env.h
// =================================================
#ifndef AP_IP_OCTETS
	#define AP_IP_OCTETS 172,18,2,1
#endif
#ifndef AP_SUBNET_OCTETS
	#define AP_SUBNET_OCTETS 255,255,255,0
#endif
#ifndef AP_DHCP_START_OCTETS
	#define AP_DHCP_START_OCTETS 172,18,2,100
#endif
#ifndef AP_DHCP_END_OCTETS
	#define AP_DHCP_END_OCTETS 172,18,2,200
#endif

// =================================================
// Unified Network Event Handler
// =================================================
void onNetworkEvent(WiFiEvent_t event) {
	switch (event) {

		// ---------- Ethernet ----------
		case ARDUINO_EVENT_ETH_START:
			debugPrint("[ETH] Started");
			ETH.setHostname(DEVICE_ID);
			break;

		case ARDUINO_EVENT_ETH_CONNECTED:
			debugPrint("[ETH] Link UP");
			break;

		case ARDUINO_EVENT_ETH_GOT_IP:
			eth_up = true;
			debugPrintf("[ETH] IP: %s", ETH.localIP().toString().c_str());
			debugPrintf("[ETH] GW: %s", ETH.gatewayIP().toString().c_str());
			debugPrintf("[ETH] SN: %s", ETH.subnetMask().toString().c_str());
			break;

		case ARDUINO_EVENT_ETH_DISCONNECTED:
			debugPrint("[ETH] Link DOWN");
			eth_up = false;
			break;

		// ---------- WiFi AP ----------
		case ARDUINO_EVENT_WIFI_AP_START:
			ap_up = true;
			debugPrint("[AP] Started");
			debugPrintf("[AP] SSID: %s", AP_SSID);
			debugPrintf("[AP] IP: %s", WiFi.softAPIP().toString().c_str());
			break;

		case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
			debugPrintf("[AP] Client joined (%d total)",
			            WiFi.softAPgetStationNum());
			break;

		case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
			debugPrintf("[AP] Client left (%d total)",
			            WiFi.softAPgetStationNum());
			break;

		default:
			break;
	}
}

// =================================================
// Ethernet (LAN) — this is the "wired" side (Pi side)
// =================================================
void setupEthernetLAN() {
	debugPrint("[ETH] Initializing LAN Ethernet");

	WiFi.onEvent(onNetworkEvent);

	// Ensure PHY power is enabled (WT32-ETH01 LAN8720)
#if defined(ETH_PHY_POWER)
	pinMode(ETH_PHY_POWER, OUTPUT);
	digitalWrite(ETH_PHY_POWER, HIGH);
	delay(300);
#else
	pinMode(16, OUTPUT);
	digitalWrite(16, HIGH);
	delay(300);
#endif

	IPAddress ip(LAN_IP_OCTETS);
	IPAddress gw(LAN_GATEWAY_OCTETS);
	IPAddress sn(LAN_SUBNET_OCTETS);
	IPAddress dns(LAN_DNS_OCTETS);

	if (!ETH.config(ip, gw, sn, dns)) {
		debugPrint("[ETH] Static IP config FAILED");
		return;
	}

	debugPrintf("[ETH] Static IP set: %s", ip.toString().c_str());

	delay(100);
	ETH.begin();
}

// =================================================
// WiFi AP — separate subnet from Ethernet (required)
// =================================================
void setupWiFiLAN() {
	debugPrint("[AP] Initializing WiFi AP");

	IPAddress ap_ip(AP_IP_OCTETS);
	IPAddress ap_gw(AP_IP_OCTETS);      // AP gateway is WT32 itself
	IPAddress ap_sn(AP_SUBNET_OCTETS);

	if (!WiFi.softAPConfig(ap_ip, ap_gw, ap_sn)) {
		debugPrint("[AP] softAPConfig FAILED");
		return;
	}

	if (!WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CLIENTS)) {
		debugPrint("[AP] softAP start FAILED");
		return;
	}

	IPAddress dhcp_start(AP_DHCP_START_OCTETS);
	IPAddress dhcp_end(AP_DHCP_END_OCTETS);
	debugPrintf("[AP] DHCP range (info): %s - %s",
	            dhcp_start.toString().c_str(),
	            dhcp_end.toString().c_str());

	debugPrint("[AP] WiFi AP ready (DHCP enabled)");
}

// =================================================
// Enable routing/NAPT so AP clients can reach Ethernet LAN
// =================================================
void setupRouting() {
	debugPrint("[NET] Enabling routing/NAPT (AP -> ETH)");

	// NAPT must be enabled in lwIP build. If it's not, upload will compile-fail.
	// Enable NAPT on the "inside" (SOFTAP_IF). This is the common ESP32 router setup.
	ip_napt_enable_no(SOFTAP_IF, 1);

	// Some builds also need ETH_IF explicitly enabled. Safe to call.
	ip_napt_enable_no(ETH_IF, 1);

	debugPrint("[NET] NAPT enabled");
}

// =================================================
// OTA
// =================================================
void setupOTA() {
	debugPrint("[OTA] Initializing");

	ArduinoOTA.setHostname(DEVICE_ID);
	ArduinoOTA.setPassword("beachnet-ota");

	ArduinoOTA.onStart([]() {
		debugPrint("[OTA] Update started");
	});

	ArduinoOTA.onEnd([]() {
		debugPrint("[OTA] Update complete");
	});

	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("[OTA] Error[%u]\n", error);
	});

	ArduinoOTA.begin();

	debugPrint("[OTA] Ready");
}

// =================================================
// Setup
// =================================================
void setup() {
	Serial.begin(115200);
	delay(1500);

	Serial.println();
	Serial.println("========================================");
	Serial.println(" WT32-ETH01 BEACH HOUSE GATEWAY");
	Serial.println("========================================");
	Serial.printf("Device ID : %s\n", DEVICE_ID);
	Serial.printf("ETH (LAN) : %d.%d.%d.%d\n", LAN_IP_OCTETS);
	Serial.printf("AP  (LAN) : %d.%d.%d.%d\n", AP_IP_OCTETS);
	Serial.println("----------------------------------------");

	setupEthernetLAN();
	delay(1500);
	setupWiFiLAN();
	setupRouting();
	setupOTA();

	if (MDNS.begin(DEVICE_ID)) {
		debugPrintf("[mDNS] %s.local active", DEVICE_ID);
	}

	Serial.println("----------------------------------------");
	Serial.println("Gateway ONLINE");
	Serial.println("========================================");
}

// =================================================
// Loop
// =================================================
void loop() {
	ArduinoOTA.handle();

	static uint32_t last = 0;
	uint32_t now = millis();

	if (now - last >= 30000) {
		last = now;

		Serial.println();
		Serial.println("=== STATUS ===");
		Serial.printf("Uptime : %lus\n", now / 1000);
		Serial.printf("ETH    : %s\n", eth_up ? "UP" : "DOWN");
		if (eth_up) {
			Serial.printf("  ETH IP: %s\n", ETH.localIP().toString().c_str());
		}
		Serial.printf("WiFi AP: %s (%d clients)\n",
		              ap_up ? "UP" : "DOWN",
		              WiFi.softAPgetStationNum());
		if (ap_up) {
			Serial.printf("  AP  IP: %s\n", WiFi.softAPIP().toString().c_str());
		}
		Serial.println("==============");
	}

	delay(50);
}

