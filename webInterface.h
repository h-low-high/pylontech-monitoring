#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#ifdef ESP8266
#include <ESP8266WebServer.h>
#define WebServer ESP8266WebServer
#else
#include <WebServer.h>
#endif

#include "batteryStack.h"
#include "PylontechMonitoring.h"

#ifndef DBG_WEB
#define DBG_WEB 0
#endif
#ifndef DBG_BMS
#define DBG_BMS 0
#endif

// En tu .ino:  #define Serial2 Serial
extern HardwareSerial Serial2;

// ================== Consola BMS ==================
static inline bool _isBmsPrompt(const String &s)
{
  return s.endsWith("pylon>") || s.endsWith("pylon_debug>");
}

static bool _bmsWaitPrompt(uint32_t timeout_ms, String *rawOut = nullptr)
{
  unsigned long t0 = millis();
  String acc;
  while (millis() - t0 < timeout_ms)
  {
    while (Serial2.available())
    {
      char c = (char)Serial2.read();
      acc += c;
      if (_isBmsPrompt(acc))
      {
        if (rawOut)
          *rawOut = acc;
        return true;
      }
    }
    delay(2);
    yield();
  }
  if (rawOut)
    *rawOut = acc;
  return false;
}

// Cambia a true si tu firmware exige CRLF
#define USE_CRLF false

String _bmsSendCmd(const String &cmd, uint32_t timeout_ms = 3000)
{
  // “Despierta” y limpia
  while (Serial2.available())
    Serial2.read();
  Serial2.print("\r");
  _bmsWaitPrompt(700);
  while (Serial2.available())
    Serial2.read();

  // Envía
#if USE_CRLF
  Serial2.print(cmd);
  Serial2.print("\r\n");
#else
  Serial2.print(cmd);
  Serial2.print("\r");
#endif

  // Lee hasta prompt / timeout
  String out;
  unsigned long t0 = millis();
  while (millis() - t0 < timeout_ms)
  {
    while (Serial2.available())
    {
      char c = (char)Serial2.read();
      out += c;
      if (_isBmsPrompt(out))
        return out;
    }
    delay(2);
    yield();
  }
  return out;
}

// ================== Estado UI ==================
static String lastCommandOutput;

// -------- helper: extraer 4 enteros en orden (tolerante a espacios) ----
static bool scan4ints(const String &line, long &a, long &b, long &c, long &d)
{
  const char *s = line.c_str();
  long vals[4];
  int n = 0;
  while (*s && n < 4)
  {
    while (*s && !((*s == '-') || (*s >= '0' && *s <= '9')))
      s++; // salta no-num
    if (!*s)
      break;
    char *end;
    long v = strtol(s, &end, 10);
    vals[n++] = v;
    s = end;
  }
  if (n == 4)
  {
    a = vals[0];
    b = vals[1];
    c = vals[2];
    d = vals[3];
    return true;
  }
  return false;
}

// ================== Interfaz Web ==================
void setupWebInterface(WebServer &server, batteryStack *batteryData)
{

  // ---------- UI principal (visual) ----------
  server.on("/", [&server]()
            {
    String html;
    html.reserve(14*1024);
    html  = F("<!DOCTYPE html><html lang='es'><head><meta charset='utf-8'>");
    html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    html += F("<title>Pylontech Monitoring </title>");
    html += F("<style>"
      "body{margin:0;font-family:Inter,system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Helvetica,Arial,sans-serif;background:#f5f7fb;color:#0f172a}"
      "nav{background:#0ea5e9;color:#fff;padding:14px 16px;display:flex;justify-content:space-between;align-items:center}"
      "nav .brand{font-weight:800;letter-spacing:.2px}"
      "main{max-width:1024px;margin:18px auto;padding:0 16px}"
      ".grid{display:grid;gap:16px;grid-template-columns:repeat(auto-fit,minmax(260px,1fr))}"
      ".card{background:#fff;border:1px solid #e2e8f0;border-radius:14px;box-shadow:0 6px 18px rgba(2,6,23,.06);padding:16px}"
      ".title{font-size:15px;font-weight:700;color:#334155;margin:0 0 8px}"
      ".big{font-size:28px;font-weight:800;margin:6px 0 2px}"
      ".muted{color:#64748b;font-size:13px}"
      ".socwrap{height:16px;background:#e2e8f0;border-radius:10px;overflow:hidden}"
      ".socbar{height:100%;width:0;background:linear-gradient(90deg,#22c55e,#10b981);transition:width .6s ease}"
      ".terminal{background:#0b1220;color:#e2e8f0;border-radius:12px;padding:12px;font-family:ui-monospace,Consolas,Monaco,monospace;white-space:pre;min-height:120px;overflow:auto;overflow-x:auto;max-width:100%}"
      ".terminal .header{color:#60a5fa;font-weight:bold;border-bottom:1px solid #1e293b;padding-bottom:4px;margin-bottom:8px;white-space:pre}"
      ".terminal .data-row{white-space:pre}"
      ".terminal .voltage{color:#10b981}"
      ".terminal .current{color:#f59e0b}"
      ".terminal .temperature{color:#ef4444}"
      ".terminal .soc{color:#8b5cf6}"
      ".terminal .state-normal{color:#10b981}"
      ".terminal .state-idle{color:#6b7280}"
      ".terminal .state-absent{color:#6b7280;opacity:0.6}"
      ".terminal .number{color:#06b6d4}"
      ".terminal .separator{color:#475569}"
      ".health-normal{color:#10b981}"
      ".health-warning{color:#f59e0b}"
      ".health-critical{color:#ef4444}"
      "#healthCard.normal{border-left:4px solid #10b981}"
      "#healthCard.warning{border-left:4px solid #f59e0b}"
      "#healthCard.critical{border-left:4px solid #ef4444}"
      ".info-icon{display:inline-block;margin-left:5px;cursor:help;color:#6b7280;font-size:14px;position:relative;user-select:none}"
      ".info-icon:hover{color:#3b82f6}"
      ".tooltip{display:none;position:absolute;top:25px;left:50%;transform:translateX(-50%);background:#1f2937;color:#e5e7eb;padding:8px 12px;border-radius:6px;font-size:12px;white-space:nowrap;z-index:1000;box-shadow:0 4px 6px rgba(0,0,0,0.3);max-width:280px;white-space:normal;text-align:left;line-height:1.3;pointer-events:none}"
      ".tooltip::before{content:'';position:absolute;top:-5px;left:50%;transform:translateX(-50%);width:0;height:0;border-left:5px solid transparent;border-right:5px solid transparent;border-bottom:5px solid #1f2937}"
      ".info-icon:hover .tooltip{display:block}"
      "@media (max-width: 768px){"
        ".terminal{font-size:11px;padding:8px;line-height:1.2}"
        "main{padding:0 8px;margin:12px auto}"
        ".card{padding:12px}"
        ".row{gap:8px}"
        ".grid{grid-template-columns:1fr}"
        "button.quick-cmd{padding:4px 8px;font-size:11px;margin:1px}"
        "input[type=text]{min-width:150px;padding:8px 10px;font-size:14px}"
        ".tooltip{max-width:250px;left:-50px;transform:none;font-size:11px}"
        "button{padding:8px 12px;font-size:13px}"
      "}"
      ".row{display:flex;gap:12px;flex-wrap:wrap;align-items:center}"
      "input[type=text]{flex:1;min-width:220px;padding:10px 12px;border:1px solid #cbd5e1;border-radius:10px;font-size:14px}"
      "button{padding:10px 14px;border:0;border-radius:10px;font-weight:700;cursor:pointer;background:#0ea5e9;color:#fff}"
      "button.secondary{background:#f1f5f9;color:#0f172a}"
      "button.quick-cmd{padding:6px 12px;margin:2px;background:#4f46e5;color:#fff;font-size:13px;font-weight:600}"
      "button.quick-cmd:hover{background:#6366f1}"
      "button.module{padding:8px 12px;margin:4px;border-radius:8px;font-size:13px;font-weight:600}"
      "button.module.active{background:#10b981;color:#fff}"
      "button.module.inactive{background:#e2e8f0;color:#64748b;cursor:not-allowed}"
      ".module-selector{margin-bottom:16px;padding:16px;background:#fff;border:1px solid #e2e8f0;border-radius:12px}"
      ".module-title{font-weight:700;margin-bottom:12px;color:#334155}"
      "a.link{color:#0ea5e9;text-decoration:none;font-weight:700}"
      ".foot{margin:22px 0;color:#94a3b8;font-size:12px}"
      "#hidden{display:none}"
    "</style></head><body>");

    html += F("<nav><div class='brand'>Pylontech Monitoring</div>"
              "<div><a class='link' href='/battery-data'>/battery-data</a></div></nav>");
    
    html += F("<main>");
    
    // Selector de módulos de batería
    html += F("<div class='module-selector'>"
              "<div class='module-title'>Seleccionar Batería:</div>"
              "<div id='moduleButtons'>"
              "<button class='module active' onclick='selectModule(\"system\")'>Sistema</button>"
              "<button class='module' onclick='selectModule(1)'>Bat 1</button>"
              "<button class='module inactive' onclick='selectModule(2)'>Bat 2</button>"
              "<button class='module inactive' onclick='selectModule(3)'>Bat 3</button>"
              "<button class='module inactive' onclick='selectModule(4)'>Bat 4</button>"
              "</div></div>");
    
    html += F("<div class='grid'>");

    // Título del módulo actual
    html += F("<div class='card' style='grid-column:1/-1;text-align:center;background:#f8fafc'>"
              "<div class='title' id='moduleTitle'>Datos del Sistema General</div></div>");

    html += F("<div class='card'><div class='title'>Estado de carga</div>"
              "<div class='big'><span id='socv'>--</span>%</div>"
              "<div class='socwrap'><div id='socbar' class='socbar'></div></div>"
              "<div class='muted' style='margin-top:8px'>Actualizado cada 3 s</div></div>");

    html += F("<div class='card'><div class='title'>Voltaje</div>"
              "<div class='big'><span id='volt'>--</span> V</div>"
              "<div class='muted' id='voltDesc'>Sistema total</div></div>");

    html += F("<div class='card'><div class='title'>Corriente</div>"
              "<div class='big'><span id='curr'>--</span> A</div>"
              "<div class='muted' id='currDesc'>Sistema total</div></div>");

    html += F("<div class='card'><div class='title'>Potencia DC</div>"
              "<div class='big'><span id='powr'>--</span> W</div>"
              "<div class='muted'>V × I</div></div>");

    html += F("<div class='card'><div class='title'>Temperatura</div>"
              "<div class='big'><span id='temp'>--</span> °C</div>"
              "<div class='muted' id='tempDesc'>Promedio sistema</div></div>");

    html += F("<div class='card' id='healthCard'><div class='title'>Salud Celdas "
              "<span class='info-icon' id='healthInfo'>ⓘ"
              "<div class='tooltip' id='healthTooltip'>Cargando información...</div>"
              "</span>"
              "</div>"
              "<div class='big'><span id='balanceStatus'>--</span></div>"
              "<div class='muted' id='balanceDetails'>Analizando...</div>"
              "<div style='font-size:10px;color:#6b7280;margin-top:8px;line-height:1.2'>"
              "Normal: ≤40mV | Advertencia: 40-60mV | Crítico: >60mV"
              "</div></div>");

    html += F("</div>"); // grid

    // Balance History Section
    html += F("<div class='card' style='margin-top:18px'>"
              "<div class='title'>Histórico de Balance Diario "
              "<button class='secondary' style='float:right;padding:4px 8px;font-size:12px' onclick='toggleHistory()'>Mostrar/Ocultar</button>"
              "</div>"
              "<div id='historySection' style='display:none'>"
              "<div class='row' style='margin-bottom:10px'>"
              "<select id='batteryFilter' style='padding:6px;margin-right:10px'>"
              "<option value='all'>Todas las baterías</option>"
              "</select>"
              "<button onclick='refreshHistory()'>Actualizar</button>"
              "<button class='secondary' onclick='exportHistory()'>Exportar CSV</button>"
              "</div>"
              "<div style='max-height:300px;overflow-y:auto'>"
              "<table id='historyTable' style='width:100%;border-collapse:collapse;font-size:12px'>"
              "<thead style='background:#f8fafc;position:sticky;top:0'>"
              "<tr><th style='padding:8px;border:1px solid #e2e8f0'>Hora</th>"
              "<th style='padding:8px;border:1px solid #e2e8f0'>Batería</th>"
              "<th style='padding:8px;border:1px solid #e2e8f0'>Balance (mV)</th>"
              "<th style='padding:8px;border:1px solid #e2e8f0'>SOC (%)</th>"
              "<th style='padding:8px;border:1px solid #e2e8f0'>Estado</th></tr>"
              "</thead>"
              "<tbody id='historyBody'>"
              "<tr><td colspan='5' style='text-align:center;padding:20px;color:#6b7280'>Cargando histórico...</td></tr>"
              "</tbody>"
              "</table>"
              "</div>"
              "<div style='margin-top:10px;font-size:11px;color:#6b7280'>"
              "Registros cada 15 minutos • Máximo 96 entradas (24 horas)"
              "</div>"
              "</div></div>");

    // Terminal
    html += F("<div class='card' style='margin-top:18px'>"
              "<div class='title'>Terminal BMS <span id='scroll-hint' style='font-size:11px;color:#6b7280;display:none'>← Desliza horizontalmente →</span></div>"
              "<div class='row' style='margin-bottom:10px'>"
              "<button class='quick-cmd' onclick='quickCmd(\"pwr\")'>pwr</button>"
              "<button class='quick-cmd' onclick='quickCmd(\"pwrsys\")'>pwrsys</button>"
              "<button class='quick-cmd' id='batCmd' onclick='quickCmdBat()'>bat</button>"
              "<button class='quick-cmd' onclick='quickCmd(\"help\")'>help</button>"
              "</div>"
              "<div class='row'>"
              "<input id='cmd' type='text' placeholder='Ej: help, bat, pwrsys, pwr'>"
              "<button onclick='sendCmd()'>Enviar</button>"
              "<button class='secondary' onclick='clearConsole()'>Limpiar</button>"
              "</div>"
              "<div id='console' class='terminal'></div></div>");

    // Bloque oculto con la última respuesta para que el JS la recoja
    html += F("<div id='hidden'><h3>Respuesta</h3><pre>");
    html += lastCommandOutput;
    html += F("</pre></div>");

    html += F("<div class='foot'>UI inspirada en la del repositorio original.</div>");

    // JS
    html += F("<script>"
      "let currentModule='system';"
      "function fmt(n,dec){if(isNaN(n))return'--';return Number(n).toFixed(dec)}"
      "function selectModule(module){"
        "currentModule=module;"
        "document.querySelectorAll('.module').forEach(b=>b.classList.remove('active'));"
        "event.target.classList.add('active');"
        "updateBatButton();"
        "if(module==='system'){"
          "document.getElementById('moduleTitle').textContent='Datos del Sistema General';"
          "document.getElementById('voltDesc').textContent='Sistema total';"
          "document.getElementById('currDesc').textContent='Sistema total';"
          "document.getElementById('tempDesc').textContent='Promedio sistema';"
        "}else{"
          "document.getElementById('moduleTitle').textContent='Datos de Batería '+module;"
          "document.getElementById('voltDesc').textContent='Batería individual';"
          "document.getElementById('currDesc').textContent='Batería individual';"
          "document.getElementById('tempDesc').textContent='Batería individual';"
        "}"
        "pull();"
      "}"
      "function paint(d){"
        "document.getElementById('socv').textContent=d.soc;"
        "document.getElementById('socbar').style.width=d.soc+'%';"
        "document.getElementById('volt').textContent=fmt(d.voltage,3);"
        "document.getElementById('curr').textContent=fmt(d.current,3);"
        "document.getElementById('powr').textContent=fmt(d.power,1);"
        "document.getElementById('temp').textContent=fmt(d.temperature,1);"
        "if(d.balanceStatus&&d.balanceStatus!=='N/A'){"
          "const statusEl=document.getElementById('balanceStatus');"
          "const detailsEl=document.getElementById('balanceDetails');"
          "const cardEl=document.getElementById('healthCard');"
          "const tooltipEl=document.getElementById('healthTooltip');"
          "statusEl.textContent=d.balanceStatus;"
          "detailsEl.textContent=d.balanceMessage;"
          "cardEl.className='card';"
          "statusEl.className='';"
          "if(d.balanceStatus==='Normal'){cardEl.classList.add('normal');statusEl.classList.add('health-normal');}"
          "else if(d.balanceStatus==='Advertencia'){cardEl.classList.add('warning');statusEl.classList.add('health-warning');}"
          "else if(d.balanceStatus==='Crítico'){cardEl.classList.add('critical');statusEl.classList.add('health-critical');}"
          "let tooltipText='Balance de '+d.cellCount+' celdas:\\n';"
          "tooltipText+='• Voltaje máximo: '+fmt(d.maxCellVoltage,3)+'V';"
          "if(d.maxCellId)tooltipText+=' (celda '+d.maxCellId+')';"
          "tooltipText+='\\n• Voltaje mínimo: '+fmt(d.minCellVoltage,3)+'V';"
          "if(d.minCellId)tooltipText+=' (celda '+d.minCellId+')';"
          "tooltipText+='\\n• Diferencia: '+fmt(d.imbalanceMv,1)+'mV';"
          "if(d.imbalanceMv<=40)tooltipText+='\\n• Estado: Excelente balance';"
          "else if(d.imbalanceMv<=60)tooltipText+='\\n• Estado: Revisar pronto';"
          "else tooltipText+='\\n• Estado: Requiere atención inmediata';"
          "tooltipEl.innerHTML=tooltipText.replace(/\\n/g,'<br>');"
        "}"
        "else{"
          "document.getElementById('balanceStatus').textContent='N/A';"
          "document.getElementById('balanceDetails').textContent='Sin datos de balance';"
          "document.getElementById('healthCard').className='card';"
          "document.getElementById('healthTooltip').innerHTML='No hay información de balance disponible para esta vista.';"
        "}"
      "}"
      "async function pull(){"
        "try{"
          "const endpoint=currentModule==='system'?'/battery-data':'/battery-data?module='+currentModule;"
          "const r=await fetch(endpoint,{cache:'no-store'});"
          "if(!r.ok)return;const d=await r.json();paint(d);"
        "}catch(e){}"
      "}"
      "async function updateModuleButtons(){"
        "try{"
          "const r=await fetch('/modules',{cache:'no-store'});"
          "if(!r.ok)return;const modules=await r.json();"
          "document.querySelectorAll('.module').forEach((btn,idx)=>{"
            "if(idx===0)return;" // Skip "Sistema" button
            "const moduleNum=idx;"
            "if(modules.includes(moduleNum)){"
              "btn.classList.remove('inactive');"
              "btn.disabled=false;"
            "}else{"
              "btn.classList.add('inactive');"
              "btn.disabled=true;"
            "}"
          "});"
        "}catch(e){}"
      "}"
      "async function sendCmd(){"
        "const c=document.getElementById('cmd').value||''; if(!c)return;"
        "try{await fetch('/cmd?q='+encodeURIComponent(c)); setTimeout(loadConsole,250);}catch(e){}"
      "}"
      "function clearConsole(){document.getElementById('console').textContent='';}"
      "function formatTerminalOutput(text){"
        "if(!text)return '';"
        "let lines=text.split('\\n');"
        "let formatted='';"
        "let inDataSection=false;"
        "for(let i=0;i<lines.length;i++){"
          "let line=lines[i];"
          "if(line.includes('Battery') && line.includes('Volt') && line.includes('Curr')){"
            "inDataSection=true;"
            "formatted+='<div class=\"header\">'+line+'</div>';"
          "}else if(line.includes('Power') && line.includes('Volt') && line.includes('Curr')){"
            "inDataSection=true;"
            "formatted+='<div class=\"header\">'+line+'</div>';"
          "}else if(inDataSection && /^\\d/.test(line)){"
            "let coloredLine=line;"
            "coloredLine=coloredLine.replace(/^(\\d+)/,'<span class=\"number\">$1</span>');"
            "coloredLine=coloredLine.replace(/(\\s)(\\d{4})(\\s)/g,'$1<span class=\"voltage\">$2</span>$3');"
            "coloredLine=coloredLine.replace(/(\\s)(-?\\d+)(\\s)/g,function(match,p1,p2,p3,offset){"
              "if(coloredLine.indexOf('<span class=\"voltage\">')>-1 && offset>coloredLine.indexOf('<span class=\"voltage\">')){"
                "if(p2.length==5) return p1+'<span class=\"temperature\">'+p2+'</span>'+p3;"
                "if(p2=='0'||p2.startsWith('-')) return p1+'<span class=\"current\">'+p2+'</span>'+p3;"
              "}"
              "return match;"
            "});"
            "coloredLine=coloredLine.replace(/(\\d+%)/g,'<span class=\"soc\">$1</span>');"
            "coloredLine=coloredLine.replace(/\\bNormal\\b/g,'<span class=\"state-normal\">Normal</span>');"
            "coloredLine=coloredLine.replace(/\\bIdle\\b/g,'<span class=\"state-idle\">Idle</span>');"
            "coloredLine=coloredLine.replace(/\\bAbsent\\b/g,'<span class=\"state-absent\">Absent</span>');"
            "formatted+='<div class=\"data-row\">'+coloredLine+'</div>';"
          "}else{"
            "if(line.trim()=='')inDataSection=false;"
            "formatted+='<div>'+line+'</div>';"
          "}"
        "}"
        "return formatted;"
      "}"
      "async function loadConsole(){"
        "try{const r=await fetch('/',{cache:'no-store'}); const t=await r.text();"
            "const m=t.match(/<h3>Respuesta<\\/h3><pre>([\\s\\S]*?)<\\/pre>/);"
            "const rawText=m?m[1].replace(/&lt;/g,'<').replace(/&gt;/g,'>'):'';"
            "document.getElementById('console').innerHTML=formatTerminalOutput(rawText);"
        "}catch(e){}"
      "}"
      "function quickCmd(cmd){"
        "document.getElementById('cmd').value=cmd; sendCmd();"
      "}"
      "function quickCmdBat(){"
        "const cmd = currentModule==='system' ? 'bat' : 'bat '+currentModule;"
        "document.getElementById('cmd').value=cmd; sendCmd();"
      "}"
      "function updateBatButton(){"
        "const btn=document.getElementById('batCmd');"
        "btn.textContent = currentModule==='system' ? 'bat' : 'bat '+currentModule;"
      "}"
      "function checkMobile(){"
        "const isMobile = window.innerWidth <= 768;"
        "const hint = document.getElementById('scroll-hint');"
        "if(isMobile && hint) hint.style.display = 'inline';"
      "}"
      "pull(); loadConsole(); updateModuleButtons(); updateBatButton(); checkMobile(); setInterval(pull,3000); setInterval(updateModuleButtons,10000);"
      "window.addEventListener('resize', checkMobile);"
      "document.getElementById('cmd').addEventListener('keydown',e=>{if(e.key==='Enter')sendCmd();});"
      "let tooltipVisible = false;"
      "document.getElementById('healthInfo').addEventListener('click', function(e){"
        "e.preventDefault();"
        "const tooltip = document.getElementById('healthTooltip');"
        "tooltipVisible = !tooltipVisible;"
        "tooltip.style.display = tooltipVisible ? 'block' : 'none';"
        "if(tooltipVisible) setTimeout(() => {tooltipVisible = false; tooltip.style.display = 'none';}, 3000);"
      "});"
      
      // Balance History Functions
      "function toggleHistory(){"
        "const section = document.getElementById('historySection');"
        "if(section.style.display === 'none'){"
          "section.style.display = 'block';"
          "refreshHistory();"
        "}else{"
          "section.style.display = 'none';"
        "}"
      "}"
      "async function refreshHistory(){"
        "try{"
          "const r = await fetch('/balance-history', {cache: 'no-store'});"
          "if(!r.ok) return;"
          "const data = await r.json();"
          "displayHistory(data);"
          "populateBatteryFilter(data);"
        "}catch(e){console.error('Error loading history:', e);}"
      "}"
      "function displayHistory(data){"
        "const tbody = document.getElementById('historyBody');"
        "const filter = document.getElementById('batteryFilter').value;"
        "tbody.innerHTML = '';"
        "if(!data.data || data.data.length === 0){"
          "tbody.innerHTML = '<tr><td colspan=\"5\" style=\"text-align:center;padding:20px;color:#6b7280\">No hay datos disponibles</td></tr>';"
          "return;"
        "}"
        "const filteredData = filter === 'all' ? data.data : data.data.filter(d => d.batteryId == filter);"
        "filteredData.slice(-48).reverse().forEach(entry => {"
          "const date = new Date(entry.timestamp);"
          "const time = date.toLocaleTimeString('es-ES', {hour: '2-digit', minute: '2-digit'});"
          "const status = entry.balanceMv <= 40 ? 'Normal' : entry.balanceMv <= 60 ? 'Advertencia' : 'Crítico';"
          "const statusClass = entry.balanceMv <= 40 ? 'health-normal' : entry.balanceMv <= 60 ? 'health-warning' : 'health-critical';"
          "tbody.innerHTML += '<tr>' +"
            "'<td style=\"padding:6px;border:1px solid #e2e8f0\">' + time + '</td>' +"
            "'<td style=\"padding:6px;border:1px solid #e2e8f0\">' + entry.batteryId + '</td>' +"
            "'<td style=\"padding:6px;border:1px solid #e2e8f0\">' + entry.balanceMv + '</td>' +"
            "'<td style=\"padding:6px;border:1px solid #e2e8f0\">' + entry.socPercent + '%</td>' +"
            "'<td style=\"padding:6px;border:1px solid #e2e8f0\" class=\"' + statusClass + '\">' + status + '</td>' +"
          "'</tr>';"
        "});"
      "}"
      "function populateBatteryFilter(data){"
        "const select = document.getElementById('batteryFilter');"
        "const currentValue = select.value;"
        "const batteries = [...new Set(data.data.map(d => d.batteryId))].sort((a,b) => a-b);"
        "select.innerHTML = '<option value=\"all\">Todas las baterías</option>';"
        "batteries.forEach(id => {"
          "select.innerHTML += '<option value=\"' + id + '\">Batería ' + id + '</option>';"
        "});"
        "select.value = currentValue;"
        "select.onchange = () => refreshHistory();"
      "}"
      "function exportHistory(){"
        "fetch('/balance-history').then(r => r.json()).then(data => {"
          "let csv = 'Timestamp,Battery ID,Balance (mV),SOC (%),Status\\n';"
          "data.data.forEach(entry => {"
            "const date = new Date(entry.timestamp);"
            "const status = entry.balanceMv <= 40 ? 'Normal' : entry.balanceMv <= 60 ? 'Warning' : 'Critical';"
            "csv += date.toISOString() + ',' + entry.batteryId + ',' + entry.balanceMv + ',' + entry.socPercent + ',' + status + '\\n';"
          "});"
          "const blob = new Blob([csv], {type: 'text/csv'});"
          "const url = URL.createObjectURL(blob);"
          "const a = document.createElement('a');"
          "a.href = url;"
          "a.download = 'balance_history_' + new Date().toISOString().split('T')[0] + '.csv';"
          "a.click();"
          "URL.revokeObjectURL(url);"
        "});"
      "}"
    "</script>");

    html += F("</main></body></html>");
    server.send(200, "text/html", html); });

  // ---------- /battery-data: PARSEAR 'bat' + completar I con 'pwrsys' ----------
  server.on("/battery-data", [&server, batteryData]()
            {
    String moduleParam = server.arg("module");
    bool isSystemView = (moduleParam.length() == 0);
    int targetModule = isSystemView ? 0 : moduleParam.toInt();

    if (isSystemView)
    {
      // Vista del sistema completo (como antes)
      String raw = _bmsSendCmd("bat", 4000);

      int cells = 0;
      long sum_mV = 0;
      long sum_mA = 0;
      long sum_mC = 0;
      long sum_soc = 0;

      auto scan4ints = [](const String &line, long &a, long &b, long &c, long &d) -> bool
      {
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
        if (n == 4)
        {
          a = vals[0];
          b = vals[1];
          c = vals[2];
          d = vals[3];
          return true;
        }
        return false;
      };

      int start = 0;
      while (start < (int)raw.length())
      {
        int end = raw.indexOf('\n', start);
        String line = (end < 0) ? raw.substring(start) : raw.substring(start, end);
        start = (end < 0) ? raw.length() : end + 1;
        line.trim();
        if (line.length() == 0)
          continue;
        if (!isDigit(line[0]))
          continue;

        long idx, mv, ma, mC;
        if (!scan4ints(line, idx, mv, ma, mC))
          continue;

        // SOC: número justo antes de '%'
        int pcent = line.indexOf('%');
        int pnum = -1, soc = 0;
        if (pcent > 0)
        {
          pnum = pcent - 1;
          while (pnum >= 0 && isDigit(line[pnum]))
            pnum--;
          pnum++;
          soc = line.substring(pnum, pcent).toInt();
        }

        (void)idx;
        cells++;
        sum_mV += mv;
        sum_mA += ma;
        sum_mC += mC;
        sum_soc += soc;
      }

      float packV = (cells > 0) ? (sum_mV / 1000.0f) : 0.0f;                  // V
      float currA = (cells > 0) ? ((sum_mA / (float)cells) / 1000.0f) : 0.0f; // A (media celda)
      float tempC = (cells > 0) ? ((sum_mC / (float)cells) / 1000.0f) : 0.0f; // °C (media)
      int socPc = (cells > 0) ? (int)(sum_soc / cells) : 0;

      // --- Si la corriente sigue a 0, intenta obtenerla de 'pwrsys' ---
      if (fabs(currA) < 0.0005f)
      {
        String sys = _bmsSendCmd("pwrsys", 3000);

        // Busca específicamente "System Curr" en la respuesta de pwrsys
        int p = sys.indexOf("System Curr");
        if (p >= 0)
        {
          // Busca el ":" después de "System Curr"
          int colon = sys.indexOf(":", p);
          if (colon >= 0)
          {
            // Extrae el número después del ":"
            int i = colon + 1;
            // Salta espacios
            while (i < (int)sys.length() && (sys[i] == ' ' || sys[i] == '\t'))
              i++;

            String numStr = "";
            // Captura el número (puede ser negativo)
            if (i < (int)sys.length() && sys[i] == '-')
            {
              numStr += sys[i++];
            }
            while (i < (int)sys.length() && ((sys[i] >= '0' && sys[i] <= '9') || sys[i] == '.'))
            {
              numStr += sys[i++];
            }

            if (numStr.length() > 0)
            {
              float found = numStr.toFloat();
              // Convertir de mA a A
              currA = found / 1000.0f;
            }
          }
        }
      }

      float power = packV * currA;

      // Refleja también en el struct (si otros módulos lo usan)
      batteryData->avgVoltage = (int)(packV * 1000.0f);
      batteryData->currentDC = (int)(currA * 1000.0f);
      batteryData->temp = (int)tempC;
      batteryData->soc = socPc;

      // Análisis de balance de celdas para alertas
      String balanceStatus = "normal";
      String balanceMessage = "";
      int maxVoltage = 0, minVoltage = 99999;
      int imbalanceMv = 0;
      
      // Re-analizar para obtener voltajes individuales de celdas
      int cellCount = 0;
      start = 0;
      while (start < (int)raw.length())
      {
        int end = raw.indexOf('\n', start);
        String line = (end < 0) ? raw.substring(start) : raw.substring(start, end);
        start = (end < 0) ? raw.length() : end + 1;
        line.trim();
        
        if (line.length() == 0 || !isDigit(line[0])) continue;
        
        long idx, mv, ma, mC;
        if (scan4ints(line, idx, mv, ma, mC))
        {
          cellCount++;
          if (mv > maxVoltage) maxVoltage = mv;
          if (mv < minVoltage) minVoltage = mv;
        }
      }
      
      if (cellCount > 0)
      {
        imbalanceMv = maxVoltage - minVoltage;
        
        if (imbalanceMv <= 40)
        {
          balanceStatus = "normal";
          balanceMessage = "✅ Balance normal: " + String(imbalanceMv) + " mV";
        }
        else if (imbalanceMv <= 60)
        {
          balanceStatus = "warning";
          balanceMessage = "⚠️ Vigilar balance: " + String(imbalanceMv) + " mV";
        }
        else
        {
          balanceStatus = "critical";
          balanceMessage = "❗ Accionar: " + String(imbalanceMv) + " mV - Revisar balanceador";
        }
      }

      String json = "{";
      json += "\"soc\":" + String(socPc) + ",";
      json += "\"voltage\":" + String(packV, 3) + ",";
      json += "\"current\":" + String(currA, 3) + ",";
      json += "\"power\":" + String(power, 1) + ",";
      json += "\"temperature\":" + String(tempC, 1) + ",";
      json += "\"balanceStatus\":\"" + balanceStatus + "\",";
      json += "\"balanceMessage\":\"" + balanceMessage + "\",";
      json += "\"imbalanceMv\":" + String(imbalanceMv) + ",";
      json += "\"cellCount\":" + String(cellCount) + ",";
      json += "\"maxCellVoltage\":" + String(maxVoltage) + ",";
      json += "\"minCellVoltage\":" + String(minVoltage);
      json += "}";
      server.send(200, "application/json", json);
    }
    else
    {
      // Vista de módulo individual - usar los datos de pwr pero calcular temperatura como promedio
      String pwrResponse = _bmsSendCmd("pwr", 4000);

      float packV = 0.0f;
      float currA = 0.0f;
      float tempC = 0.0f;
      int socPc = 0;
      bool found = false;

      int start = 0;
      while (start < (int)pwrResponse.length())
      {
        int end = pwrResponse.indexOf('\n', start);
        String line = (end < 0) ? pwrResponse.substring(start) : pwrResponse.substring(start, end);
        start = (end < 0) ? pwrResponse.length() : end + 1;
        line.trim();

        if (line.length() == 0)
          continue;
        if (!isDigit(line[0]))
          continue;
        if (line.indexOf("Absent") >= 0)
          continue;

        // Extraer el número del módulo (primer número de la línea)
        int spacePos = line.indexOf(' ');
        if (spacePos > 0)
        {
          int moduleNum = line.substring(0, spacePos).toInt();
          if (moduleNum == targetModule)
          {
            found = true;
            
            // Dividir la línea en tokens
            String tokens[25];
            int tokenCount = 0;
            int pos = 0;
            
            while (pos < (int)line.length() && tokenCount < 25)
            {
              while (pos < (int)line.length() && (line[pos] == ' ' || line[pos] == '\t'))
                pos++;
              
              if (pos >= (int)line.length()) break;
              
              int start = pos;
              while (pos < (int)line.length() && line[pos] != ' ' && line[pos] != '\t')
                pos++;
              
              tokens[tokenCount++] = line.substring(start, pos);
            }
            
            if (tokenCount >= 17)
            {
              packV = tokens[1].toFloat() / 1000.0f;    // Volt: mV -> V
              currA = tokens[2].toFloat() / 1000.0f;    // Curr: mA -> A
              
              // Para consistencia con el sistema, obtener temperatura del comando bat
              // y calcular promedio de las celdas de esta batería
              String batCmd = "bat " + String(targetModule);
              String batResponse = _bmsSendCmd(batCmd, 3000);
              
              // Calcular promedio de temperatura de las celdas de esta batería
              int cells = 0;
              long sum_mC = 0;
              
              auto scan4ints = [](const String &line, long &a, long &b, long &c, long &d) -> bool
              {
                const char *s = line.c_str();
                long vals[4];
                int n = 0;
                while (*s && n < 4)
                {
                  while (*s && !((*s == '-') || (*s >= '0' && *s <= '9')))
                    s++;
                  if (!*s) break;
                  char *end;
                  long v = strtol(s, &end, 10);
                  vals[n++] = v;
                  s = end;
                }
                if (n == 4)
                {
                  a = vals[0]; b = vals[1]; c = vals[2]; d = vals[3];
                  return true;
                }
                return false;
              };
              
              int batStart = 0;
              while (batStart < (int)batResponse.length())
              {
                int batEnd = batResponse.indexOf('\n', batStart);
                String batLine = (batEnd < 0) ? batResponse.substring(batStart) : batResponse.substring(batStart, batEnd);
                batStart = (batEnd < 0) ? batResponse.length() : batEnd + 1;
                batLine.trim();
                
                if (batLine.length() == 0 || !isDigit(batLine[0])) continue;
                
                long id, mv, ma, mC;
                if (scan4ints(batLine, id, mv, ma, mC))
                {
                  cells++;
                  sum_mC += mC;
                }
              }
              
              // Usar promedio de temperatura si hay datos de bat, sino usar fallback
              if (cells > 0)
              {
                tempC = ((sum_mC / (float)cells) / 1000.0f); // Promedio como en sistema
              }
              else
              {
                // Fallback: usar temperatura de pwr como antes
                if (tokens[3] != "-" && tokens[3].length() > 0) {
                  tempC = tokens[3].toFloat() / 1000.0f;
                } else if (tokens[16] != "-" && tokens[16].length() > 0) {
                  tempC = tokens[16].toFloat() / 1000.0f;
                }
              }
              
              // SOC de pwr
              String coulombStr = tokens[12];
              int percentPos = coulombStr.indexOf('%');
              if (percentPos > 0)
              {
                socPc = coulombStr.substring(0, percentPos).toInt();
              }
            }
            break;
          }
        }
      }

      // Análisis de balance para módulo individual
      String balanceStatus = "N/A";
      String balanceMessage = "Sin datos";
      float imbalanceMv = 0;
      int cellCount = 0;
      float maxVoltage = 0;
      float minVoltage = 0;
      int maxCellId = 0;
      int minCellId = 0;

      if (found) {
        String batCmd = "bat " + String(targetModule);
        String batResponse = _bmsSendCmd(batCmd, 3000);
        
        float minV = 999999;
        float maxV = 0;
        
        auto scan4ints = [](const String &line, long &a, long &b, long &c, long &d) -> bool
        {
          const char *s = line.c_str();
          long vals[4];
          int n = 0;
          while (*s && n < 4)
          {
            while (*s && !((*s == '-') || (*s >= '0' && *s <= '9')))
              s++;
            if (!*s) break;
            char *end;
            long v = strtol(s, &end, 10);
            vals[n++] = v;
            s = end;
          }
          if (n == 4)
          {
            a = vals[0]; b = vals[1]; c = vals[2]; d = vals[3];
            return true;
          }
          return false;
        };
        
        int start = 0;
        while (start < (int)batResponse.length())
        {
          int end = batResponse.indexOf('\n', start);
          String line = (end < 0) ? batResponse.substring(start) : batResponse.substring(start, end);
          start = (end < 0) ? batResponse.length() : end + 1;
          line.trim();
          
          if (line.length() == 0 || !isDigit(line[0])) continue;
          
          long id, mv, ma, mC;
          if (scan4ints(line, id, mv, ma, mC))
          {
            float cellV = mv / 1000.0f;
            if (cellV > maxV) {
              maxV = cellV;
              maxCellId = id;
            }
            if (cellV < minV) {
              minV = cellV;
              minCellId = id;
            }
            cellCount++;
          }
        }
        
        if (cellCount > 0) {
          imbalanceMv = (maxV - minV) * 1000;
          maxVoltage = maxV;
          minVoltage = minV;
          
          // Categorizar estado (umbrales LiFePO4)
          if (imbalanceMv <= 40) {
            balanceStatus = "Normal";
            balanceMessage = "Balance óptimo (" + String(imbalanceMv, 1) + "mV)";
          } else if (imbalanceMv <= 60) {
            balanceStatus = "Advertencia";
            balanceMessage = "Desequilibrio moderado (" + String(imbalanceMv, 1) + "mV)";
          } else {
            balanceStatus = "Crítico";
            balanceMessage = "Desequilibrio alto (" + String(imbalanceMv, 1) + "mV)";
          }
        }
      }

      if (!found)
      {
        // Módulo no encontrado o sin datos
        String json = "{";
        json += "\"soc\":0,";
        json += "\"voltage\":0.0,";
        json += "\"current\":0.0,";
        json += "\"power\":0.0,";
        json += "\"temperature\":0.0,";
        json += "\"error\":\"Batería no disponible\"";
        json += "}";
        server.send(200, "application/json", json);
        return;
      }

      float power = packV * currA;

      String json = "{";
      json += "\"soc\":" + String(socPc) + ",";
      json += "\"voltage\":" + String(packV, 3) + ",";
      json += "\"current\":" + String(currA, 3) + ",";
      json += "\"power\":" + String(power, 1) + ",";
      json += "\"temperature\":" + String(tempC, 1) + ",";
      json += "\"balanceStatus\":\"" + balanceStatus + "\",";
      json += "\"balanceMessage\":\"" + balanceMessage + "\",";
      json += "\"imbalanceMv\":" + String(imbalanceMv, 1) + ",";
      json += "\"cellCount\":" + String(cellCount) + ",";
      json += "\"maxCellVoltage\":" + String(maxVoltage, 3) + ",";
      json += "\"minCellVoltage\":" + String(minVoltage, 3) + ",";
      json += "\"maxCellId\":" + String(maxCellId) + ",";
      json += "\"minCellId\":" + String(minCellId);
      json += "}";
      server.send(200, "application/json", json);
    } });

  // ---------- /modules: detectar qué baterías están presentes ----------
  server.on("/modules", [&server]()
            {
    String pwr = _bmsSendCmd("pwr", 4000);
    
    String json = "[";
    bool first = true;
    
    // Parsear la salida del comando pwr para encontrar módulos presentes
    int start = 0;
    while (start < (int)pwr.length()) {
      int end = pwr.indexOf('\n', start);
      String line = (end < 0) ? pwr.substring(start) : pwr.substring(start, end);
      start = (end < 0) ? pwr.length() : end + 1;
      line.trim();
      
      if (line.length() == 0) continue;
      if (!isDigit(line[0])) continue;
      
      // Buscar líneas que NO contienen "Absent"
      if (line.indexOf("Absent") < 0) {
        // Extraer el número del módulo (primer número de la línea)
        int spacePos = line.indexOf(' ');
        if (spacePos > 0) {
          int moduleNum = line.substring(0, spacePos).toInt();
          if (moduleNum > 0) {
            if (!first) json += ",";
            json += String(moduleNum);
            first = false;
          }
        }
      }
    }
    
    json += "]";
    server.send(200, "application/json", json); });

  // ---------- /cmd: enviar comandos (redirige a / como el repo) ----------
  server.on("/cmd", [&server](void)
            {
    const String cmd = server.arg("q");   // escribe: help, bat, pwrsys, pwr...
    lastCommandOutput.clear();
    if (cmd.length()) {
      lastCommandOutput = _bmsSendCmd(cmd, 5000);
#if DBG_BMS
      Serial2.println("[BMS] RX:\n" + lastCommandOutput);
#endif
    }
    // Redirige para que la home embeba la última respuesta y el JS la pinte en la terminal
    server.sendHeader("Location", "/");
    server.send(303); });

  // ---------- /balance-history: serve historical balance data ----------
  server.on("/balance-history", [&server, batteryData]()
            {
    String json = "{\"data\":[";
    bool first = true;
    
    // Get all valid entries from the history
    for (uint8_t i = 0; i < batteryData->history.entryCount; i++) {
      balanceHistoryEntry* entry = batteryData->history.getEntry(i);
      if (entry && entry->isValid) {
        if (!first) json += ",";
        json += "{";
        json += "\"timestamp\":" + String(entry->timestamp) + ",";
        json += "\"batteryId\":" + String(entry->batteryId) + ",";
        json += "\"balanceMv\":" + String(entry->balanceMv) + ",";
        json += "\"socPercent\":" + String(entry->socPercent);
        json += "}";
        first = false;
      }
    }
    
    json += "],";
    json += "\"totalEntries\":" + String(batteryData->history.entryCount) + ",";
    json += "\"maxEntries\":" + String(MAX_BALANCE_HISTORY_ENTRIES) + ",";
    json += "\"currentTime\":" + String(millis());
    json += "}";
    
    server.send(200, "application/json", json); });

  // ---------- /debug-history: debug endpoint for history status ----------
  server.on("/debug-history", [&server, batteryData]()
            {
    String json = "{";
    json += "\"currentTime\":" + String(millis()) + ",";
    json += "\"lastSaveTime\":" + String(batteryData->history.lastSaveTime) + ",";
    json += "\"timeSinceLastSave\":" + String(millis() - batteryData->history.lastSaveTime) + ",";
    json += "\"shouldRecord\":" + String(batteryData->shouldRecordHistory(millis()) ? "true" : "false") + ",";
    json += "\"currentIndex\":" + String(batteryData->history.currentIndex) + ",";
    json += "\"entryCount\":" + String(batteryData->history.entryCount) + ",";
    json += "\"maxEntries\":" + String(MAX_BALANCE_HISTORY_ENTRIES);
    json += "}";
    
    server.send(200, "application/json", json); });
}

#endif //
