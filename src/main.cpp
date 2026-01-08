#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>
#include <ESPmDNS.h>
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
// Ethernet (LAN) Setup
// =================================================
void setupEthernetLAN() {
	debugPrint("[ETH] Initializing LAN Ethernet");

	WiFi.onEvent(onNetworkEvent);

	IPAddress ip(LAN_IP_OCTETS);
  IPAddress gw(LAN_GATEWAY_OCTETS);
  IPAddress sn(LAN_SUBNET_OCTETS);
  IPAddress dns(LAN_DNS_OCTETS);


	if (!ETH.config(ip, gw, sn, dns)) {
		debugPrint("[ETH] Static IP config FAILED");
		return;
	}

	debugPrintf("[ETH] Static IP set: %s", ip.toString().c_str());

	// PHY parameters come from platformio.ini
	ETH.begin();
}

// =================================================
// WiFi Access Point (LAN)
// =================================================
void setupWiFiLAN() {
	debugPrint("[AP] Initializing LAN WiFi AP");

	IPAddress ip(LAN_IP_OCTETS);
  IPAddress gw(LAN_GATEWAY_OCTETS);
  IPAddress sn(LAN_SUBNET_OCTETS);

	if (!WiFi.softAPConfig(ip, gw, sn)) {
		debugPrint("[AP] softAPConfig FAILED");
		return;
	}

	if (!WiFi.softAP(
		AP_SSID,
		AP_PASSWORD,
		AP_CHANNEL,
		false,              // SSID visible
		AP_MAX_CLIENTS
	)) {
		debugPrint("[AP] softAP start FAILED");
		return;
	}

	debugPrint("[AP] WiFi AP ready (DHCP enabled)");
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
	Serial.printf("LAN IP    : %d.%d.%d.%d\n", LAN_IP_OCTETS);
	Serial.println("----------------------------------------");

	// LAN comes up first — always
	setupEthernetLAN();
	delay(1500);
	setupWiFiLAN();

	// Local discovery (LAN only)
	if (MDNS.begin(DEVICE_ID)) {
		debugPrintf("[mDNS] %s.local active", DEVICE_ID);
	}

	Serial.println("----------------------------------------");
	Serial.println("LAN gateway ONLINE");
	Serial.println("========================================");
}

// =================================================
// Loop
// =================================================
void loop() {
	static uint32_t last = 0;
	uint32_t now = millis();

	if (now - last >= 30000) {
		last = now;

		Serial.println();
		Serial.println("=== STATUS ===");
		Serial.printf("Uptime : %lus\n", now / 1000);
		Serial.printf("ETH    : %s\n", eth_up ? "UP" : "DOWN");
		Serial.printf("WiFi AP: %s (%d clients)\n",
		              ap_up ? "UP" : "DOWN",
		              WiFi.softAPgetStationNum());
		Serial.println("==============");
	}

	delay(100);
}
