/***** PylontechMonitoring.ino (ESP8266 Wemos D1 mini)

 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>  // For balance history storage
#include <DNSServer.h> // For captive portal
#include <time.h>      // For NTP time sync
#include <WiFiUdp.h>   // For NTP
#include <NTPClient.h> // For NTP time synchronization

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

// NTP Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 2 * 3600, 60000); // UTC+2 (Madrid, España CEST - horario de verano), update every minute
unsigned long lastNtpSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000; // Sync every hour

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

// Function to update battery data array from BMS commands
void updateBatteryData()
{
  // Clear all battery data first
  for (int i = 0; i < MAX_PYLON_BATTERIES_SUPPORTED; i++)
  {
    stack.batts[i].isPresent = false;
    stack.batts[i].soc = 0;
    stack.batts[i].voltage = 0;
    stack.batts[i].current = 0;
    stack.batts[i].tempr = 0;
    stack.batts[i].cellVoltHigh = 0;
    stack.batts[i].cellVoltLow = 0;
  }

  // Get battery data using the same logic as /battery-data endpoint
  String raw = _bmsSendCmd("bat", 4000);

  // Parse the 'bat' command response
  int cells = 0;
  long sum_mV = 0;
  long sum_mA = 0;
  long sum_mC = 0;
  long sum_soc = 0;
  long maxVoltage = 0;
  long minVoltage = 99999;

  int start = 0;
  while (start < (int)raw.length())
  {
    int end = raw.indexOf('\n', start);
    String line = (end < 0) ? raw.substring(start) : raw.substring(start, end);
    start = (end < 0) ? raw.length() : end + 1;
    line.trim();

    if (line.length() == 0 || !isDigit(line[0]))
      continue;

    // Parse line: cell_id voltage current temp soc%
    const char *s = line.c_str();
    long vals[4];
    int n = 0;
    while (*s && n < 4)
    {
      while (*s && !((*s == '-') || (*s >= '0' && *s <= '9')))
        s++;
      if (!*s)
        break;
      char *end;
      long v = strtol(s, &end, 10);
      vals[n++] = v;
      s = end;
    }

    if (n >= 4)
    {
      long mv = vals[1]; // voltage in mV
      long ma = vals[2]; // current in mA
      long mC = vals[3]; // temperature in mC

      // Extract SOC from line (number before %)
      int pcent = line.indexOf('%');
      int soc = 0;
      if (pcent > 0)
      {
        int pnum = pcent - 1;
        while (pnum >= 0 && isDigit(line[pnum]))
          pnum--;
        pnum++;
        soc = line.substring(pnum, pcent).toInt();
      }

      cells++;
      sum_mV += mv;
      sum_mA += ma;
      sum_mC += mC;
      sum_soc += soc;

      // Track min/max voltages for balance calculation
      if (mv > maxVoltage)
        maxVoltage = mv;
      if (mv < minVoltage)
        minVoltage = mv;
    }
  }

  // If we found cells, populate the first battery slot with aggregated data
  if (cells > 0)
  {
    stack.batts[0].isPresent = true;
    stack.batts[0].soc = sum_soc / cells;     // Average SOC
    stack.batts[0].voltage = sum_mV;          // Total voltage in mV
    stack.batts[0].current = sum_mA / cells;  // Average current in mA
    stack.batts[0].tempr = sum_mC / cells;    // Average temperature in mC
    stack.batts[0].cellVoltHigh = maxVoltage; // Highest cell voltage
    stack.batts[0].cellVoltLow = minVoltage;  // Lowest cell voltage

    Serial.print("[BATTERY UPDATE] Updated battery 1: SOC=");
    Serial.print(stack.batts[0].soc);
    Serial.print("%, Balance=");
    Serial.print(maxVoltage - minVoltage);
    Serial.println("mV");
  }
}

// Function to get current Unix timestamp (real time, not millis)
unsigned long getCurrentTimestamp()
{
  if (wifiConnected && timeClient.isTimeSet())
  {
    return timeClient.getEpochTime();
  }
  // Fallback to millis() if NTP not available (should not be used for production)
  return millis() / 1000; // Convert millis to seconds for compatibility
}

// Function to initialize and sync NTP
void setupNTP()
{
  if (wifiConnected)
  {
    Serial.println("[NTP] Initializing time synchronization...");
    timeClient.begin();
    timeClient.update();

    if (timeClient.isTimeSet())
    {
      Serial.print("[NTP] Time synchronized: ");
      Serial.println(timeClient.getFormattedTime());
      lastNtpSync = millis();
    }
    else
    {
      Serial.println("[NTP] Failed to synchronize time");
    }
  }
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
    setupNTP(); // Initialize NTP time synchronization
  }

  // Initialize battery stack and load history
  stack.init();
  Serial.println("[HISTORY] Stack initialized");
  Serial.print("[HISTORY] Current millis(): ");
  Serial.println(millis());

  if (stack.loadBalanceHistory())
  {
    Serial.println("[HISTORY] Balance history loaded from flash");
    Serial.print("[HISTORY] Loaded entries: ");
    Serial.println(stack.history.entryCount);
    Serial.print("[HISTORY] Last save time from flash: ");
    Serial.println(stack.history.lastSaveTime);
  }
  else
  {
    Serial.println("[HISTORY] No existing history found - starting fresh");
  }

  // Force reset lastSaveTime to ensure first record happens quickly
  stack.history.lastSaveTime = 0;
  Serial.println("[HISTORY] Reset lastSaveTime to 0 for quick first record");

  // For testing: Force immediate recording by setting lastSaveTime to an old value
  unsigned long currentTime = millis();
  if (currentTime > 60000) // If we've been running for more than 1 minute
  {
    stack.history.lastSaveTime = currentTime - 61000; // Set to 61 seconds ago
    Serial.println("[HISTORY] Forced lastSaveTime to 61 seconds ago for immediate recording");
  }

  // Test shouldRecordHistory function
  unsigned long testTime = millis();
  Serial.print("[HISTORY] Testing shouldRecordHistory() at ");
  Serial.print(testTime);
  Serial.print(": ");
  Serial.println(stack.shouldRecordHistory(testTime) ? "YES" : "NO");

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

  // Periodic NTP synchronization (every hour)
  if (wifiConnected && (millis() - lastNtpSync > NTP_SYNC_INTERVAL))
  {
    timeClient.update();
    if (timeClient.isTimeSet())
    {
      Serial.print("[NTP] Time resynchronized: ");
      Serial.println(timeClient.getFormattedTime());
      lastNtpSync = millis();
    }
  }

  // Update battery data periodically (every 30 seconds)
  static unsigned long lastBatteryUpdate = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastBatteryUpdate > 30000) // 30 seconds
  {
    updateBatteryData();
    lastBatteryUpdate = currentTime;
  }

  // Check if we should record balance history (every 15 minutes)
  if (stack.shouldRecordHistory(getCurrentTimestamp()))
  {
    Serial.println("[HISTORY] Recording balance history...");

    // Check if we have real battery data
    bool hasRealData = false;
    for (int i = 0; i < MAX_PYLON_BATTERIES_SUPPORTED; i++)
    {
      if (stack.batts[i].isPresent)
      {
        hasRealData = true;
        break;
      }
    }

    if (hasRealData)
    {
      // Record actual battery data with real timestamp
      stack.recordBalanceHistory(getCurrentTimestamp());
      Serial.println("[HISTORY] Real battery data recorded");
    }
    else
    {
      // No batteries detected - skip recording
      Serial.println("[HISTORY] No batteries detected, skipping recording until real batteries are connected");
    }

    stack.updateLastSaveTime(getCurrentTimestamp());

    // Save to persistent storage every 2 records (2 minutes with 1min interval for testing)
    static uint8_t saveCounter = 0;
    saveCounter++;
    if (saveCounter >= 2)
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
    Serial.print("[HISTORY] Current time: ");
    Serial.print(currentTime);
    Serial.print(", Last save time: ");
    Serial.println(stack.history.lastSaveTime);
  }
  else
  {
    // Debug: Show why we're not recording
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime > 10000) // Debug every 10 seconds
    {
      Serial.print("[HISTORY DEBUG] Current time: ");
      Serial.print(currentTime);
      Serial.print(", Last save: ");
      Serial.print(stack.history.lastSaveTime);
      Serial.print(", Should record: ");
      Serial.println(stack.shouldRecordHistory(currentTime) ? "YES" : "NO");
      lastDebugTime = currentTime;
    }
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
