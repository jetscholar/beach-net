#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "env.h"

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
// AP subnet defaults (override in env.h if needed)
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
// Network Events
// =================================================
void onNetworkEvent(WiFiEvent_t event) {
	switch (event) {

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
			break;

		case ARDUINO_EVENT_ETH_DISCONNECTED:
			debugPrint("[ETH] Link DOWN");
			eth_up = false;
			break;

		case ARDUINO_EVENT_WIFI_AP_START:
			ap_up = true;
			debugPrint("[AP] Started");
			debugPrintf("[AP] IP: %s", WiFi.softAPIP().toString().c_str());
			break;

		default:
			break;
	}
}

// =================================================
// Ethernet (LAN / Pi side)
// =================================================
void setupEthernetLAN() {
	debugPrint("[ETH] Initializing Ethernet");

	WiFi.onEvent(onNetworkEvent);

	// PHY power (WT32-ETH01 = GPIO16)
	pinMode(16, OUTPUT);
	digitalWrite(16, HIGH);
	delay(300);

	ETH.begin();   // IMPORTANT: begin first

	delay(200);

	IPAddress ip(LAN_IP_OCTETS);
	IPAddress gw(LAN_GATEWAY_OCTETS);
	IPAddress sn(LAN_SUBNET_OCTETS);
	IPAddress dns(LAN_DNS_OCTETS);

	if (!ETH.config(ip, gw, sn, dns)) {
		debugPrint("[ETH] Static IP config FAILED");
	} else {
		debugPrintf("[ETH] Static IP set: %s", ip.toString().c_str());
	}
}

// =================================================
// WiFi AP (client side)
// =================================================
void setupWiFiLAN() {
	debugPrint("[AP] Initializing WiFi AP");

	IPAddress ap_ip(AP_IP_OCTETS);
	IPAddress ap_gw(AP_IP_OCTETS);
	IPAddress ap_sn(AP_SUBNET_OCTETS);

	if (!WiFi.softAPConfig(ap_ip, ap_gw, ap_sn)) {
		debugPrint("[AP] softAPConfig FAILED");
		return;
	}

	if (!WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CLIENTS)) {
		debugPrint("[AP] softAP start FAILED");
		return;
	}

	debugPrintf("[AP] DHCP: %d.%d.%d.100 - 200", AP_IP_OCTETS);
	debugPrint("[AP] WiFi AP ready");
}

// =================================================
// OTA
// =================================================
void setupOTA() {
	debugPrint("[OTA] Initializing");

	ArduinoOTA.setHostname(DEVICE_ID);
	ArduinoOTA.setPassword("beachnet-ota");

	ArduinoOTA.onStart([]() {
		debugPrint("[OTA] Start");
	});

	ArduinoOTA.onEnd([]() {
		debugPrint("[OTA] End");
	});

	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("[OTA] Error %u\n", error);
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

	setupEthernetLAN();
	delay(1500);

	setupWiFiLAN();
	setupOTA();

	if (MDNS.begin(DEVICE_ID)) {
		debugPrintf("[mDNS] %s.local", DEVICE_ID);
	}

	Serial.println("========================================");
}

// =================================================
// Loop
// =================================================
void loop() {
	ArduinoOTA.handle();

	static uint32_t last = 0;
	if (millis() - last > 30000) {
		last = millis();

		Serial.println();
		Serial.println("=== STATUS ===");
		Serial.printf("ETH : %s\n", eth_up ? "UP" : "DOWN");
		Serial.printf("AP  : %s (%d clients)\n",
			ap_up ? "UP" : "DOWN",
			WiFi.softAPgetStationNum());
	}
	delay(50);
}


