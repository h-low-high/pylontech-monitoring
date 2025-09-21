/***** PylontechMonitoring.ino (ESP8266 Wemos D1 mini)

 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

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

WebServer server(80);
batteryStack stack;

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
#if DBG_WIFI
  Serial.println("\n[WiFi] Inicializando STA…");
#endif
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
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
#if DBG_WIFI
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("[WiFi] ✅ Conectado");
    dumpNet();
  }
  else
  {
    Serial.println("[WiFi] ❌ No conecta");
  }
#endif
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
  setupOTA();

  setupWebInterface(server, &stack); // Web estilo original del repo
  server.begin();
  Serial.println("HTTP server listo");
}

void loop()
{
  server.handleClient();
  ArduinoOTA.handle();
}
