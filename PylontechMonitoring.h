#ifndef PYLONTECHMONITORING_H
#define PYLONTECHMONITORING_H

// #define DISABLE_MQTT

#define WIFI_SSID "wlan"
#define WIFI_PASS "pass"
#define WIFI_HOSTNAME "PylontechESP32"

#define STATIC_IP
IPAddress ip(192, 168, 9, 20);
IPAddress gateway(192, 168, 9, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 9, 1);

// #define AUTHENTICATION
const char *www_username = "admin";
const char *www_password = "admin";

#define MQTT_SERVER "192.168.1.100"
#define MQTT_PORT 1883
#define MQTT_USER "mqttuser"
#define MQTT_PASSWORD "mqttpass"
#define MQTT_TOPIC_ROOT "pylontech/sensor/"
#define MQTT_PUSH_FREQ_SEC 10

#define GMT 7200
#define MAX_PYLON_BATTERIES 6

#endif