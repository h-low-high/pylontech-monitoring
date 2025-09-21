# Monitor Pylontech ESP32 - Versión Mejorada

**Basado en el proyecto original:** https://github.com/hidaba/PylontechMonitoring

Sistema completo de monitoreo para baterías Pylontech usando ESP32/ESP8266 con interfaz web avanzada y análisis de salud en tiempo real.






## Instalación y Configuración

### Requisitos:
- ESP32 o ESP8266
- MAX3232
- Arduino IDE o PlatformIO
- Conexión serial a sistema Pylontech
- Red WiFi

### Conexionado Físico

#### ESP32 Mini + MAX3232 + RJ45

**Conexiones ESP32 Mini → MAX3232:**
- Mini TX → T1IN del MAX3232
- Mini RX → R1OUT del MAX3232  
- Mini GND → GND del MAX3232
- Mini 3.3V → VCC del MAX3232

**Conexiones MAX3232 → RJ45 (Topología B):**
- R1IN del MAX3232 → Pin 3 (Blanco/Verde) - RX desde Pylontech
- T1OUT del MAX3232 → Pin 6 (Verde) - TX hacia Pylontech
- GND del MAX3232 → Pin 8 (Marrón) - Tierra común

#### Pinout RJ45 - Vista frontal
```
┌─────────────────────────────┐
│ 1 2 3 4 5 6 7 8            │
└─┴─┴─┴─┴─┴─┴─┴─┴───────────┘
      │     │   │
      │     │     └─ GND (Marrón)
      │     └─ TX (Verde)  
      └─ RX (Blanco/Verde)
```


## Configuración de Variables

Antes de compilar y usar el sistema, debe configurar las siguientes variables en el código:

### Variables Obligatorias

| Variable | Archivo | Descripción | Ejemplo |
|----------|---------|-------------|---------|
| `WIFI_SSID` | PylontechMonitoring.h | Nombre de la red WiFi | `"MiRedWiFi"` |
| `WIFI_PASS` | PylontechMonitoring.h | Contraseña de la red WiFi | `"MiContraseña123"` |
| `WIFI_HOSTNAME` | PylontechMonitoring.h | Nombre del dispositivo en la red | `"PylontechESP32"` |

### Variables de Configuración de Red

#### IP Estática (Opcional)
Para usar IP fija, descomente `#define STATIC_IP` en PylontechMonitoring.h y configure:

| Variable | Archivo | Descripción | Ejemplo |
|----------|---------|-------------|---------|
| `STATIC_IP` | PylontechMonitoring.h | Activar IP estática (descomente la línea) | `#define STATIC_IP` |
| `ip` | PylontechMonitoring.h | Dirección IP fija del ESP32 | `IPAddress ip(192, 168, 1, 100);` |
| `gateway` | PylontechMonitoring.h | Gateway de la red | `IPAddress gateway(192, 168, 1, 1);` |
| `subnet` | PylontechMonitoring.h | Máscara de subred | `IPAddress subnet(255, 255, 255, 0);` |
| `dns` | PylontechMonitoring.h | Servidor DNS | `IPAddress dns(192, 168, 1, 1);` |

**Para usar DHCP (IP automática):** Comente la línea `#define STATIC_IP` agregando `//` al inicio:
```cpp
// #define STATIC_IP
```

### Variables de Seguridad

#### Autenticación Web (Opcional)
Para proteger la interfaz web con usuario y contraseña:

| Variable | Archivo | Descripción | Ejemplo |
|----------|---------|-------------|---------|
| `AUTHENTICATION` | PylontechMonitoring.h | Activar autenticación (descomente) | `#define AUTHENTICATION` |
| `www_username` | PylontechMonitoring.h | Usuario para acceso web | `"admin"` |
| `www_password` | PylontechMonitoring.h | Contraseña para acceso web | `"mipassword123"` |

#### Contraseña OTA
| Variable | Archivo | Descripción | Ejemplo |
|----------|---------|-------------|---------|
| `ArduinoOTA.setPassword()` | PylontechMonitor_ESP32E_Display.ino (línea ~90) | Contraseña para actualizaciones remotas | `"ota123"` |

### Variables MQTT (Opcional)

| Variable | Archivo | Descripción | Ejemplo |
|----------|---------|-------------|---------|
| `DISABLE_MQTT` | PylontechMonitoring.h | Desactivar MQTT (descomente) | `#define DISABLE_MQTT` |
| `MQTT_SERVER` | PylontechMonitoring.h | Dirección IP del servidor MQTT | `"192.168.1.100"` |
| `MQTT_PORT` | PylontechMonitoring.h | Puerto del servidor MQTT | `1883` |
| `MQTT_USER` | PylontechMonitoring.h | Usuario MQTT | `"mqttuser"` |
| `MQTT_PASSWORD` | PylontechMonitoring.h | Contraseña MQTT | `"mqttpass"` |
| `MQTT_TOPIC_ROOT` | PylontechMonitoring.h | Raíz de topics MQTT | `"pylontech/sensor/"` |
| `MQTT_PUSH_FREQ_SEC` | PylontechMonitoring.h | Frecuencia envío datos (segundos) | `10` |

### Variables del Sistema

| Variable | Archivo | Descripción | Ejemplo |
|----------|---------|-------------|---------|
| `GMT` | PylontechMonitoring.h | Zona horaria en segundos desde UTC | `7200` (UTC+2) |
| `MAX_PYLON_BATTERIES` | PylontechMonitoring.h | Número máximo de baterías a detectar | `6` |

## Ejemplos de Configuración

### Configuración Básica (Solo WiFi)
```cpp
// En PylontechMonitoring.h
#define WIFI_SSID "MiRedWiFi"
#define WIFI_PASS "MiContraseña123"
#define WIFI_HOSTNAME "PylontechESP32"

// Comentar para usar DHCP
// #define STATIC_IP

// Comentar para desactivar autenticación
// #define AUTHENTICATION

// Comentar para desactivar MQTT
// #define DISABLE_MQTT
```

### Configuración con IP Estática
```cpp
// En PylontechMonitoring.h
#define STATIC_IP
IPAddress ip(192, 168, 1, 100);        // IP deseada para el ESP32
IPAddress gateway(192, 168, 1, 1);     // IP del router
IPAddress subnet(255, 255, 255, 0);    // Máscara de red estándar
IPAddress dns(192, 168, 1, 1);         // DNS (normalmente el router)
```

### Configuración con Autenticación Web
```cpp
// En PylontechMonitoring.h
#define AUTHENTICATION
const char* www_username = "admin";
const char* www_password = "mipassword123";  // ¡Cambiar por una contraseña segura!
```

### Configuración MQTT Completa
```cpp
// En PylontechMonitoring.h
#define MQTT_SERVER        "192.168.1.50"      // IP del broker MQTT
#define MQTT_PORT          1883
#define MQTT_USER          "homeassistant"
#define MQTT_PASSWORD      "mqtt_password"
#define MQTT_TOPIC_ROOT    "energia/baterias/"
#define MQTT_PUSH_FREQ_SEC 30                  // Enviar cada 30 segundos
```

### Configuración de Contraseña OTA
```cpp
// En PylontechMonitor_ESP32E_Display.ino (línea ~90)
ArduinoOTA.setPassword("mi_ota_password_seguro");  // ¡Cambiar por contraseña segura!
```

## Características Principales

### Dashboard Web Responsive
- **Dashboard Principal:** Vista general del sistema completo
- **Selector de Módulos:** Botones dinámicos para monitorear baterías individuales
- **Terminal BMS:** Output en tiempo real con colores y scroll horizontal
- **Diseño Mobile-First:** Optimizado para smartphones y tablets

### Monitoreo Multi-Batería
- **Detección Automática:** Identifica automáticamente qué módulos están conectados
- **Vista Sistema:** Datos consolidados de todas las baterías
- **Vista Individual:** Monitoreo específico por módulo
- **Cambio Dinámico:** Botones para alternar entre diferentes baterías

### Terminal BMS Avanzado
- **Colorización por Datos:**
  - Verde: Voltajes y estados normales
  - Naranja: Corrientes y potencia
  - Rojo: Temperaturas
  - Púrpura: Estado de carga (SOC)
  - Azul: Números y valores
  - Gris: Estados inactivos/ausentes

- **Scroll Horizontal:** Para dispositivos móviles con hint visual
- **Formato Tabular:** Datos organizados y fáciles de leer
- **Actualización Automática:** Cada 3 segundos

### Análisis de Salud de Baterías
Sistema proactivo de monitoreo específico para baterías LiFePO4:

#### Estados de Balance de Celdas:
- **Normal:** ≤40mV de diferencia entre celdas
- **Advertencia:** 40-60mV de diferencia 
- **Crítico:** >60mV de diferencia

#### Información Detallada:
- Número total de celdas monitoreadas
- Voltaje máximo y mínimo entre celdas
- Diferencia exacta en milivolts
- Mensaje descriptivo del estado
- Tooltip informativo con detalles específicos de celda

### Diseño Responsive
- **Desktop:** Layout de 4 columnas con tarjetas grandes
- **Tablet:** Layout adaptativo de 2 columnas
- **Mobile:** Layout de 1 columna con elementos optimizados
- **Terminal Móvil:** Scroll horizontal con hints visuales

#### Capturas de Pantalla:
 ![alt text](web-1.png) ![alt text](command-1.png) ![alt text](inform-1.png)



### Pasos de Instalación:
1. Clonar o descargar este proyecto
2. Abrir el archivo `.ino` en Arduino IDE
3. Configurar la placa ESP32/ESP8266 en Tools > Board
4. Instalar las librerías necesarias si no están presentes
5. Configurar las variables de la tabla anterior según su entorno
6. Conectar físicamente el ESP32 al sistema Pylontech vía UART
7. Compilar y subir el código
8. Conectar a la red WiFi configurada
9. Acceder a la IP asignada al ESP32 desde un navegador

## Comandos BMS Soportados

### Comando `bat`
- **Propósito:** Información detallada de todas las celdas
- **Uso:** Vista sistema y análisis de balance
- **Datos:** Voltajes individuales, temperaturas, corrientes

### Comando `pwrsys`
- **Propósito:** Estado general del sistema
- **Uso:** Vista consolidada
- **Datos:** SOC, voltaje total, corriente del pack

### Comando `pwr`
- **Propósito:** Estado específico por módulo
- **Uso:** Vista individual de baterías
- **Datos:** Estado operacional, temperaturas

## Uso de la Interfaz

### Botones de Navegación:
- **"Sistema":** Vista consolidada de todas las baterías
- **"Batería X":** Vista individual del módulo específico
- **Detección Automática:** Solo aparecen los módulos conectados

### Tarjetas de Información:
1. **Estado de Carga (SOC):** Porcentaje con barra visual
2. **Voltaje:** Voltaje total del pack/módulo
3. **Corriente:** Corriente actual (carga/descarga)
4. **Potencia:** Potencia instantánea calculada
5. **Temperatura:** Promedio del sistema o módulo
6. **Salud Celdas:** Estado de balance con tooltip informativo

### Terminal BMS:
- **Datos en Tiempo Real:** Actualización cada 3 segundos
- **Scroll Horizontal:** En móviles, deslizar para ver todos los datos
- **Colores Informativos:** Cada tipo de dato tiene su color
- **Formato Estructurado:** Headers y separadores visuales

### Tooltip de Información:
- **Activación:** Pasar el ratón sobre el icono "i" en Salud Celdas
- **Contenido:** Detalles específicos de voltajes por celda
- **Información:** Celda con mayor/menor voltaje y diferencias exactas

## Características Técnicas

### Comunicación Serial:
- **Protocolo:** ASCII comandos a 9600 baud
- **Timeout:** 5 segundos por comando
- **Buffer:** Optimizado para respuestas largas

### Servidor Web:
- **Puerto:** 80 (HTTP)
- **Endpoints:**
  - `/` - Interfaz principal
  - `/battery-data` - Datos JSON del sistema
  - `/battery-data?module=X` - Datos JSON módulo específico
  - `/modules` - Lista de módulos detectados

### Gestión de Memoria:
- **String Buffers:** Optimizados para ESP32/ESP8266
- **Cache:** Sin cache para datos en tiempo real
- **CORS:** Configurado para desarrollo

## Personalización

### Umbrales de Salud:
Modificar en `webInterface.h` líneas ~545-560:
```cpp
// Umbrales para baterías LiFePO4
if (imbalanceMv <= 40) {
  balanceStatus = "Normal";
} else if (imbalanceMv <= 60) {
  balanceStatus = "Advertencia"; 
} else {
  balanceStatus = "Crítico";
}
```

### Colores del Terminal:
Modificar en CSS líneas ~153-161:
```css
.terminal .voltage{color:#10b981}    /* Verde */
.terminal .current{color:#f59e0b}    /* Naranja */
.terminal .temperature{color:#ef4444} /* Rojo */
```

### Intervalos de Actualización:
Modificar en JavaScript:
```javascript
setInterval(pull, 3000); // 3 segundos
```

## Soporte y Troubleshooting

### Problemas Comunes:

1. **"Corriente 0.000 A":**
   - Solucionado: Parsing mejorado del comando `pwrsys`

2. **Terminal no responsive:**
   - Solucionado: Scroll horizontal automático en móviles

3. **Datos de temperatura inconsistentes:**
   - Solucionado: Promedio calculado desde comando `bat`

4. **No se detectan módulos:**
   - Verificar conexiones seriales
   - Comprobar que las baterías respondan al comando `pwr`

5. **No se conecta a WiFi:**
   - Verificar SSID y contraseña en la configuración
   - Comprobar que la red esté disponible
   - Revisar el monitor serie para mensajes de conexión

6. **Página web no carga:**
   - Verificar que el ESP32 esté conectado a WiFi
   - Comprobar la IP asignada en el monitor serie
   - Intentar acceso desde la misma red

### Debug:
- Usar monitor serie de Arduino IDE
- Verificar respuestas de comandos BMS
- Comprobar conectividad WiFi
- Revisar logs de conexión en monitor serie

## Mejoras Implementadas

- Interfaz multi-batería responsive
- Terminal colorizado y scrolleable
- Análisis de salud proactivo
- Optimización móvil completa
- Tooltip informativo con detalles de celdas
- Balance de celdas por módulo individual
- Detección automática de módulos
- Colores específicos por tipo de dato

## Versión

**v2.0 - Monitor Avanzado**
Compatible con sistemas Pylontech y baterías LiFePO4 usando protocolo de comunicación estándar.

---

**Proyecto basado en:** https://github.com/hidaba/PylontechMonitoring  
**Desarrollado para sistemas Pylontech con ESP32/ESP8266**