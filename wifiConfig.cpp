#include "wifiConfig.h"

WiFiConfigManager wifiConfig;

bool WiFiConfigManager::loadConfig()
{
    if (!LittleFS.begin())
    {
        Serial.println("[WiFiConfig] Failed to mount LittleFS");
        return false;
    }

    if (!LittleFS.exists("/wifi_config.dat"))
    {
        Serial.println("[WiFiConfig] No config file found");
        return false;
    }

    File file = LittleFS.open("/wifi_config.dat", "r");
    if (!file)
    {
        Serial.println("[WiFiConfig] Failed to open config file");
        return false;
    }

    size_t bytesRead = file.read((uint8_t *)&config, sizeof(WiFiConfig));
    file.close();

    if (bytesRead != sizeof(WiFiConfig) || !config.isValid)
    {
        Serial.println("[WiFiConfig] Invalid config file");
        return false;
    }

    Serial.println("[WiFiConfig] Configuration loaded successfully");
    return true;
}

bool WiFiConfigManager::saveConfig()
{
    if (!LittleFS.begin())
    {
        Serial.println("[WiFiConfig] Failed to mount LittleFS");
        return false;
    }

    config.isValid = true;

    File file = LittleFS.open("/wifi_config.dat", "w");
    if (!file)
    {
        Serial.println("[WiFiConfig] Failed to create config file");
        return false;
    }

    size_t bytesWritten = file.write((uint8_t *)&config, sizeof(WiFiConfig));
    file.close();

    if (bytesWritten != sizeof(WiFiConfig))
    {
        Serial.println("[WiFiConfig] Failed to write complete config");
        return false;
    }

    Serial.println("[WiFiConfig] Configuration saved successfully");
    return true;
}

bool WiFiConfigManager::connectToWiFi()
{
    if (!config.isValid || strlen(config.ssid) == 0)
    {
        Serial.println("[WiFiConfig] No valid WiFi configuration");
        return false;
    }

    Serial.print("[WiFiConfig] Connecting to: ");
    Serial.println(config.ssid);

    WiFi.mode(WIFI_STA);

    if (config.useStaticIP)
    {
        if (!WiFi.config(config.staticIP, config.gateway, config.subnet, config.dns))
        {
            Serial.println("[WiFiConfig] Failed to configure static IP");
        }
    }

    WiFi.begin(config.ssid, config.password);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("[WiFiConfig] ✅ WiFi connected successfully");
        Serial.print("[WiFiConfig] IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }
    else
    {
        Serial.println("[WiFiConfig] ❌ WiFi connection failed");
        return false;
    }
}

void WiFiConfigManager::startConfigPortal()
{
    Serial.println("[WiFiConfig] Starting configuration portal...");

    WiFi.mode(WIFI_AP_STA);

    // Create AP name with last 6 digits of MAC address
    String macAddress = WiFi.macAddress();
    macAddress.replace(":", "");
    String apName = "ESP_" + macAddress.substring(6); // Last 6 characters

    // Start Access Point
    WiFi.softAP(apName.c_str(), "1234");

    Serial.print("[WiFiConfig] AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("[WiFiConfig] AP Name: ");
    Serial.println(apName);

    // Start DNS server for captive portal
    dnsServer.start(53, "*", WiFi.softAPIP());

    // Setup web server
    setupConfigServer();
    configServer.begin();

    apMode = true;
    apStartTime = millis();

    Serial.println("[WiFiConfig] ✅ Configuration portal started");
    Serial.println("[WiFiConfig] Connect to 'ESP_XXXXXX' with password '1234'");
    Serial.println("[WiFiConfig] Then navigate to http://192.168.4.1");
}

void WiFiConfigManager::handleConfigPortal()
{
    if (!apMode)
        return;

    dnsServer.processNextRequest();
    configServer.handleClient();

    // Auto-close AP after timeout
    if (millis() - apStartTime > AP_TIMEOUT)
    {
        Serial.println("[WiFiConfig] ⏰ AP timeout, restarting...");
        ESP.restart();
    }
}

void WiFiConfigManager::stop()
{
    if (apMode)
    {
        configServer.stop();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        apMode = false;
        Serial.println("[WiFiConfig] Configuration portal stopped");
    }
}

void WiFiConfigManager::setupConfigServer()
{
    configServer.on("/", [this]()
                    { handleRoot(); });
    configServer.on("/save", HTTP_POST, [this]()
                    { handleSave(); });
    configServer.on("/scan", [this]()
                    { handleScan(); });

    // Captive portal - redirect all requests to root
    configServer.onNotFound([this]()
                            {
    configServer.sendHeader("Location", "http://192.168.4.1/", true);
    configServer.send(302, "text/plain", ""); });
}

void WiFiConfigManager::handleRoot()
{
    configServer.send(200, "text/html", generateConfigPage());
}

void WiFiConfigManager::handleScan()
{
    String json = "[";
    int n = WiFi.scanNetworks();

    for (int i = 0; i < n; i++)
    {
        if (i > 0)
            json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":" + String(WiFi.encryptionType(i));
        json += "}";
    }

    json += "]";
    configServer.send(200, "application/json", json);
}

void WiFiConfigManager::handleSave()
{
    // Get form data
    strncpy(config.ssid, configServer.arg("ssid").c_str(), sizeof(config.ssid) - 1);
    strncpy(config.password, configServer.arg("password").c_str(), sizeof(config.password) - 1);

    config.useStaticIP = configServer.arg("ipType") == "static";

    if (config.useStaticIP)
    {
        config.staticIP = parseIP(configServer.arg("ip"));
        config.gateway = parseIP(configServer.arg("gateway"));
        config.subnet = parseIP(configServer.arg("subnet"));
        config.dns = parseIP(configServer.arg("dns"));
    }

    if (saveConfig())
    {
        configServer.send(200, "text/html",
                          "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Guardado</title>"
                          "<style>body{font-family:Arial;text-align:center;padding:50px;background:#f0f8ff}</style></head>"
                          "<body><h2>✅ Configuración Guardada</h2>"
                          "<p>Reiniciando dispositivo para conectar a la nueva red...</p>"
                          "<script>setTimeout(function(){window.location.href='/';},3000);</script></body></html>");

        delay(2000);
        ESP.restart();
    }
    else
    {
        configServer.send(500, "text/html",
                          "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Error</title></head>"
                          "<body><h2>❌ Error al guardar</h2><a href='/'>Volver</a></body></html>");
    }
}

IPAddress WiFiConfigManager::parseIP(const String &ipStr)
{
    IPAddress ip;
    ip.fromString(ipStr);
    return ip;
}

String WiFiConfigManager::generateConfigPage()
{
    String html = R"(
<!DOCTYPE html>
<html lang='es'>
<head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <title>Configuración WiFi - PylontechMonitoring</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f8ff; }
        .container { max-width: 500px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
        .header { text-align: center; margin-bottom: 30px; color: #2c3e50; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; color: #34495e; }
        input, select { width: 100%; padding: 10px; border: 1px solid #bdc3c7; border-radius: 5px; font-size: 14px; box-sizing: border-box; }
        button { width: 100%; padding: 12px; background: #3498db; color: white; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; margin-top: 10px; }
        button:hover { background: #2980b9; }
        .scan-btn { background: #27ae60; margin-bottom: 10px; }
        .scan-btn:hover { background: #229954; }
        .networks { max-height: 150px; overflow-y: auto; border: 1px solid #bdc3c7; border-radius: 5px; margin-bottom: 10px; }
        .network-item { padding: 10px; border-bottom: 1px solid #ecf0f1; cursor: pointer; display: flex; justify-content: space-between; }
        .network-item:hover { background: #ecf0f1; }
        .network-item:last-child { border-bottom: none; }
        .signal { font-size: 12px; color: #7f8c8d; }
        .ip-config { display: none; }
        .ip-config.show { display: block; }
        .note { font-size: 12px; color: #7f8c8d; margin-top: 5px; }
    </style>
</head>
<body>
    <div class='container'>
        <div class='header'>
            <h1>🔧 Configuración WiFi</h1>
            <p>Configure su conexión a internet</p>
        </div>
        
        <form method='POST' action='/save'>
            <div class='form-group'>
                <button type='button' class='scan-btn' onclick='scanNetworks()'>🔍 Buscar Redes</button>
                <div id='networks' class='networks' style='display:none'></div>
            </div>
            
            <div class='form-group'>
                <label>Nombre de Red (SSID):</label>
                <input type='text' name='ssid' id='ssid' required placeholder='Nombre de su red WiFi'>
            </div>
            
            <div class='form-group'>
                <label>Contraseña:</label>
                <input type='password' name='password' placeholder='Contraseña de la red'>
                <div class='note'>Deje en blanco para redes abiertas</div>
            </div>
            
            <div class='form-group'>
                <label>Configuración IP:</label>
                <select name='ipType' onchange='toggleIPConfig(this.value)'>
                    <option value='dhcp'>DHCP (Automático)</option>
                    <option value='static'>IP Estática</option>
                </select>
            </div>
            
            <div id='ipConfig' class='ip-config'>
                <div class='form-group'>
                    <label>Dirección IP:</label>
                    <input type='text' name='ip' placeholder='192.168.1.100'>
                </div>
                <div class='form-group'>
                    <label>Puerta de enlace:</label>
                    <input type='text' name='gateway' placeholder='192.168.1.1'>
                </div>
                <div class='form-group'>
                    <label>Máscara de subred:</label>
                    <input type='text' name='subnet' value='255.255.255.0'>
                </div>
                <div class='form-group'>
                    <label>DNS:</label>
                    <input type='text' name='dns' placeholder='8.8.8.8'>
                </div>
            </div>
            
            <button type='submit'>💾 Guardar y Conectar</button>
        </form>
        
        <div style='text-align: center; margin-top: 20px; color: #7f8c8d; font-size: 12px;'>
            <p>El dispositivo se reiniciará después de guardar</p>
        </div>
    </div>

    <script>
        function toggleIPConfig(value) {
            const ipConfig = document.getElementById('ipConfig');
            if (value === 'static') {
                ipConfig.classList.add('show');
            } else {
                ipConfig.classList.remove('show');
            }
        }

        function scanNetworks() {
            const networksDiv = document.getElementById('networks');
            networksDiv.innerHTML = '<div style="padding:10px;text-align:center">🔄 Buscando redes...</div>';
            networksDiv.style.display = 'block';
            
            fetch('/scan')
                .then(response => response.json())
                .then(networks => {
                    networksDiv.innerHTML = '';
                    
                    if (networks.length === 0) {
                        networksDiv.innerHTML = '<div style="padding:10px;text-align:center">❌ No se encontraron redes</div>';
                        return;
                    }
                    
                    networks.forEach(network => {
                        const item = document.createElement('div');
                        item.className = 'network-item';
                        item.onclick = () => selectNetwork(network.ssid);
                        
                        const lockIcon = network.encryption === 7 ? '🔓' : '🔒';
                        const signalBars = getSignalBars(network.rssi);
                        
                        item.innerHTML = `
                            <span>${lockIcon} ${network.ssid}</span>
                            <span class='signal'>${signalBars} ${network.rssi}dBm</span>
                        `;
                        
                        networksDiv.appendChild(item);
                    });
                })
                .catch(error => {
                    networksDiv.innerHTML = '<div style="padding:10px;text-align:center">❌ Error al buscar redes</div>';
                });
        }

        function selectNetwork(ssid) {
            document.getElementById('ssid').value = ssid;
            document.getElementById('networks').style.display = 'none';
        }

        function getSignalBars(rssi) {
            if (rssi > -50) return '📶';
            if (rssi > -60) return '📶';
            if (rssi > -70) return '📶';
            if (rssi > -80) return '📶';
            return '📶';
        }
    </script>
</body>
</html>
)";

    return html;
}