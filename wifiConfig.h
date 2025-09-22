#ifndef WIFICONFIG_H
#define WIFICONFIG_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>

// WiFi configuration structure
struct WiFiConfig
{
    char ssid[32];
    char password[64];
    bool useStaticIP;
    IPAddress staticIP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns;
    bool isValid;

    WiFiConfig()
    {
        memset(ssid, 0, sizeof(ssid));
        memset(password, 0, sizeof(password));
        useStaticIP = false;
        staticIP = IPAddress(0, 0, 0, 0);
        gateway = IPAddress(0, 0, 0, 0);
        subnet = IPAddress(255, 255, 255, 0);
        dns = IPAddress(0, 0, 0, 0);
        isValid = false;
    }
};

class WiFiConfigManager
{
private:
    WiFiConfig config;
    ESP8266WebServer configServer;
    DNSServer dnsServer;
    bool apMode;
    unsigned long apStartTime;
    static const unsigned long AP_TIMEOUT = 300000; // 5 minutes

public:
    WiFiConfigManager() : configServer(80), apMode(false), apStartTime(0) {}

    bool loadConfig();
    bool saveConfig();
    bool connectToWiFi();
    void startConfigPortal();
    void handleConfigPortal();
    bool isInAPMode() { return apMode; }
    void stop();

private:
    void setupConfigServer();
    void handleRoot();
    void handleSave();
    void handleScan();
    String generateConfigPage();
    IPAddress parseIP(const String &ipStr);
};

// Global instance
extern WiFiConfigManager wifiConfig;

#endif