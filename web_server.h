#ifndef ANENJI_WEB_SERVER_H
#define ANENJI_WEB_SERVER_H

#include <Arduino.h>
#include <WebServer.h>

struct WebLiveData {
  bool valid;
  char status[96];
  char updateTime[32];
  char inverterIp[24];
  char mode[96];
  char faults[256];
  char warnings[256];
  float gridPower,pvPower,loadPower;
  float batteryPower,batteryChargePower,batteryDischargePower;
  float gridVoltage,pvVoltage,loadVoltage,batteryVoltage;
  float gridKwh,pvKwh,loadKwh;
  float batteryChargeKwh,batteryDischargeKwh;
  uint8_t batteryPercent;
};

struct WebSample {
  uint32_t timestamp;
  uint16_t minuteOfDay;
  float gridPower,pvPower,loadPower;
  float batteryChargePower,batteryDischargePower;
};

struct WebHistoryHour {
  uint16_t year;
  uint8_t month,day,hour;
  float gridKwh,pvKwh,loadKwh;
  float batteryChargeKwh,batteryDischargeKwh;
};

struct WebSettings {
  String ssid,password,inverterIp;
  uint32_t pageUpdateSeconds,registerIntervalSeconds;
  int8_t timezone;
  String language;
};

using WebLiveCallback=void (*)(WebLiveData&);
using WebSamplesCallback=size_t (*)(WebSample*,size_t);
using WebHistoryCallback=size_t (*)(WebHistoryHour*,size_t,const String&);
using WebSettingsCallback=void (*)(const WebSettings&);

WebServer anenjiWebServer(80);
WebLiveCallback webLiveCallback=nullptr;
WebSamplesCallback webSamplesCallback=nullptr;
WebHistoryCallback webHistoryCallback=nullptr;
WebSettingsCallback webSettingsCallback=nullptr;
WebSettings webSettings;
bool webSetupMode=false;

String webHtmlEscape(const String& value){
  String out;
  for(size_t i=0;i<value.length();++i){
    switch(value[i]){
      case '&':out+="&amp;";break;case '<':out+="&lt;";break;
      case '>':out+="&gt;";break;case '"':out+="&quot;";break;
      case '\'':out+="&#39;";break;default:out+=value[i];
    }
  }
  return out;
}

String webJsonEscape(const String& value){
  String out;
  for(size_t i=0;i<value.length();++i){
    char c=value[i];
    if(c=='"'||c=='\\'){out+='\\';out+=c;}
    else if(c=='\n')out+=F("\\n");
    else if(c=='\r')out+=F("\\r");
    else if(static_cast<uint8_t>(c)<32)out+=' ';
    else out+=c;
  }
  return out;
}

String webSettingsPage(){
  bool ru=webSettings.language=="ru";String h;h.reserve(6500);
  h+=F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Anenji settings</title><style>*{box-sizing:border-box}body{font:16px Arial;background:#edf1f8;color:#172033;margin:0;padding:20px}.box{max-width:650px;margin:auto;background:#fff;padding:24px;border-radius:18px;box-shadow:0 5px 25px #c3cada}label{display:block;font-weight:bold;margin-top:15px}input,select{width:100%;padding:11px;margin-top:5px;border:1px solid #bec8d8;border-radius:8px;font-size:16px}button{margin-top:22px;padding:12px 20px;border:0;border-radius:8px;background:#326de8;color:white;font-size:16px}.info{padding:12px;background:#eaf0ff;border-radius:8px}a{color:#326de8}</style></head><body><div class='box'>");
  h+=ru?F("<h2>Настройки Anenji</h2>"):F("<h2>Anenji settings</h2>");
  if(webSetupMode)h+=F("<p class='info'>Точка доступа Anenji-setup. Сохраните настройки для подключения к Wi-Fi.</p>");
  else h+=F("<p><a href='/'>← Dashboard / Панель</a></p>");
  h+=F("<form method='post' action='/save'>");
  h+=ru?F("<label>Имя Wi-Fi сети</label>"):F("<label>Wi-Fi network name</label>");
  h+=F("<input name='ssid' required value=\"")+webHtmlEscape(webSettings.ssid)+F("\">");
  h+=ru?F("<label>Пароль Wi-Fi</label>"):F("<label>Wi-Fi password</label>");
  h+=F("<input name='password' type='password' value=\"")+webHtmlEscape(webSettings.password)+F("\">");
  h+=ru?F("<label>IP инвертора</label>"):F("<label>Inverter IP address</label>");
  h+=F("<input name='inverterIp' required value=\"")+webHtmlEscape(webSettings.inverterIp)+F("\">");
  h+=ru?F("<label>Обновление страницы, секунд</label>"):F("<label>Page update interval, seconds</label>");
  h+=F("<input name='pageUpdate' type='number' min='1' max='3600' value='")+String(webSettings.pageUpdateSeconds)+F("'>");
  h+=ru?F("<label>Опрос регистров, секунд</label>"):F("<label>Register reading interval, seconds</label>");
  h+=F("<input name='registerInterval' type='number' min='5' max='3600' value='")+String(webSettings.registerIntervalSeconds)+F("'>");
  h+=ru?F("<label>Часовой пояс UTC</label>"):F("<label>Time zone UTC</label>");
  h+=F("<input name='timezone' type='number' min='-12' max='14' value='")+String(webSettings.timezone)+F("'>");
  h+=ru?F("<label>Язык</label>"):F("<label>Language</label>");
  h+=F("<select name='language'><option value='en'");if(!ru)h+=F(" selected");
  h+=F(">English</option><option value='ru'");if(ru)h+=F(" selected");
  h+=F(">Русский</option></select>");
  h+=ru?F("<button>Сохранить и перезагрузить</button>"):F("<button>Save and reboot</button>");
  h+=F("</form></div></body></html>");return h;
}

String webDashboardPage(){
  String h;h.reserve(10000);
  h+=F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Anenji</title><style>body{font:16px Arial;margin:0;background:#edf1f8;color:#172033}.wrap{max-width:1450px;margin:auto;padding:16px}.block{background:#fff;border-radius:20px;padding:20px;margin-bottom:18px;box-shadow:0 5px 22px #c4ccda}.top{display:flex;justify-content:space-between;align-items:center}.settings{color:#326de8}.diagram{display:grid;grid-template-columns:1fr 1.5fr 1.1fr 1.5fr 1fr;grid-template-rows:1fr 1fr;gap:32px 0;align-items:center;min-height:410px}.device{text-align:center}.icon{height:108px;max-width:150px;margin:auto;border-radius:22px;display:grid;place-items:center;background:#eef3ff;box-shadow:0 4px 16px #d9e1ef;font-size:57px}.card{max-width:170px;margin:9px auto;background:#fff;border-radius:18px;box-shadow:0 4px 14px #dbe2ed;padding:10px 5px;font-size:17px;font-weight:600;line-height:1.25}.grid-device{grid-column:1;grid-row:1}.battery-device{grid-column:1;grid-row:2}.pv-device{grid-column:5;grid-row:1}.load-device{grid-column:5;grid-row:2}.inverter{grid-column:3;grid-row:1/3;text-align:center}.inverter .icon{height:142px;background:linear-gradient(to bottom,#8ab6ff,#d9e8ff 52%,#fff);font-size:72px}.line{height:6px;background:var(--color);border-radius:5px}.grid-line{grid-column:2;grid-row:1;--color:#1685ee}.battery-line{grid-column:2;grid-row:2;--color:#2ab65b}.pv-line{grid-column:4;grid-row:1;--color:#efa536}.load-line{grid-column:4;grid-row:2;--color:#ae72df}.off{opacity:.22}.tiles{display:grid;grid-template-columns:repeat(5,1fr);gap:10px}.tile{border-radius:13px;padding:15px;text-align:center;border-bottom:4px solid var(--color)}.chart{width:100%;height:350px}.checks{display:flex;flex-wrap:wrap;gap:8px 17px;margin-top:12px}@media(max-width:700px){.wrap{padding:9px}.block{padding:13px}.diagram{grid-template-columns:1fr 18px 1fr 18px 1fr;gap:16px 0;min-height:365px}.icon{height:78px;font-size:43px}.inverter .icon{height:110px;font-size:55px}.card{font-size:12px;padding:7px 3px}.tiles{grid-template-columns:repeat(2,1fr)}}</style></head><body><main class='wrap'><section class='block'><div class='top'><div><b>Status:</b> <span id='status'>Waiting for data</span></div><a class='settings' href='/settings'>Настройки / Settings</a></div><div><b>Inverter IP:</b> <span id='inverterIp'>—</span></div></section><section class='block'><div class='diagram'><div class='device grid-device'><div class='icon'>⚡</div><div class='card'>Grid<br><span id='gridPower'>0</span> W<br><small><span id='gridVoltage'>0</span> V</small></div></div><div class='line grid-line off' id='flowGrid'></div><div class='device battery-device'><div class='icon'>🔋</div><div class='card'>Battery<br><span id='batteryPower'>0</span> W<br><small><span id='batteryVoltage'>0</span> V<br><span id='batteryPercent'>0</span>%</small></div></div><div class='line battery-line off' id='flowBattery'></div><div class='inverter'><div class='icon'>▣</div><div class='card'>Anenji<div id='mode'>Режим: ожидание данных</div><div id='faults'>Неполадок нет</div><div id='warnings'>Предупреждений нет</div></div></div><div class='line pv-line off' id='flowPv'></div><div class='device pv-device'><div class='icon'>☀️</div><div class='card'>PV<br><span id='pvPower'>0</span> W<br><small><span id='pvVoltage'>0</span> V</small></div></div><div class='line load-line off' id='flowLoad'></div><div class='device load-device'><div class='icon'>🏠</div><div class='card'>Load<br><span id='loadPower'>0</span> W<br><small><span id='loadVoltage'>0</span> V</small></div></div></div></section><section class='block'><h2>Total energy — <span id='energyDate'>—</span></h2><div class='tiles'><div class='tile' style='--color:#1685ee'>Grid<br><b id='gridKwh'>0</b> kWh</div><div class='tile' style='--color:#efa536'>PV<br><b id='pvKwh'>0</b> kWh</div><div class='tile' style='--color:#ae72df'>Load<br><b id='loadKwh'>0</b> kWh</div><div class='tile' style='--color:#2ab65b'>Battery charge<br><b id='chargeKwh'>0</b> kWh</div><div class='tile' style='--color:#e34449'>Battery discharge<br><b id='dischargeKwh'>0</b> kWh</div></div></section><section class='block'><h2>History</h2><canvas id='historyChart' class='chart'></canvas><div class='checks'>Grid · PV · Load · Battery charge · Battery discharge</div></section></main><script>const $=id=>document.getElementById(id);async function refresh(){try{const r=await fetch('/api/live',{cache:'no-store'}),d=await r.json();$('status').textContent=d.status+((d.updateTime&&d.updateTime!=='-')?' — '+d.updateTime:'');$('inverterIp').textContent=d.inverterIp;for(const k of ['gridPower','pvPower','loadPower','batteryPower','gridVoltage','pvVoltage','loadVoltage','batteryVoltage'])$(k).textContent=Number(d[k]||0).toFixed(k.includes('Voltage')?1:0);$('batteryPercent').textContent=d.batteryPercent||0;$('mode').textContent=d.mode||'Ожидание данных';$('faults').textContent=d.faults||'Неполадок нет';$('warnings').textContent=d.warnings||'Предупреждений нет'}catch(e){console.error(e)}}refresh();setInterval(refresh,WEB_INTERVAL);</script></body></html>");
  h.replace("WEB_INTERVAL",String(webSettings.pageUpdateSeconds*1000UL));return h;
}

void webHandleLive(){
  WebLiveData d={};if(webLiveCallback)webLiveCallback(d);String j="{";
  j+=F("\"valid\":");j+=d.valid?F("true"):F("false");
  j+=F(",\"status\":\"")+webJsonEscape(d.status)+F("\",\"updateTime\":\"")+webJsonEscape(d.updateTime)+F("\",\"inverterIp\":\"")+webJsonEscape(d.inverterIp)+F("\",\"mode\":\"")+webJsonEscape(d.mode)+F("\",\"faults\":\"")+webJsonEscape(d.faults)+F("\",\"warnings\":\"")+webJsonEscape(d.warnings)+F("\"");
  j+=F(",\"gridPower\":");j+=String(d.gridPower,2);j+=F(",\"pvPower\":");j+=String(d.pvPower,2);j+=F(",\"loadPower\":");j+=String(d.loadPower,2);j+=F(",\"batteryPower\":");j+=String(d.batteryPower,2);j+=F(",\"gridVoltage\":");j+=String(d.gridVoltage,2);j+=F(",\"pvVoltage\":");j+=String(d.pvVoltage,2);j+=F(",\"loadVoltage\":");j+=String(d.loadVoltage,2);j+=F(",\"batteryVoltage\":");j+=String(d.batteryVoltage,2);j+=F(",\"batteryPercent\":");j+=String(d.batteryPercent);j+='}';
  anenjiWebServer.send(200,"application/json; charset=utf-8",j);
}

void webHandleSamples(){
  static WebSample points[288];size_t count=webSamplesCallback?webSamplesCallback(points,288):0;String j="[";
  for(size_t i=0;i<count;i++){if(i)j+=',';j+=F("{\"minute\":");j+=String(points[i].minuteOfDay);j+=F(",\"grid\":");j+=String(points[i].gridPower,2);j+=F(",\"pv\":");j+=String(points[i].pvPower,2);j+=F(",\"load\":");j+=String(points[i].loadPower,2);j+=F(",\"charge\":");j+=String(points[i].batteryChargePower,2);j+=F(",\"discharge\":");j+=String(points[i].batteryDischargePower,2);j+='}';}j+=']';
  anenjiWebServer.send(200,"application/json; charset=utf-8",j);
}

void webHandleHistory(){
  static WebHistoryHour history[400];size_t count=webHistoryCallback?webHistoryCallback(history,400,"hour"):0;String j="[";
  for(size_t i=0;i<count;i++){if(i)j+=',';j+=F("{\"year\":");j+=String(history[i].year);j+=F(",\"month\":");j+=String(history[i].month);j+=F(",\"day\":");j+=String(history[i].day);j+=F(",\"hour\":");j+=String(history[i].hour);j+=F(",\"grid\":");j+=String(history[i].gridKwh,3);j+=F(",\"pv\":");j+=String(history[i].pvKwh,3);j+=F(",\"load\":");j+=String(history[i].loadKwh,3);j+=F(",\"charge\":");j+=String(history[i].batteryChargeKwh,3);j+=F(",\"discharge\":");j+=String(history[i].batteryDischargeKwh,3);j+='}';}j+=']';
  anenjiWebServer.send(200,"application/json; charset=utf-8",j);
}

void webHandleSave(){
  WebSettings s=webSettings;s.ssid=anenjiWebServer.arg("ssid");s.password=anenjiWebServer.arg("password");s.inverterIp=anenjiWebServer.arg("inverterIp");
  s.pageUpdateSeconds=static_cast<uint32_t>(constrain(anenjiWebServer.arg("pageUpdate").toInt(),1L,3600L));
  s.registerIntervalSeconds=static_cast<uint32_t>(constrain(anenjiWebServer.arg("registerInterval").toInt(),5L,3600L));
  s.timezone=static_cast<int8_t>(constrain(anenjiWebServer.arg("timezone").toInt(),-12L,14L));
  s.language=anenjiWebServer.arg("language")=="ru"?"ru":"en";
  IPAddress ip;if(s.ssid.isEmpty()||!ip.fromString(s.inverterIp)){anenjiWebServer.send(400,"text/plain; charset=utf-8","Укажите имя Wi-Fi и правильный IPv4-адрес инвертора.");return;}
  webSettings=s;if(webSettingsCallback)webSettingsCallback(webSettings);
  anenjiWebServer.send(200,"text/html; charset=utf-8","<!doctype html><meta charset='utf-8'><h2>Настройки сохранены. Перезагрузка...</h2>");delay(1000);ESP.restart();
}

void beginWebServer(WebLiveCallback liveCallback,WebSamplesCallback samplesCallback,WebHistoryCallback historyCallback,WebSettingsCallback settingsCallback,const WebSettings& initialSettings,bool setupMode=false){
  webLiveCallback=liveCallback;webSamplesCallback=samplesCallback;webHistoryCallback=historyCallback;webSettingsCallback=settingsCallback;webSettings=initialSettings;webSetupMode=setupMode;
  anenjiWebServer.on("/",HTTP_GET,[]{if(webSetupMode){anenjiWebServer.sendHeader("Location","/settings");anenjiWebServer.send(302,"text/plain","");}else anenjiWebServer.send(200,"text/html; charset=utf-8",webDashboardPage());});
  anenjiWebServer.on("/settings",HTTP_GET,[]{anenjiWebServer.send(200,"text/html; charset=utf-8",webSettingsPage());});
  anenjiWebServer.on("/save",HTTP_POST,webHandleSave);anenjiWebServer.on("/api/live",HTTP_GET,webHandleLive);anenjiWebServer.on("/api/samples",HTTP_GET,webHandleSamples);anenjiWebServer.on("/api/history",HTTP_GET,webHandleHistory);anenjiWebServer.begin();
}
void handleWebServer(){anenjiWebServer.handleClient();}
#endif
