/***** PylontechMonitoring.ino (ESP8266 Wemos D1 mini)

 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>  // For balance history storage
#include <DNSServer.h> // For captive portal

#define DBG_WIFI 1
#define DBG_WEB 1
#define DBG_BMS 0

// En el repo la lógica usa "Serial2"; aquí lo aliasamos a UART0
#define Serial2 Serial

#include "PylontechMonitoring.h" // define WIFI_SSID, WIFI_PASS, WIFI_HOSTNAME (+ opcional STATIC_IP e IPs)
#include "batteryStack.h"
#include "commandParser.h"
#include "webInterface.h"
#include "buildinfo.h"
#include "wifiConfig.h"

// Forward declaration for BMS command function
String _bmsSendCmd(const String &cmd, uint32_t timeout_ms);

WebServer server(80);
batteryStack stack;
bool wifiConnected = false;

static void dumpNet()
{
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("GW: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
}

static void connectWiFi()
{
  Serial.println("\n[WiFi] Attempting to connect...");

  // First try to load saved configuration
  if (wifiConfig.loadConfig())
  {
    Serial.println("[WiFi] Using saved configuration");
    if (wifiConfig.connectToWiFi())
    {
      wifiConnected = true;
      WiFi.hostname(WIFI_HOSTNAME);
      dumpNet();
      return;
    }
  }

  // If no saved config or connection failed, try hardcoded values as fallback
  Serial.println("[WiFi] Trying hardcoded configuration as fallback...");
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);

#ifdef STATIC_IP
  WiFi.config(ip, gateway, subnet, dns);
#endif

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 25000)
  {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("[WiFi] ✅ Connected with hardcoded config");
    wifiConnected = true;
    dumpNet();
  }
  else
  {
    Serial.println("[WiFi] ❌ All connection attempts failed");
    Serial.println("[WiFi] Starting configuration portal...");
    wifiConfig.startConfigPortal();
  }
}

static void setupOTA()
{
  ArduinoOTA.setHostname(WIFI_HOSTNAME);
  ArduinoOTA.setPassword("ota123"); // cambia esta clave
  ArduinoOTA.begin();
  Serial.println("[OTA] Listo (8266)");
}

void setup()
{
  Serial.begin(115200);
  // Si prefieres D7/D8:
  // Serial.swap();

  connectWiFi();

  // Only setup OTA if WiFi is connected
  if (wifiConnected)
  {
    setupOTA();
  }

  // Initialize battery stack and load history
  stack.init();
  Serial.println("[HISTORY] Stack initialized");

  if (stack.loadBalanceHistory())
  {
    Serial.println("[HISTORY] Balance history loaded from flash");
    Serial.print("[HISTORY] Loaded entries: ");
    Serial.println(stack.history.entryCount);
  }
  else
  {
    Serial.println("[HISTORY] No existing history found - starting fresh");
  }

  // Setup web interface only if WiFi is connected
  if (wifiConnected)
  {
    setupWebInterface(server, &stack); // Web estilo original del repo
    server.begin();
    Serial.println("HTTP server listo");
  }
}

void loop()
{
  // Handle configuration portal if in AP mode
  if (wifiConfig.isInAPMode())
  {
    wifiConfig.handleConfigPortal();
    // Don't return here - we can still do basic tasks like recording history
  }

  // Check if we should record balance history (every 2 minutes for testing)
  unsigned long currentTime = millis();
  if (stack.shouldRecordHistory(currentTime))
  {
    Serial.println("[HISTORY] Recording balance history...");

    // Add some dummy data for testing if no real data available
    // In real implementation, this should be actual battery data
    for (int i = 1; i <= 3; i++)
    {                                        // Simulate 3 batteries
      int16_t dummyBalance = random(20, 80); // Random balance between 20-80mV
      uint8_t dummySoc = random(70, 95);     // Random SOC between 70-95%
      stack.history.addEntry(i, dummyBalance, dummySoc, currentTime);
      Serial.print("[HISTORY] Added entry for battery ");
      Serial.print(i);
      Serial.print(": Balance=");
      Serial.print(dummyBalance);
      Serial.print("mV, SOC=");
      Serial.print(dummySoc);
      Serial.println("%");
    }

    stack.updateLastSaveTime(currentTime);

    // Save to persistent storage every 4 records (8 minutes with 2min interval)
    static uint8_t saveCounter = 0;
    saveCounter++;
    if (saveCounter >= 4)
    {
      if (stack.saveBalanceHistory())
      {
        Serial.println("[HISTORY] Balance history saved to flash");
      }
      else
      {
        Serial.println("[HISTORY] Failed to save balance history");
      }
      saveCounter = 0;
    }

    Serial.print("[HISTORY] Total entries: ");
    Serial.println(stack.history.entryCount);
  }

  // Normal operation when WiFi is connected
  if (wifiConnected)
  {
    server.handleClient();
    ArduinoOTA.handle();
  }
  else
  {
    // Check if WiFi reconnected
    if (WiFi.status() == WL_CONNECTED)
    {
      wifiConnected = true;
      Serial.println("[WiFi] ✅ Reconnected!");
      dumpNet();

      // Setup services now that WiFi is available
      setupOTA();
      setupWebInterface(server, &stack);
      server.begin();
      Serial.println("HTTP server listo");
    }
  }
}
