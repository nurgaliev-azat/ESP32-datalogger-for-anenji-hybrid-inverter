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

  float gridPower, pvPower, loadPower;
  float batteryPower, batteryChargePower, batteryDischargePower;
  float gridVoltage, pvVoltage, loadVoltage, batteryVoltage;
  float gridKwh, pvKwh, loadKwh;
  float batteryChargeKwh, batteryDischargeKwh;
  uint8_t batteryPercent;
};

struct WebSample {
  uint32_t timestamp;
  uint16_t minuteOfDay;
  float gridPower, pvPower, loadPower;
  float batteryChargePower, batteryDischargePower;
};

struct WebHistoryHour {
  uint16_t year;
  uint8_t month, day, hour;
  float gridKwh, pvKwh, loadKwh;
  float batteryChargeKwh, batteryDischargeKwh;
};

struct WebSettings {
  String ssid, password, inverterIp;
  uint32_t pageUpdateSeconds, registerIntervalSeconds;
  int8_t timezone;
  String language;
};

using WebLiveCallback = void (*)(WebLiveData&);
using WebSamplesCallback = size_t (*)(WebSample*, size_t);
using WebHistoryCallback = size_t (*)(WebHistoryHour*, size_t, const String&);
using WebSettingsCallback = void (*)(const WebSettings&);

WebServer anenjiWebServer(80);
WebLiveCallback webLiveCallback = nullptr;
WebSamplesCallback webSamplesCallback = nullptr;
WebHistoryCallback webHistoryCallback = nullptr;
WebSettingsCallback webSettingsCallback = nullptr;
WebSettings webSettings;
bool webSetupMode = false;

String webHtmlEscape(const String& value) {
  String out;
  for (size_t i = 0; i < value.length(); ++i) {
    switch (value[i]) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += value[i];
    }
  }
  return out;
}

String webJsonEscape(const String& value) {
  String out;
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += F("\\n");
    } else if (c == '\r') {
      out += F("\\r");
    } else if (static_cast<uint8_t>(c) < 32) {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

String webSettingsPage() {
  bool ru = webSettings.language == "ru";
  String h;
  h.reserve(6500);
  h += F(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Anenji settings</title><style>"
    "*{box-sizing:border-box}body{font:16px Arial;background:#edf1f8;"
    "color:#172033;margin:0;padding:20px}"
    ".box{max-width:650px;margin:auto;background:#fff;padding:24px;"
    "border-radius:18px;box-shadow:0 5px 25px #c3cada}"
    "label{display:block;font-weight:bold;margin-top:15px}"
    "input,select{width:100%;padding:11px;margin-top:5px;"
    "border:1px solid #bec8d8;border-radius:8px;font-size:16px}"
    "button{margin-top:22px;padding:12px 20px;border:0;"
    "border-radius:8px;background:#326de8;color:white;font-size:16px}"
    ".info{padding:12px;background:#eaf0ff;border-radius:8px}"
    "a{color:#326de8}</style></head><body><div class='box'>"
  );
  h += ru ? F("<h2>Настройки Anenji</h2>") : F("<h2>Anenji settings</h2>");
  if (webSetupMode) {
    h += F("<p class='info'>Точка доступа Anenji-setup. "
           "Сохраните настройки для подключения к Wi-Fi.</p>");
  } else {
    h += F("<p><a href='/'>← Dashboard / Панель</a></p>");
  }
  h += F("<form method='post' action='/save'>");

  h += ru ? F("<label>Имя Wi-Fi сети</label>")
          : F("<label>Wi-Fi network name</label>");
  h += F("<input name='ssid' required value=\"");
  h += webHtmlEscape(webSettings.ssid);
  h += F("\">");

  h += ru ? F("<label>Пароль Wi-Fi</label>")
          : F("<label>Wi-Fi password</label>");
  h += F("<input name='password' type='password' value=\"");
  h += webHtmlEscape(webSettings.password);
  h += F("\">");

  h += ru ? F("<label>IP инвертора</label>")
          : F("<label>Inverter IP address</label>");
  h += F("<input name='inverterIp' required value=\"");
  h += webHtmlEscape(webSettings.inverterIp);
  h += F("\">");

  h += ru ? F("<label>Обновление страницы, секунд</label>")
          : F("<label>Page update interval, seconds</label>");
  h += F("<input name='pageUpdate' type='number' min='1' max='3600' value='");
  h += String(webSettings.pageUpdateSeconds);
  h += F("'>");

  h += ru ? F("<label>Опрос регистров, секунд</label>")
          : F("<label>Register reading interval, seconds</label>");
  h += F("<input name='registerInterval' type='number' min='5' max='3600' value='");
  h += String(webSettings.registerIntervalSeconds);
  h += F("'>");

  h += ru ? F("<label>Часовой пояс UTC</label>")
          : F("<label>Time zone UTC</label>");
  h += F("<input name='timezone' type='number' min='-12' max='14' value='");
  h += String(webSettings.timezone);
  h += F("'>");

  h += ru ? F("<label>Язык</label>") : F("<label>Language</label>");
  h += F("<select name='language'><option value='en'");
  if (!ru) h += F(" selected");
  h += F(">English</option><option value='ru'");
  if (ru) h += F(" selected");
  h += F(">Русский</option></select>");
  h += ru ? F("<button>Сохранить и перезагрузить</button>")
          : F("<button>Save and reboot</button>");
  h += F("</form></div></body></html>");
  return h;
}

String webDashboardPage() {
  String h;
  h.reserve(31000);

  h += F(R"WEB(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Anenji</title>
<style>
*{box-sizing:border-box}
:root{
  --grid:#1685ee;--pv:#efa536;--load:#ae72df;
  --charge:#2ab65b;--discharge:#e34449;
}
body{font:16px Arial,sans-serif;margin:0;background:#edf1f8;color:#172033}
.wrap{max-width:1450px;margin:auto;padding:16px}
.block{background:white;border-radius:20px;padding:20px;margin:0 0 18px;
  box-shadow:0 5px 22px #c4ccda}
h2{font-size:21px;margin:0 0 16px}
.top{display:flex;justify-content:space-between;align-items:center;gap:12px}
.settings{color:#326de8;white-space:nowrap}
.status{line-height:1.9}
.diagram{display:grid;grid-template-columns:minmax(125px,1fr) minmax(20px,1.5fr)
  minmax(155px,1.1fr) minmax(20px,1.5fr) minmax(125px,1fr);
  grid-template-rows:1fr 1fr;gap:32px 0;align-items:center;min-height:410px}
.device{text-align:center;min-width:0}
.icon{height:108px;max-width:150px;margin:auto;border-radius:22px;
  display:grid;place-items:center;background:#eef3ff;
  box-shadow:0 4px 16px #d9e1ef;font-size:57px}
.icon svg{width:64px;height:64px}
.card{max-width:170px;margin:9px auto 0;background:#fff;border-radius:18px;
  box-shadow:0 4px 14px #dbe2ed;padding:10px 5px;
  font-size:17px;font-weight:600;line-height:1.25}
.card small{font-weight:400}
.grid-device{grid-column:1;grid-row:1}
.battery-device{grid-column:1;grid-row:2}
.pv-device{grid-column:5;grid-row:1}
.load-device{grid-column:5;grid-row:2}
.inverter{grid-column:3;grid-row:1/3;text-align:center;z-index:1}
.inverter .icon{max-width:150px;height:142px;
  background:linear-gradient(to bottom,#8ab6ff 0%,#d9e8ff 52%,#fff 100%);
  font-size:72px}
.inverter .card{margin-top:9px}
.diagnostic{font-size:13px;font-weight:400;overflow-wrap:anywhere;
  line-height:1.35;margin-top:5px}
.line{height:6px;background:var(--color);position:relative;
  border-radius:5px;min-width:0}
.line.off{opacity:.22}
.line .square{position:absolute;top:0;left:0;width:6px;height:6px;
  background:#fff;border:1px solid var(--color);
  animation:flow 2s linear infinite}
.line .square:nth-child(2){animation-delay:-.67s}
.line .square:nth-child(3){animation-delay:-1.33s}
.line.off .square{display:none}
.line.reverse .square{animation-direction:reverse}
@keyframes flow{from{left:0}to{left:calc(100% - 6px)}}
.grid-line{grid-column:2;grid-row:1;--color:var(--grid)}
.battery-line{grid-column:2;grid-row:2;--color:var(--charge)}
.pv-line{grid-column:4;grid-row:1;--color:var(--pv)}
.load-line{grid-column:4;grid-row:2;--color:var(--load)}
.tiles{display:grid;grid-template-columns:repeat(5,1fr);gap:10px}
.tile{border-radius:13px;padding:15px;text-align:center;
  background:color-mix(in srgb,var(--color) 12%,white);
  border-bottom:4px solid var(--color);color:var(--color)}
.tile b{font-size:21px}
.chart{display:block;width:100%;height:350px;background:#fff;
  border:1px solid #d2dbe8;border-radius:10px}
.checks{display:flex;flex-wrap:wrap;gap:8px 17px;margin-top:12px}
.checks label{color:var(--color);font-weight:bold;white-space:nowrap}
.checks input{accent-color:var(--color)}
.controls{display:flex;gap:7px;align-items:center;flex-wrap:wrap;margin:0 0 13px}
button{padding:9px 12px;border:0;border-radius:8px;
  background:#e5edff;color:#174496;cursor:pointer}
button.active{background:#326de8;color:white}
.period{min-width:125px;text-align:center;font-weight:bold}
.notice{font-size:13px;color:#617085;margin-top:8px}
@media(max-width:700px){
  .wrap{padding:9px}.block{padding:13px}
  .diagram{grid-template-columns:minmax(90px,1fr) 18px
    minmax(105px,1fr) 18px minmax(90px,1fr);
    gap:16px 0;min-height:365px}
  .icon{height:78px;font-size:43px}.icon svg{width:46px;height:46px}
  .inverter .icon{height:110px;font-size:55px}
  .card{font-size:12px;padding:7px 3px}
  .diagnostic{font-size:10px}
  .tiles{grid-template-columns:repeat(2,1fr)}
  .tile b{font-size:17px}
}
</style></head><body><main class="wrap">
<section class="block status">
 <div class="top"><div><b>Status:</b> <span id="status">Waiting for data</span></div>
 <a class="settings" href="/settings">Настройки / Settings</a></div>
 <div><b>Inverter IP:</b> <span id="inverterIp">—</span></div>
</section>

<section class="block">
 <div class="diagram">
  <div class="device grid-device">
   <div class="icon" aria-hidden="true">
    <svg viewBox="0 0 64 64"><path fill="#ffae3b"
      d="M35 3 15 34h14l-3 27 23-36H35z"/></svg>
   </div>
   <div class="card">Grid<br><span id="gridPower">0</span> W<br>
    <small><span id="gridVoltage">0</span> V</small></div>
  </div>
  <div class="line grid-line off" id="flowGrid">
   <i class="square"></i><i class="square"></i><i class="square"></i>
  </div>

  <div class="device battery-device">
   <div class="icon" aria-hidden="true">
    <svg viewBox="0 0 64 64">
     <rect x="24" y="4" width="16" height="5" rx="2" fill="#52677c"/>
     <rect x="16" y="9" width="32" height="52" rx="5" fill="#59bb68"/>
     <rect x="21" y="17" width="22" height="38" rx="2" fill="#aeed73"/>
     <path d="M32 24v13m-6-7h12" stroke="white" stroke-width="3"/>
    </svg>
   </div>
   <div class="card">Battery<br><span id="batteryPercent">0</span>% ·
    <span id="batteryPower">0</span> W<br>
    <small><span id="batteryVoltage">0</span> V</small></div>
  </div>
  <div class="line battery-line off" id="flowBattery">
   <i class="square"></i><i class="square"></i><i class="square"></i>
  </div>

  <div class="inverter">
   <div class="icon" aria-hidden="true">▣</div>
   <div class="card">Anenji
    <div class="diagnostic" id="mode">Режим: ожидание данных</div>
    <div class="diagnostic" id="faults">Неполадок нет</div>
    <div class="diagnostic" id="warnings">Предупреждений нет</div>
   </div>
  </div>

  <div class="line pv-line off reverse" id="flowPv">
   <i class="square"></i><i class="square"></i><i class="square"></i>
  </div>
  <div class="device pv-device">
   <div class="icon" aria-hidden="true">☀️</div>
   <div class="card">PV<br><span id="pvPower">0</span> W<br>
    <small><span id="pvVoltage">0</span> V</small></div>
  </div>

  <div class="line load-line off" id="flowLoad">
   <i class="square"></i><i class="square"></i><i class="square"></i>
  </div>
  <div class="device load-device">
   <div class="icon" aria-hidden="true">🏠</div>
   <div class="card">Load<br><span id="loadPower">0</span> W<br>
    <small><span id="loadVoltage">0</span> V</small></div>
  </div>
 </div>
</section>

<section class="block">
 <h2>Total energy — <span id="energyDate">—</span></h2>
 <div class="tiles">
  <div class="tile" style="--color:var(--grid)">Grid<br><b id="gridKwh">0</b> kWh</div>
  <div class="tile" style="--color:var(--pv)">PV<br><b id="pvKwh">0</b> kWh</div>
  <div class="tile" style="--color:var(--load)">Load<br><b id="loadKwh">0</b> kWh</div>
  <div class="tile" style="--color:var(--charge)">Battery charge<br>
   <b id="chargeKwh">0</b> kWh</div>
  <div class="tile" style="--color:var(--discharge)">Battery discharge<br>
   <b id="dischargeKwh">0</b> kWh</div>
 </div>
 <div class="notice">Итоги за текущий календарный день, включая текущий час.</div>
</section>

<section class="block">
 <h2>Мгновенная мощность — текущий день</h2>
 <canvas id="dayChart" class="chart"></canvas>
 <div class="checks" id="dayChecks">
  <label style="--color:var(--grid)"><input id="dayGrid" type="checkbox" checked> Grid</label>
  <label style="--color:var(--pv)"><input id="dayPv" type="checkbox" checked> PV</label>
  <label style="--color:var(--load)"><input id="dayLoad" type="checkbox" checked> Load</label>
  <label style="--color:var(--charge)"><input id="dayCharge" type="checkbox" checked> Battery charge</label>
  <label style="--color:var(--discharge)"><input id="dayDischarge" type="checkbox" checked> Battery discharge</label>
 </div>
</section>

<section class="block">
 <h2>История накопленной энергии</h2>
 <div class="controls">
  <button data-scale="hour">Часы</button>
  <button data-scale="day">Дни</button>
  <button data-scale="month">Месяцы</button>
  <button data-scale="year">Годы</button>
  <button id="prevPeriod" title="Предыдущий период">◀</button>
  <span class="period" id="periodLabel">—</span>
  <button id="nextPeriod" title="Следующий период">▶</button>
  <button id="currentPeriod">Сегодня</button>
 </div>
 <canvas id="historyChart" class="chart"></canvas>
 <div class="checks" id="historyChecks">
  <label style="--color:var(--grid)"><input id="histGrid" type="checkbox" checked> Grid</label>
  <label style="--color:var(--pv)"><input id="histPv" type="checkbox" checked> PV</label>
  <label style="--color:var(--load)"><input id="histLoad" type="checkbox" checked> Load</label>
  <label style="--color:var(--charge)"><input id="histCharge" type="checkbox" checked> Battery charge</label>
  <label style="--color:var(--discharge)"><input id="histDischarge" type="checkbox" checked> Battery discharge</label>
 </div>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
const css=name=>getComputedStyle(document.documentElement)
  .getPropertyValue('--'+name).trim();
const series=[
 {key:'grid',color:'grid',day:'dayGrid',hist:'histGrid'},
 {key:'pv',color:'pv',day:'dayPv',hist:'histPv'},
 {key:'load',color:'load',day:'dayLoad',hist:'histLoad'},
 {key:'charge',color:'charge',day:'dayCharge',hist:'histCharge'},
 {key:'discharge',color:'discharge',day:'dayDischarge',hist:'histDischarge'}
];
let samples=[],hours=[],live=null,scale='hour';
let selected={year:0,month:0,day:0};
const format=(n,d=2)=>Number(n||0).toFixed(d);
const pad=n=>String(n).padStart(2,'0');

function setToday(){
 if(!live || !/^\d{4}-\d\d-\d\d/.test(live.updateTime))return;
 const x=live.updateTime.slice(0,10).split('-').map(Number);
 selected={year:x[0],month:x[1],day:x[2]};
}
function prepare(id){
 const c=$(id),w=Math.max(260,c.clientWidth),h=c.clientHeight;
 const d=window.devicePixelRatio||1;
 c.width=Math.round(w*d);c.height=Math.round(h*d);
 const ctx=c.getContext('2d');ctx.setTransform(d,0,0,d,0,0);
 return {ctx,w,h};
}
function axes(ctx,w,h,max,yUnit,ticks){
 const left=54,right=w-14,top=16,bottom=h-40;
 ctx.font='12px Arial';ctx.lineWidth=1;
 ctx.fillStyle='#516176';ctx.strokeStyle='#dbe2eb';
 ctx.textAlign='right';ctx.textBaseline='middle';
 for(let i=0;i<=ticks;i++){
  const y=bottom-(bottom-top)*i/ticks;
  ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();
  ctx.fillText((max*i/ticks).toFixed(yUnit==='кВт' ? 1 : 2),
    left-7,y);
 }
 ctx.save();ctx.translate(12,(top+bottom)/2);ctx.rotate(-Math.PI/2);
 ctx.textAlign='center';ctx.fillText(yUnit,0,0);ctx.restore();
 return {left,right,top,bottom};
}
function dayChart(){
 const {ctx,w,h}=prepare('dayChart');
 const active=series.filter(s=>$(s.day).checked);
 let peak=0;
 for(const p of samples)for(const s of active)
  peak=Math.max(peak,Number(p[s.key])||0);
 // Шаг вертикальной шкалы строго 0,2 кВт.
 const ticks=Math.max(1,Math.ceil(peak/200));
 const max=ticks*.2;
 const a=axes(ctx,w,h,max,'кВт',ticks);
 ctx.textAlign='center';ctx.textBaseline='top';ctx.fillStyle='#516176';
 for(let hour=0;hour<=24;hour+=w<550?4:2){
  const x=a.left+(a.right-a.left)*hour/24;
  ctx.fillText(pad(hour)+':00',x,a.bottom+8);
 }
 for(const s of active){
  ctx.strokeStyle=css(s.color);ctx.lineWidth=2;ctx.beginPath();
  let started=false;
  for(const p of samples){
   const x=a.left+(a.right-a.left)*Number(p.minute)/1440;
   const y=a.bottom-(a.bottom-a.top)*Number(p[s.key]||0)/(max*1000);
   if(!started){ctx.moveTo(x,y);started=true}else ctx.lineTo(x,y);
  }
  if(started)ctx.stroke();
 }
}
function periodLength(){
 if(scale==='hour')return 24;
 if(scale==='day')return new Date(Date.UTC(
   selected.year,selected.month,0)).getUTCDate();
 if(scale==='month')return 12;
 const years=hours.map(x=>Number(x.year)).filter(Boolean);
 return Math.max(1,(years.length?Math.max(...years):selected.year)-
   (years.length?Math.min(...years):selected.year)+1);
}
function periodStartYear(){
 const years=hours.map(x=>Number(x.year)).filter(Boolean);
 return years.length?Math.min(...years):selected.year;
}
function periodText(){
 if(!selected.year)return '—';
 if(scale==='hour')return `${pad(selected.day)}.${pad(selected.month)}.${selected.year}`;
 if(scale==='day')return `${pad(selected.month)}.${selected.year}`;
 if(scale==='month')return String(selected.year);
 return 'Все доступные годы';
}
function columns(){
 const count=periodLength();
 const out=Array.from({length:count},()=>({
  grid:0,pv:0,load:0,charge:0,discharge:0
 }));
 for(const p of hours){
  const y=Number(p.year),m=Number(p.month),d=Number(p.day);
  let index=-1;
  if(scale==='hour'&&y===selected.year&&m===selected.month&&d===selected.day)
   index=Number(p.hour);
  if(scale==='day'&&y===selected.year&&m===selected.month)index=d-1;
  if(scale==='month'&&y===selected.year)index=m-1;
  if(scale==='year')index=y-periodStartYear();
  if(index>=0&&index<count)for(const s of series)
   out[index][s.key]+=Number(p[s.key])||0;
 }
 return out;
}
function historyChart(){
 const {ctx,w,h}=prepare('historyChart');
 $('periodLabel').textContent=periodText();
 document.querySelectorAll('[data-scale]').forEach(
  b=>b.classList.toggle('active',b.dataset.scale===scale));
 if(!selected.year){axes(ctx,w,h,1,'кВт·ч',5);return}
 const data=columns(),active=series.filter(s=>$(s.hist).checked);
 let peak=0;
 for(const p of data)for(const s of active)peak=Math.max(peak,p[s.key]);
 const max=Math.max(.2,Math.ceil(peak*5)/5);
 const a=axes(ctx,w,h,max,'кВт·ч',5);
 const gw=(a.right-a.left)/data.length;
 const bar=Math.max(1,Math.min(17,(gw-2)/Math.max(1,active.length)));
 data.forEach((p,i)=>{
  active.forEach((s,j)=>{
   const value=p[s.key],height=(a.bottom-a.top)*value/max;
   ctx.fillStyle=css(s.color);
   ctx.fillRect(a.left+i*gw+(gw-bar*active.length)/2+j*bar,
     a.bottom-height,Math.max(.7,bar-.7),height);
  });
 });
 ctx.fillStyle='#516176';ctx.font='11px Arial';
 ctx.textAlign='center';ctx.textBaseline='top';
 const every=scale==='hour'? (w<650?4:2):
   scale==='day'? (w<650?5:2):1;
 data.forEach((p,i)=>{
  if(i%every && i!==data.length-1)return;
  let label=scale==='hour'?pad(i)+':00':
    scale==='day'?String(i+1):
    scale==='month'?pad(i+1):String(periodStartYear()+i);
  ctx.fillText(label,a.left+(i+.5)*gw,a.bottom+8);
 });
}
function updateTotals(){
 if(!live || !selected.year)return;
 const date=live.updateTime.slice(0,10);
 $('energyDate').textContent=/^\d{4}-\d\d-\d\d$/.test(date)?date:'—';
 const day=hours.filter(p=>p.year===Number(date.slice(0,4)) &&
   p.month===Number(date.slice(5,7)) && p.day===Number(date.slice(8,10)));
 for(const [key,id] of [['grid','gridKwh'],['pv','pvKwh'],
   ['load','loadKwh'],['charge','chargeKwh'],['discharge','dischargeKwh']])
  $(id).textContent=format(day.reduce((a,p)=>a+(Number(p[key])||0),0),3);
}
function updateFlows(d){
 const flow=(id,on,color,reverse)=>{
  const el=$(id);el.classList.toggle('off',!on);
  el.classList.toggle('reverse',reverse);
  if(color)el.style.setProperty('--color',css(color));
 };
 flow('flowGrid',d.gridPower>0,'grid',false);
 flow('flowPv',d.pvPower>0,'pv',true);
 flow('flowLoad',d.loadPower>0,'load',false);
 const charge=d.batteryChargePower>0,discharge=d.batteryDischargePower>0;
 flow('flowBattery',charge||discharge,
   discharge?'discharge':'charge',charge);
}
async function json(url){
 const r=await fetch(url,{cache:'no-store'});
 if(!r.ok)throw Error('HTTP '+r.status);
 return r.json();
}
async function refresh(){
 try{
  const d=await json('/api/live');live=d;
  $('status').textContent=d.status+
    (d.updateTime&&d.updateTime!=='-'?' — '+d.updateTime:'');
  $('inverterIp').textContent=d.inverterIp;
  for(const key of ['gridPower','pvPower','loadPower','batteryPower',
    'gridVoltage','pvVoltage','loadVoltage','batteryVoltage'])
   $(key).textContent=format(d[key],key.includes('Voltage')?1:0);
  $('batteryPercent').textContent=Number(d.batteryPercent||0);
  $('mode').textContent='Режим: '+(d.mode||'ожидание данных');
  $('faults').textContent=d.faults||'Неполадок нет';
  $('warnings').textContent=d.warnings||'Предупреждений нет';
  updateFlows(d);
  if(!selected.year)setToday();
  const [points,history]=await Promise.all([
    json('/api/samples'),json('/api/history?scale=hour')
  ]);
  samples=points;hours=history;
  // Последнее измерение показывается и между пятиминутными отсчётами.
  if(d.valid && /^\d{4}-\d\d-\d\d \d\d:\d\d/.test(d.updateTime)){
   const minute=Number(d.updateTime.slice(11,13))*60+
     Number(d.updateTime.slice(14,16));
   const p={minute,grid:d.gridPower,pv:d.pvPower,load:d.loadPower,
     charge:d.batteryChargePower,discharge:d.batteryDischargePower};
   if(samples.length && samples[samples.length-1].minute===minute)
    samples[samples.length-1]=p;
   else samples.push(p);
  }
  dayChart();historyChart();updateTotals();
 }catch(e){console.error(e)}
}
function shiftPeriod(direction){
 if(!selected.year)return;
 if(scale==='hour'){
  const x=new Date(Date.UTC(selected.year,selected.month-1,
    selected.day+direction));
  selected={year:x.getUTCFullYear(),month:x.getUTCMonth()+1,day:x.getUTCDate()};
 }else if(scale==='day'){
  const x=new Date(Date.UTC(selected.year,selected.month-1+direction,1));
  selected.year=x.getUTCFullYear();selected.month=x.getUTCMonth()+1;
  selected.day=1;
 }else if(scale==='month')selected.year+=direction;
 else return;
 historyChart();
}
document.querySelectorAll('[data-scale]').forEach(b=>
 b.addEventListener('click',()=>{scale=b.dataset.scale;historyChart()}));
$('prevPeriod').onclick=()=>shiftPeriod(-1);
$('nextPeriod').onclick=()=>shiftPeriod(1);
$('currentPeriod').onclick=()=>{setToday();historyChart()};
document.querySelectorAll('input[type=checkbox]').forEach(el=>
 el.addEventListener('change',()=>{dayChart();historyChart()}));
window.addEventListener('resize',()=>{dayChart();historyChart()});
refresh();
setInterval(refresh,WEB_INTERVAL);
</script></body></html>
)WEB");

  h.replace("WEB_INTERVAL", String(webSettings.pageUpdateSeconds * 1000UL));
  return h;
}

void webHandleLive() {
  WebLiveData d = {};
  if (webLiveCallback) webLiveCallback(d);

  String j;
  j.reserve(1400);
  j += '{';
  j += F("\"valid\":"); j += d.valid ? F("true") : F("false");
  j += F(",\"status\":\""); j += webJsonEscape(d.status);
  j += F("\",\"updateTime\":\""); j += webJsonEscape(d.updateTime);
  j += F("\",\"inverterIp\":\""); j += webJsonEscape(d.inverterIp);
  j += F("\",\"mode\":\""); j += webJsonEscape(d.mode);
  j += F("\",\"faults\":\""); j += webJsonEscape(d.faults);
  j += F("\",\"warnings\":\""); j += webJsonEscape(d.warnings);
  j += F("\",\"gridPower\":"); j += String(d.gridPower, 2);
  j += F(",\"pvPower\":"); j += String(d.pvPower, 2);
  j += F(",\"loadPower\":"); j += String(d.loadPower, 2);
  j += F(",\"batteryPower\":"); j += String(d.batteryPower, 2);
  j += F(",\"batteryChargePower\":"); j += String(d.batteryChargePower, 2);
  j += F(",\"batteryDischargePower\":"); j += String(d.batteryDischargePower, 2);
  j += F(",\"gridVoltage\":"); j += String(d.gridVoltage, 2);
  j += F(",\"pvVoltage\":"); j += String(d.pvVoltage, 2);
  j += F(",\"loadVoltage\":"); j += String(d.loadVoltage, 2);
  j += F(",\"batteryVoltage\":"); j += String(d.batteryVoltage, 2);
  j += F(",\"batteryPercent\":"); j += String(d.batteryPercent);
  j += F(",\"gridKwh\":"); j += String(d.gridKwh, 3);
  j += F(",\"pvKwh\":"); j += String(d.pvKwh, 3);
  j += F(",\"loadKwh\":"); j += String(d.loadKwh, 3);
  j += F(",\"batteryChargeKwh\":"); j += String(d.batteryChargeKwh, 3);
  j += F(",\"batteryDischargeKwh\":"); j += String(d.batteryDischargeKwh, 3);
  j += '}';
  anenjiWebServer.send(200, "application/json; charset=utf-8", j);
}

void webHandleSamples() {
  static WebSample points[288];
  size_t count=webSamplesCallback?webSamplesCallback(points,288):0;
  String j;
  j.reserve(23000);
  j='[';
  for(size_t i=0;i<count;i++){
    if(i)j+=',';
    j+=F("{\"minute\":");j+=String(points[i].minuteOfDay);
    j+=F(",\"grid\":");j+=String(points[i].gridPower,2);
    j+=F(",\"pv\":");j+=String(points[i].pvPower,2);
    j+=F(",\"load\":");j+=String(points[i].loadPower,2);
    j+=F(",\"charge\":");j+=String(points[i].batteryChargePower,2);
    j+=F(",\"discharge\":");j+=String(points[i].batteryDischargePower,2);
    j+='}';
  }
  j+=']';
  anenjiWebServer.send(200,"application/json; charset=utf-8",j);
}

void webHandleHistory() {
  // Передаём датированные часовые записи; группировка выполняется браузером.
  static WebHistoryHour history[400];
  size_t count=webHistoryCallback?
    webHistoryCallback(history,400,"hour"):0;
  String j;
  j.reserve(37000);
  j='[';
  for(size_t i=0;i<count;i++){
    if(i)j+=',';
    j+=F("{\"year\":");j+=String(history[i].year);
    j+=F(",\"month\":");j+=String(history[i].month);
    j+=F(",\"day\":");j+=String(history[i].day);
    j+=F(",\"hour\":");j+=String(history[i].hour);
    j+=F(",\"grid\":");j+=String(history[i].gridKwh,3);
    j+=F(",\"pv\":");j+=String(history[i].pvKwh,3);
    j+=F(",\"load\":");j+=String(history[i].loadKwh,3);
    j+=F(",\"charge\":");j+=String(history[i].batteryChargeKwh,3);
    j+=F(",\"discharge\":");j+=String(history[i].batteryDischargeKwh,3);
    j+='}';
    if(i%25==0)delay(1);
  }
  j+=']';
  anenjiWebServer.send(200,"application/json; charset=utf-8",j);
}

void webHandleSave() {
  WebSettings s=webSettings;
  s.ssid=anenjiWebServer.arg("ssid");
  s.password=anenjiWebServer.arg("password");
  s.inverterIp=anenjiWebServer.arg("inverterIp");
  s.pageUpdateSeconds=static_cast<uint32_t>(constrain(
    anenjiWebServer.arg("pageUpdate").toInt(),1L,3600L));
  s.registerIntervalSeconds=static_cast<uint32_t>(constrain(
    anenjiWebServer.arg("registerInterval").toInt(),5L,3600L));
  s.timezone=static_cast<int8_t>(constrain(
    anenjiWebServer.arg("timezone").toInt(),-12L,14L));
  s.language=anenjiWebServer.arg("language")=="ru"?"ru":"en";

  IPAddress ip;
  if(s.ssid.isEmpty() || !ip.fromString(s.inverterIp)){
    anenjiWebServer.send(400,"text/plain; charset=utf-8",
      "Укажите имя Wi-Fi и правильный IPv4-адрес инвертора.");
    return;
  }
  webSettings=s;
  if(webSettingsCallback)webSettingsCallback(webSettings);
  anenjiWebServer.send(200,"text/html; charset=utf-8",
    "<!doctype html><meta charset='utf-8'>"
    "<h2>Настройки сохранены. Перезагрузка...</h2>");
  delay(1000);
  ESP.restart();
}

void beginWebServer(
  WebLiveCallback liveCallback,
  WebSamplesCallback samplesCallback,
  WebHistoryCallback historyCallback,
  WebSettingsCallback settingsCallback,
  const WebSettings& initialSettings,
  bool setupMode=false
) {
  webLiveCallback=liveCallback;
  webSamplesCallback=samplesCallback;
  webHistoryCallback=historyCallback;
  webSettingsCallback=settingsCallback;
  webSettings=initialSettings;
  webSetupMode=setupMode;

  anenjiWebServer.on("/",HTTP_GET,[](){
    if(webSetupMode){
      anenjiWebServer.sendHeader("Location","/settings");
      anenjiWebServer.send(302,"text/plain","");
    }else{
      anenjiWebServer.send(200,"text/html; charset=utf-8",webDashboardPage());
    }
  });
  anenjiWebServer.on("/settings",HTTP_GET,[](){
    anenjiWebServer.send(200,"text/html; charset=utf-8",webSettingsPage());
  });
  anenjiWebServer.on("/save",HTTP_POST,webHandleSave);
  anenjiWebServer.on("/api/live",HTTP_GET,webHandleLive);
  anenjiWebServer.on("/api/samples",HTTP_GET,webHandleSamples);
  anenjiWebServer.on("/api/history",HTTP_GET,webHandleHistory);
  anenjiWebServer.begin();
}

void handleWebServer() {
  anenjiWebServer.handleClient();
}

#endif