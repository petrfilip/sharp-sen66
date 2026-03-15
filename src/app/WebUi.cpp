#include "WebUi.h"

#include <errno.h>
#include <stdlib.h>
#include <cmath>

#include <ArduinoJson.h>

namespace sharp {

namespace {

constexpr size_t kRange24HSlots = 1440U;
constexpr size_t kRange7DSlots = 672U;
constexpr size_t kDefaultHistoryBins = 220U;
constexpr size_t kMinHistoryBins = 48U;
constexpr size_t kMaxHistoryBins = 320U;

static const char kRootHtml[] PROGMEM = R"HTML(
<!doctype html><html lang="cs"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>SEN66 panel</title>
<style>
body{font-family:Arial,sans-serif;margin:0;background:#f3f5f7;color:#222}header{background:#0f172a;color:#fff;padding:12px 16px}main{padding:16px;max-width:1120px;margin:0 auto}
.tabs{display:flex;gap:8px;margin-bottom:12px;flex-wrap:wrap}.tab{padding:10px 14px;border:0;border-radius:8px;background:#dbe2ea;cursor:pointer}.tab.active{background:#2563eb;color:#fff}
.panel{display:none;background:#fff;padding:16px;border-radius:10px;box-shadow:0 1px 3px rgba(0,0,0,.15)}.panel.active{display:block}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}.card{border:1px solid #e5e7eb;border-radius:8px;padding:10px}
label{display:block;font-size:.9rem;margin-top:8px}input,select{width:100%;padding:8px;border:1px solid #cbd5e1;border-radius:6px;background:#fff}
button.save,button.secondary,button.warn{margin-top:12px;padding:10px 14px;color:#fff;border:0;border-radius:8px;cursor:pointer}
button.save{background:#16a34a}button.secondary{background:#2563eb}button.warn{background:#b91c1c}
.muted{color:#666;font-size:.85rem}.ok{color:#166534}.err{color:#b91c1c}code.url{display:block;padding:8px;background:#f1f5f9;border-radius:6px;word-break:break-all}
.hidden{display:none!important}.stack{display:flex;flex-direction:column;gap:12px}.history-toolbar{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;align-items:end}.mode-switch{display:flex;gap:8px;flex-wrap:wrap}.mode-btn{margin-top:8px;padding:10px 14px;border:1px solid #cbd5e1;border-radius:8px;background:#f8fafc;cursor:pointer}.mode-btn.active{background:#0f766e;color:#fff;border-color:#0f766e}
.metric-toggle-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:8px}.metric-toggle{display:flex;align-items:center;gap:8px;padding:8px 10px;border:1px solid #e2e8f0;border-radius:8px;background:#f8fafc}.metric-toggle input{width:auto;margin:0}.swatch{width:10px;height:10px;border-radius:999px;display:inline-block}
.history-shell{display:flex;flex-direction:column;gap:12px}.history-summary{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px}.summary-card{border:1px solid #e2e8f0;border-radius:10px;padding:10px;background:#f8fafc}.summary-card strong{display:block;margin-bottom:6px}.summary-card .meta{font-size:.82rem;color:#475569}
.history-chart{border:1px solid #e2e8f0;border-radius:12px;background:linear-gradient(180deg,#fbfdff 0%,#f8fafc 100%);padding:12px;min-height:320px;overflow:hidden}.history-chart svg{width:100%;height:auto;display:block}.history-empty{display:flex;align-items:center;justify-content:center;min-height:280px;color:#64748b;text-align:center;padding:24px}
.history-legend{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}.legend-chip{display:inline-flex;align-items:center;gap:8px;padding:6px 10px;border-radius:999px;background:#f1f5f9;font-size:.82rem}.history-note{margin:0}
@media (max-width:700px){main{padding:12px}.history-chart{padding:8px;min-height:260px}}
</style></head><body><header><h2>SEN66 MQTT displej</h2></header><main>
<div class="tabs"><button class="tab active" data-tab="data">Aktuální data</button><button class="tab" data-tab="history">Historie</button><button class="tab" data-tab="cfg">Konfigurace</button></div>
<section id="data" class="panel active"><div class="grid" id="cards"></div><p class="muted" id="status"></p></section>
<section id="history" class="panel"><div class="stack">
<div class="history-toolbar">
<div><strong>Režim grafu</strong><div class="mode-switch"><button id="historyModeSingle" class="mode-btn active" type="button">Single</button><button id="historyModeCompare" class="mode-btn" type="button">Compare</button></div></div>
<label>Rozsah<select id="historyRange"><option value="0">24 hodin</option><option value="1">7 dní</option></select></label>
<label id="historySingleWrap">Metrika<select id="historyMetric"></select></label>
</div>
<div id="historyCompareWrap" class="hidden"><strong>Compare série</strong><div id="historyCompareMetrics" class="metric-toggle-grid"></div></div>
<p id="historyHint" class="muted history-note"></p>
<div class="history-shell">
<div id="historySummary" class="history-summary"></div>
<div id="historyChart" class="history-chart"><div class="history-empty">Načítám historii…</div></div>
</div>
<p id="historyMsg" class="muted"></p>
</div></section>
<section id="cfg" class="panel"><form id="cfgForm"><h3>Wi-Fi setup</h3>
<p class="muted" id="wifiMode"></p><p class="muted" id="wifiConn"></p>
<label>SSID<input name="wifiSsid" required></label>
<label>Heslo<input type="password" name="wifiPassword" id="wifiPass"></label>
<label><input id="showPass" type="checkbox" style="width:auto"> Zobrazit heslo</label>
<button id="wifiReconnectBtn" class="secondary" type="button">Reconnect Wi-Fi</button>
<p class="muted">Stejné SSID + heslo: jen reconnect bez zápisu do flash. Změněné údaje: uložit a restartovat zařízení.</p>
<button id="wifiForgetBtn" class="warn" type="button">Zapomenout Wi-Fi</button><p class="muted" id="wifiMsg"></p>
<h3>MQTT</h3><label>Server<input name="mqttServer"></label><label>Port<input type="number" min="1" max="65535" name="mqttPort" required></label><label>Uživatel<input name="mqttUser"></label><label>Heslo<input type="password" name="mqttPassword"></label>
<h3>TMEP.cz</h3><label>Doména pro zasílání hodnot<input name="tmepDomain" placeholder="xxk4sk-g6rxfh"></label><label>Parametry požadavku<input name="tmepParams" placeholder="tempV=*TEMP*&humV=*HUM*&co2=*CO2*"></label>
<p class="muted">Použitelné proměnné: *TEMP*, *HUM*, *PM1*, *PM2*, *PM4*, *PM10*, *VOC*, *NOX*, *CO2*.</p><p class="muted">Reálné URL volané na TMEP.cz:</p><code id="tmepUrl" class="url muted">Není dostupné</code>
<button id="tmepSendBtn" class="secondary" type="button">Odeslat TMEP request ručně</button><p id="tmepMsg" class="muted"></p>
<h3>Displej</h3><label>Rotace (0-3)<input type="number" min="0" max="3" name="displayRotation" required></label><p class="muted">Otáčí celý obraz po 90°. Hodnoty 0, 1, 2, 3 odpovídají 0°, 90°, 180°, 270°; použij, když je obraz vzhůru nohama nebo na bok.</p><label>Inverze (0/1)<input type="number" min="0" max="1" name="displayInvertRequested" required></label>
<label>Režim displeje<select name="displayMode"><option value="0">Ruční volba screenu</option><option value="1">Automatické cyklování</option></select></label>
<label>Vybraný screen<select name="displayScreen"><option value="0">Dashboard</option><option value="1">Graph view</option></select></label>
<label>Graph veličina<select name="displayGraphMetric"><option value="0">CO2</option><option value="1">PM1</option><option value="2">PM2.5</option><option value="3">PM4</option><option value="4">PM10</option><option value="5">Teplota</option><option value="6">Vlhkost</option><option value="7">VOC</option><option value="8">NOx</option></select></label>
<label>Graph rozsah<select name="displayGraphRange"><option value="0">24 hodin</option><option value="1">7 dní</option></select></label>
<p class="muted">Bez tlačítka můžeš nechat dashboard, fixní graph screen, nebo zapnout automatické cyklování.</p><button id="displayApplyBtn" class="secondary" type="button">Použít zobrazení dočasně</button><button id="displayResetBtn" class="secondary" type="button">Vrátit uložené zobrazení</button><p id="displayMsg" class="muted"></p>
<h3>Intervaly (ms)</h3><label>Překreslení displeje<input type="number" min="500" name="displayRefreshInterval" required></label><label>Auto-cycle displeje<input type="number" min="2000" name="displayCycleInterval" required></label><label>MQTT publish<input type="number" min="1000" name="mqttPublishInterval" required></label><label>TMEP request interval<input type="number" min="1000" name="tmepRequestInterval" required></label><label>MQTT warmup delay<input type="number" min="1000" name="mqttWarmupDelay" required></label><label>Temperature offset<input type="number" step="0.1" name="temperatureOffset" required></label><p class="muted">hodnota, kterou přičíst k naměřené teplotě</p>
<button class="save" type="submit">Uložit plnou konfiguraci</button><p id="cfgMsg" class="muted"></p></form></section></main>
<script>
const METRICS=[
  {id:0,key:'co2',label:'CO2',color:'#0f766e'},
  {id:1,key:'pm1',label:'PM1',color:'#2563eb'},
  {id:2,key:'pm25',label:'PM2.5',color:'#ea580c'},
  {id:3,key:'pm4',label:'PM4',color:'#475569'},
  {id:4,key:'pm10',label:'PM10',color:'#dc2626'},
  {id:5,key:'temp',label:'Teplota',color:'#ca8a04'},
  {id:6,key:'hum',label:'Vlhkost',color:'#0891b2'},
  {id:7,key:'voc',label:'VOC',color:'#65a30d'},
  {id:8,key:'nox',label:'NOx',color:'#be123c'}
];
const historyState={mode:'single',range:0,singleMetric:0,compareMetrics:new Set(METRICS.map(m=>m.id)),data:null,revisionKey:'',hoverSlot:null,loadedOnce:false};
const tabs=document.querySelectorAll('.tab');
function currentTab(){const active=document.querySelector('.tab.active');return active?active.dataset.tab:'data'}
function activateTab(tabId){tabs.forEach(x=>x.classList.toggle('active',x.dataset.tab===tabId));document.querySelectorAll('.panel').forEach(p=>p.classList.toggle('active',p.id===tabId));if(tabId==='history'){loadHistory(true)}}
tabs.forEach(t=>t.onclick=()=>activateTab(t.dataset.tab));
function metricMeta(id){return METRICS.find(m=>m.id===Number(id))||METRICS[0]}
function setMsg(id,text,ok){const m=document.getElementById(id);m.textContent=text;m.className=ok===undefined?'muted':(ok?'ok':'err')}
function formatMetricValue(metricId,value,unit){if(value===null||value===undefined||Number.isNaN(value))return `-- ${unit||''}`.trim();const fixed=(metricId===0||metricId===7||metricId===8)?0:1;return `${Number(value).toFixed(fixed)} ${unit}`.trim()}
function escapeHtml(value){return String(value).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;')}
function buildConfigPayload(form){const payload=Object.fromEntries(new FormData(form).entries());['mqttPort','displayRotation','displayInvertRequested','displayMode','displayScreen','displayGraphMetric','displayGraphRange','displayRefreshInterval','displayCycleInterval','mqttPublishInterval','tmepRequestInterval','mqttWarmupDelay','temperatureOffset'].forEach(k=>{if(payload[k]!==undefined&&payload[k]!==''&&!Number.isNaN(Number(payload[k])))payload[k]=Number(payload[k])});return payload}
function buildDisplayPayload(form){const payload=buildConfigPayload(form);return{displayMode:payload.displayMode,displayScreen:payload.displayScreen,displayGraphMetric:payload.displayGraphMetric,displayGraphRange:payload.displayGraphRange}}
function populateHistoryControls(){const metricSelect=document.getElementById('historyMetric');metricSelect.innerHTML=METRICS.map(m=>`<option value="${m.id}">${m.label}</option>`).join('');const compareWrap=document.getElementById('historyCompareMetrics');compareWrap.innerHTML=METRICS.map(m=>`<label class="metric-toggle"><input type="checkbox" data-history-metric="${m.id}" checked><span class="swatch" style="background:${m.color}"></span><span>${m.label}</span></label>`).join('');metricSelect.value=String(historyState.singleMetric);compareWrap.querySelectorAll('input[data-history-metric]').forEach(input=>{input.onchange=()=>{const id=Number(input.dataset.historyMetric);if(input.checked){historyState.compareMetrics.add(id)}else{historyState.compareMetrics.delete(id)}loadHistory(true)}})}
function updateHistoryModeUi(){const single=historyState.mode==='single';document.getElementById('historyModeSingle').classList.toggle('active',single);document.getElementById('historyModeCompare').classList.toggle('active',!single);document.getElementById('historySingleWrap').classList.toggle('hidden',!single);document.getElementById('historyCompareWrap').classList.toggle('hidden',single);document.getElementById('historyHint').textContent=single?'Single: absolutní osa Y v jednotkách vybrané metriky. Hover nebo tap ukáže konkrétní historickou hodnotu.':'Compare: všechny zapnuté série jsou normalizované na 0-100 %, takže graf porovnává trend, ne absolutní jednotky.'}
function initializeHistoryDefaults(snapshot){if(historyState.loadedOnce)return;historyState.singleMetric=Number(snapshot.currentMetricId||0);historyState.range=Number(snapshot.currentRangeId||0);document.getElementById('historyMetric').value=String(historyState.singleMetric);document.getElementById('historyRange').value=String(historyState.range);historyState.loadedOnce=true}
async function loadData(){const r=await fetch('/api/data');const d=await r.json();const cards=document.getElementById('cards');cards.innerHTML='';for(const [k,v] of Object.entries(d.values)){const c=document.createElement('div');c.className='card';c.innerHTML=`<strong>${escapeHtml(k)}</strong><div>${escapeHtml(v)}</div>`;cards.appendChild(c)}
document.getElementById('status').textContent=`WiFi: ${d.wifi} | režim: ${d.wifiMode} | displej: ${d.displayMode}/${d.currentView}${d.currentView==='graph'?(' '+d.currentMetric+' '+d.currentRange):''}${d.displayTemporary?' (dočasně)':''} | TMEP: ${d.tmepStatus} | MQTT: ${d.mqtt} | validní data: ${d.valid} | uptime: ${d.uptime}s`;
document.getElementById('wifiMode').textContent=`Režim: ${d.wifiMode} ${d.apSsid?('| AP: '+d.apSsid+' @ '+d.apIp):''}`;
document.getElementById('wifiConn').textContent=`Aktuální SSID: ${d.currentSsid||'-'} | IP: ${d.currentIp||'-'} | RSSI: ${d.rssi||'-'} dBm`;
const tmepUrlEl=document.getElementById('tmepUrl');tmepUrlEl.textContent=d.tmepUrl||'Není dostupné';tmepUrlEl.className=d.tmepUrl?'url':'url muted';initializeHistoryDefaults(d)}
async function loadCfg(){const r=await fetch('/api/config');const c=await r.json();const f=document.getElementById('cfgForm');Object.keys(c).forEach(k=>{if(f[k])f[k].value=c[k]})}
function selectedHistoryMetrics(){if(historyState.mode==='single')return[historyState.singleMetric];return Array.from(historyState.compareMetrics).sort((a,b)=>a-b)}
function historyBins(){const chart=document.getElementById('historyChart');const width=Math.max(320,Math.floor(chart.clientWidth||720));return Math.min(280,Math.max(120,Math.floor(width/2.2)))}
function historySlotAt(series,index){if(series.renderPointCount<=1)return series.startSlot;const ratio=index/(series.renderPointCount-1);return series.startSlot+(ratio*(series.rawPointCount-1))}
function nearestSeriesIndex(series,targetSlot){if(series.renderPointCount===0)return-1;if(series.renderPointCount===1)return 0;const span=Math.max(1,series.rawPointCount-1);const ratio=(targetSlot-series.startSlot)/span;const approx=Math.round(ratio*(series.renderPointCount-1));if(approx<0)return 0;if(approx>=series.renderPointCount)return series.renderPointCount-1;return approx}
function relativeSlotLabel(slot,range,slotCount){const remaining=Math.max(0,slotCount-1-slot);if(remaining===0)return'teď';if(range===0){if(remaining>=60){const hours=remaining/60;return `-${hours>=10?hours.toFixed(0):hours.toFixed(1)}h`}return `-${remaining}m`}const hours=(remaining*15)/60;if(hours>=24){const days=hours/24;return `-${days>=3?days.toFixed(1):days.toFixed(2)}d`}return `-${hours>=10?hours.toFixed(0):hours.toFixed(1)}h`}
function renderHistorySummary(response){const summary=document.getElementById('historySummary');if(!response||!response.series||!response.series.length){summary.innerHTML='';return}const hoverSlot=historyState.hoverSlot;summary.innerHTML=response.series.map(series=>{const meta=metricMeta(series.metric);const hoverIndex=hoverSlot===null?-1:nearestSeriesIndex(series,hoverSlot);const hoverValue=hoverIndex>=0&&hoverIndex<series.points.length?series.points[hoverIndex]:null;const hoverLabel=hoverIndex>=0?relativeSlotLabel(historySlotAt(series,hoverIndex),response.range,response.slotCount):'aktuální';const currentText=series.currentValueValid?formatMetricValue(series.metric,series.currentValue,series.unit):formatMetricValue(series.metric,null,series.unit);const mainValue=hoverValue===null?currentText:formatMetricValue(series.metric,hoverValue,series.unit);const detail=hoverValue===null?`Aktuálně ${currentText}`:`${hoverLabel}: ${formatMetricValue(series.metric,hoverValue,series.unit)}`;return `<div class="summary-card"><strong><span class="swatch" style="background:${meta.color};margin-right:8px"></span>${escapeHtml(series.label)}</strong><div>${escapeHtml(mainValue)}</div><div class="meta">${escapeHtml(detail)} | min ${escapeHtml(formatMetricValue(series.metric,series.min,series.unit))} | max ${escapeHtml(formatMetricValue(series.metric,series.max,series.unit))}</div></div>`}).join('')}
function renderHistoryLegend(response){if(!response||!response.series||!response.series.length)return'';return `<div class="history-legend">${response.series.map(series=>{const meta=metricMeta(series.metric);return `<span class="legend-chip"><span class="swatch" style="background:${meta.color}"></span>${escapeHtml(series.label)}</span>`}).join('')}</div>`}
function buildPath(series,response,mode,minY,maxY){if(!series.renderPointCount||!series.points.length)return'';const width=820;const height=320;const left=62;const right=18;const top=18;const bottom=42;const innerWidth=width-left-right;const innerHeight=height-top-bottom;const span=maxY-minY||1;let path='';for(let index=0;index<series.points.length;index+=1){const slot=historySlotAt(series,index);const x=left+((slot/(response.slotCount-1))*innerWidth);let normalized;if(mode==='compare'){const localSpan=series.max-series.min;normalized=localSpan<0.0001?0.5:(series.points[index]-series.min)/localSpan}else{normalized=(series.points[index]-minY)/span}const y=top+innerHeight-(normalized*innerHeight);path+=`${index===0?'M':'L'}${x.toFixed(2)} ${y.toFixed(2)} `}return path.trim()}
function yTickLabels(response,mode,minY,maxY){if(mode==='compare')return['100 %','75 %','50 %','25 %','0 %'];const labels=[];for(let i=0;i<5;i+=1){const value=maxY-((i/4)*(maxY-minY));labels.push(formatMetricValue(response.series[0].metric,value,response.series[0].unit).replace(` ${response.series[0].unit}`,'').trim())}return labels}
function renderHistoryChart(){const container=document.getElementById('historyChart');const response=historyState.data;if(!response||!response.series||!response.series.length){container.innerHTML='<div class="history-empty">Vyber metriku nebo zapni alespoň jednu sérii.</div>';renderHistorySummary(response);return}const seriesWithData=response.series.filter(series=>series.rawPointCount>1&&series.points.length>1);if(!seriesWithData.length){container.innerHTML='<div class="history-empty">Nedostatek dat pro vykreslení historie. Zařízení musí nejdřív nasbírat alespoň dva body v daném rozsahu.</div>';renderHistorySummary(response);return}const width=820;const height=320;const left=62;const right=18;const top=18;const bottom=42;const innerWidth=width-left-right;const innerHeight=height-top-bottom;const mode=historyState.mode;const baseSeries=response.series[0];let minY=baseSeries.min;let maxY=baseSeries.max;if(mode==='single'&&Math.abs(maxY-minY)<0.001){minY-=1;maxY+=1}const yLabels=yTickLabels(response,mode,minY,maxY);const xLabels=response.range===0?['-24h','-12h','teď']:['-7d','-3.5d','teď'];let hoverMarkup='';if(historyState.hoverSlot!==null){const x=left+((historyState.hoverSlot/(response.slotCount-1))*innerWidth);hoverMarkup+=`<line x1="${x.toFixed(2)}" y1="${top}" x2="${x.toFixed(2)}" y2="${top+innerHeight}" stroke="#94a3b8" stroke-dasharray="4 4" stroke-width="1"/>`;response.series.forEach(series=>{const index=nearestSeriesIndex(series,historyState.hoverSlot);if(index<0||index>=series.points.length)return;const slot=historySlotAt(series,index);if(slot<series.startSlot)return;const px=left+((slot/(response.slotCount-1))*innerWidth);const localSpan=series.max-series.min;const normalized=mode==='compare'?(localSpan<0.0001?0.5:(series.points[index]-series.min)/localSpan):((series.points[index]-minY)/(maxY-minY||1));const py=top+innerHeight-(normalized*innerHeight);hoverMarkup+=`<circle cx="${px.toFixed(2)}" cy="${py.toFixed(2)}" r="4" fill="${metricMeta(series.metric).color}" stroke="#fff" stroke-width="2"/>`})}const gridLines=[];for(let i=0;i<5;i+=1){const y=top+((i/4)*innerHeight);gridLines.push(`<line x1="${left}" y1="${y.toFixed(2)}" x2="${left+innerWidth}" y2="${y.toFixed(2)}" stroke="#e2e8f0" stroke-width="1"/>`)}for(let i=0;i<7;i+=1){const x=left+((i/6)*innerWidth);gridLines.push(`<line x1="${x.toFixed(2)}" y1="${top}" x2="${x.toFixed(2)}" y2="${top+innerHeight}" stroke="#f1f5f9" stroke-width="1"/>`)}const paths=seriesWithData.map(series=>`<path d="${buildPath(series,response,mode,minY,maxY)}" fill="none" stroke="${metricMeta(series.metric).color}" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round"/>`).join('');container.innerHTML=`<svg viewBox="0 0 ${width} ${height}" role="img" aria-label="Historický graf"><rect x="${left}" y="${top}" width="${innerWidth}" height="${innerHeight}" rx="12" fill="#fff" stroke="#cbd5e1"/><g>${gridLines.join('')}</g><g>${paths}</g><g>${hoverMarkup}</g><g fill="#475569" font-size="12" font-family="Arial, sans-serif">${yLabels.map((label,index)=>{const y=top+((index/4)*innerHeight);return `<text x="${left-8}" y="${index===0?y+4:y+4}" text-anchor="end">${escapeHtml(label)}</text>`}).join('')}<text x="${left}" y="${height-12}" text-anchor="start">${xLabels[0]}</text><text x="${left+(innerWidth/2)}" y="${height-12}" text-anchor="middle">${xLabels[1]}</text><text x="${left+innerWidth}" y="${height-12}" text-anchor="end">${xLabels[2]}</text></g><rect x="${left}" y="${top}" width="${innerWidth}" height="${innerHeight}" fill="transparent" pointer-events="all"/></svg>${renderHistoryLegend(response)}`;const svg=container.querySelector('svg');if(svg){const updateHover=clientX=>{const rect=svg.getBoundingClientRect();const ratio=Math.min(1,Math.max(0,(clientX-rect.left-(left/width)*rect.width)/(((innerWidth)/width)*rect.width)));historyState.hoverSlot=ratio*(response.slotCount-1);renderHistorySummary(response);renderHistoryChart()};svg.onmouseleave=()=>{historyState.hoverSlot=null;renderHistorySummary(response);renderHistoryChart()};svg.onmousemove=e=>updateHover(e.clientX);svg.ontouchstart=e=>{if(e.touches&&e.touches[0])updateHover(e.touches[0].clientX)};svg.ontouchmove=e=>{if(e.touches&&e.touches[0])updateHover(e.touches[0].clientX)}}renderHistorySummary(response)}
async function loadHistory(force){if(currentTab()!=='history'&&!force)return;const metrics=selectedHistoryMetrics();if(!metrics.length){historyState.data=null;historyState.revisionKey='';historyState.hoverSlot=null;setMsg('historyMsg','Vyber aspoň jednu sérii pro compare režim.');renderHistoryChart();return}const params=new URLSearchParams({range:String(historyState.range),metrics:metrics.join(','),bins:String(historyBins())});const r=await fetch(`/api/history?${params.toString()}`);if(!r.ok){setMsg('historyMsg',await r.text(),false);return}const data=await r.json();if(!force&&data.revisionKey===historyState.revisionKey)return;historyState.data=data;historyState.revisionKey=data.revisionKey||'';historyState.hoverSlot=null;setMsg('historyMsg',`Načteno ${data.series.length} sérií, vzorků pro vykreslení: ${data.series.map(s=>s.renderPointCount).join('/')}.`,true);renderHistoryChart()}
document.getElementById('historyModeSingle').onclick=()=>{historyState.mode='single';updateHistoryModeUi();loadHistory(true)};
document.getElementById('historyModeCompare').onclick=()=>{historyState.mode='compare';updateHistoryModeUi();loadHistory(true)};
document.getElementById('historyRange').onchange=e=>{historyState.range=Number(e.target.value);loadHistory(true)};
document.getElementById('historyMetric').onchange=e=>{historyState.singleMetric=Number(e.target.value);loadHistory(true)};
document.getElementById('showPass').onchange=(e)=>{document.getElementById('wifiPass').type=e.target.checked?'text':'password'};
document.getElementById('cfgForm').onsubmit=async(e)=>{e.preventDefault();const f=e.target;const payload=buildConfigPayload(f);const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});setMsg('cfgMsg',await r.text(),r.ok)};
document.getElementById('displayApplyBtn').onclick=async()=>{const f=document.getElementById('cfgForm');const r=await fetch('/api/display/runtime',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(buildDisplayPayload(f))});setMsg('displayMsg',await r.text(),r.ok);await loadData();if(currentTab()==='history')await loadHistory(true)};
document.getElementById('displayResetBtn').onclick=async()=>{const r=await fetch('/api/display/runtime',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({reset:true})});setMsg('displayMsg',await r.text(),r.ok);await loadData();if(currentTab()==='history')await loadHistory(true)};
document.getElementById('wifiReconnectBtn').onclick=async()=>{const f=document.getElementById('cfgForm');const payload={wifiSsid:f.wifiSsid.value,wifiPassword:f.wifiPassword.value};const r=await fetch('/api/wifi/reconnect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});const d=await r.json();setMsg('wifiMsg',d.message||'?',r.ok)};
document.getElementById('wifiForgetBtn').onclick=async()=>{const r=await fetch('/api/wifi/forget',{method:'POST'});const d=await r.json();setMsg('wifiMsg',d.message||'?',r.ok);};
document.getElementById('tmepSendBtn').onclick=async()=>{const r=await fetch('/api/tmep/send',{method:'POST'});setMsg('tmepMsg',await r.text(),r.ok);await loadData()};
populateHistoryControls();updateHistoryModeUi();loadData();loadCfg();setInterval(()=>{loadData().catch(()=>{});},2000);setInterval(()=>{loadHistory(false).catch(()=>{});},5000);
</script></body></html>)HTML";

void fillDataDocument(JsonDocument& doc, const WebUiDataSnapshot& data) {
  doc["wifi"] = data.wifiStatus;
  doc["mqtt"] = data.mqttStatus;
  doc["valid"] = data.valid;
  doc["displayTemporary"] = data.displayTemporary;
  doc["uptime"] = data.uptimeSeconds;
  doc["tmepUrl"] = data.tmepUrl;
  doc["tmepStatus"] = data.tmepStatus;
  doc["wifiMode"] = data.wifiMode;
  doc["displayMode"] = data.displayMode;
  doc["currentView"] = data.currentView;
  doc["currentMetric"] = data.currentMetric;
  doc["currentRange"] = data.currentRange;
  doc["currentMetricId"] = data.currentMetricId;
  doc["currentRangeId"] = data.currentRangeId;
  doc["apSsid"] = data.apSsid;
  doc["apIp"] = data.apIp;
  doc["currentSsid"] = data.currentSsid;
  doc["currentIp"] = data.currentIp;
  doc["rssi"] = data.rssi;

  JsonObject values = doc["values"].to<JsonObject>();
  values["temperature"] = round(data.temperature * 10.0f) / 10.0f;
  values["humidity"] = round(data.humidity * 10.0f) / 10.0f;
  values["pm1"] = round(data.pm1 * 10.0f) / 10.0f;
  values["pm25"] = round(data.pm25 * 10.0f) / 10.0f;
  values["pm4"] = round(data.pm4 * 10.0f) / 10.0f;
  values["pm10"] = round(data.pm10 * 10.0f) / 10.0f;
  values["voc"] = round(data.voc);
  values["nox"] = round(data.nox);
  values["co2"] = data.co2;
}

void fillConfigDocument(JsonDocument& doc, const AppConfig& config) {
  doc["wifiSsid"] = config.wifiSsid;
  doc["wifiPassword"] = config.wifiPassword;
  doc["mqttServer"] = config.mqttServer;
  doc["mqttPort"] = config.mqttPort;
  doc["mqttUser"] = config.mqttUser;
  doc["mqttPassword"] = config.mqttPassword;
  doc["tmepDomain"] = config.tmepDomain;
  doc["tmepParams"] = config.tmepParams;
  doc["displayRotation"] = config.displayRotation;
  doc["displayInvertRequested"] = config.displayInvertRequested ? 1 : 0;
  doc["displayMode"] = config.displayMode;
  doc["displayScreen"] = config.displayScreen;
  doc["displayGraphMetric"] = config.displayGraphMetric;
  doc["displayGraphRange"] = config.displayGraphRange;
  doc["displayRefreshInterval"] = config.displayRefreshInterval;
  doc["displayCycleInterval"] = config.displayCycleInterval;
  doc["mqttPublishInterval"] = config.mqttPublishInterval;
  doc["tmepRequestInterval"] = config.tmepRequestInterval;
  doc["mqttWarmupDelay"] = config.mqttWarmupDelay;
  doc["temperatureOffset"] = config.temperatureOffset;
}

WebUiDisplayConfig buildDisplayConfig(const AppConfig& config) {
  WebUiDisplayConfig displayConfig;
  displayConfig.displayMode = config.displayMode;
  displayConfig.displayScreen = config.displayScreen;
  displayConfig.displayGraphMetric = config.displayGraphMetric;
  displayConfig.displayGraphRange = config.displayGraphRange;
  return displayConfig;
}

bool validateDisplayConfig(const WebUiDisplayConfig& displayConfig) {
  if (displayConfig.displayMode > 1) return false;
  if (displayConfig.displayScreen > 1) return false;
  if (!airmon::isValidMetricIdValue(displayConfig.displayGraphMetric)) return false;
  if (displayConfig.displayGraphRange > 1) return false;
  return true;
}

size_t historySlotCount(const airmon::HistoryRange range) {
  return range == airmon::HistoryRange::Range24H ? kRange24HSlots : kRange7DSlots;
}

float roundMetricValue(const airmon::MetricId metric, const float value) {
  if (airmon::metricUsesSingleDecimal(metric)) {
    return roundf(value * 10.0f) / 10.0f;
  }
  return roundf(value);
}

bool parseMetricMask(const String& value, uint16_t& outMask) {
  outMask = 0U;
  if (value.length() == 0) {
    return false;
  }

  int start = 0;
  while (start <= value.length()) {
    int end = value.indexOf(',', start);
    if (end < 0) {
      end = value.length();
    }

    String token = value.substring(start, end);
    token.trim();
    if (token.length() == 0) {
      return false;
    }

    char* parseEnd = nullptr;
    errno = 0;
    const long parsed = strtol(token.c_str(), &parseEnd, 10);
    if (errno != 0 || parseEnd == token.c_str() || *parseEnd != '\0' || !airmon::isValidMetricId(parsed)) {
      return false;
    }

    outMask |= static_cast<uint16_t>(1U << static_cast<uint8_t>(parsed));
    if (end == value.length()) {
      break;
    }
    start = end + 1;
  }

  return outMask != 0U;
}

bool parseHistoryRange(const String& value, airmon::HistoryRange& outRange) {
  if (value.length() == 0) {
    return false;
  }

  char* parseEnd = nullptr;
  errno = 0;
  const long parsed = strtol(value.c_str(), &parseEnd, 10);
  if (errno != 0 || parseEnd == value.c_str() || *parseEnd != '\0' || parsed < 0 || parsed > 1) {
    return false;
  }

  outRange = parsed == 0 ? airmon::HistoryRange::Range24H : airmon::HistoryRange::Range7D;
  return true;
}

bool parseHistoryBins(const String& value, size_t& outBins) {
  if (value.length() == 0) {
    outBins = kDefaultHistoryBins;
    return true;
  }

  char* parseEnd = nullptr;
  errno = 0;
  const unsigned long parsed = strtoul(value.c_str(), &parseEnd, 10);
  if (errno != 0 || parseEnd == value.c_str() || *parseEnd != '\0') {
    return false;
  }

  if (parsed < kMinHistoryBins) {
    outBins = kMinHistoryBins;
  } else if (parsed > kMaxHistoryBins) {
    outBins = kMaxHistoryBins;
  } else {
    outBins = static_cast<size_t>(parsed);
  }
  return true;
}

void fillHistorySeriesDocument(JsonObject& seriesDoc,
                               WebUi::Delegate& delegate,
                               const airmon::MetricId metric,
                               const airmon::HistoryRange range,
                               const size_t bins,
                               String& revisionKey) {
  const size_t rawPointCount = delegate.webUiHistoryPointCount(metric, range);
  const size_t slotCount = historySlotCount(range);
  const size_t startSlot = rawPointCount > slotCount ? 0U : (slotCount - rawPointCount);
  const size_t renderPointCount = (rawPointCount == 0U) ? 0U : (rawPointCount < bins ? rawPointCount : bins);
  const uint32_t revision = delegate.webUiHistoryRevision(metric, range);
  bool currentValueValid = false;
  const float currentValue = delegate.webUiLiveMetricValue(metric, currentValueValid);

  seriesDoc["metric"] = static_cast<uint8_t>(metric);
  seriesDoc["label"] = airmon::metricLabel(metric);
  seriesDoc["unit"] = airmon::metricUnit(metric);
  seriesDoc["rawPointCount"] = rawPointCount;
  seriesDoc["renderPointCount"] = renderPointCount;
  seriesDoc["startSlot"] = startSlot;
  seriesDoc["revision"] = revision;
  seriesDoc["currentValueValid"] = currentValueValid;
  seriesDoc["currentValue"] = roundMetricValue(metric, currentValue);

  revisionKey += String(static_cast<unsigned int>(static_cast<uint8_t>(metric)));
  revisionKey += ':';
  revisionKey += String(static_cast<unsigned long>(revision));
  revisionKey += '|';

  if (rawPointCount == 0U) {
    seriesDoc["min"] = 0;
    seriesDoc["max"] = 0;
    seriesDoc["hasData"] = false;
    seriesDoc["points"].to<JsonArray>();
    return;
  }

  float minValue = 0.0f;
  float maxValue = 0.0f;
  float sample = 0.0f;
  if (delegate.webUiHistoryPointAt(metric, range, 0U, sample)) {
    minValue = sample;
    maxValue = sample;
  }
  for (size_t index = 1U; index < rawPointCount; ++index) {
    if (!delegate.webUiHistoryPointAt(metric, range, index, sample)) {
      continue;
    }
    if (sample < minValue) minValue = sample;
    if (sample > maxValue) maxValue = sample;
  }

  seriesDoc["hasData"] = true;
  seriesDoc["min"] = roundMetricValue(metric, minValue);
  seriesDoc["max"] = roundMetricValue(metric, maxValue);

  JsonArray points = seriesDoc["points"].to<JsonArray>();
  for (size_t bucketIndex = 0U; bucketIndex < renderPointCount; ++bucketIndex) {
    const size_t bucketStart = (bucketIndex * rawPointCount) / renderPointCount;
    size_t bucketEnd = ((bucketIndex + 1U) * rawPointCount) / renderPointCount;
    if (bucketEnd <= bucketStart) {
      bucketEnd = bucketStart + 1U;
    }

    float bucketSum = 0.0f;
    size_t bucketSamples = 0U;
    for (size_t index = bucketStart; index < bucketEnd; ++index) {
      if (!delegate.webUiHistoryPointAt(metric, range, index, sample)) {
        continue;
      }
      bucketSum += sample;
      ++bucketSamples;
    }

    const float bucketValue = bucketSamples == 0U ? minValue : (bucketSum / static_cast<float>(bucketSamples));
    points.add(roundMetricValue(metric, bucketValue));
  }
}

void sendJson(WebServer& server, const int statusCode, const JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);
  server.send(statusCode, "application/json", payload);
}

bool readIntField(const JsonDocument& doc, const char* key, int& out) {
  const JsonVariantConst value = doc[key];
  if (value.isNull()) {
    return false;
  }

  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    if (text == nullptr || *text == '\0') {
      return false;
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
      return false;
    }

    out = static_cast<int>(parsed);
    return true;
  }

  out = value.as<int>();
  return true;
}

bool readUnsignedLongField(const JsonDocument& doc, const char* key, unsigned long& out) {
  const JsonVariantConst value = doc[key];
  if (value.isNull()) {
    return false;
  }

  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    if (text == nullptr || *text == '\0' || *text == '-') {
      return false;
    }

    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
      return false;
    }

    out = parsed;
    return true;
  }

  out = value.as<unsigned long>();
  return true;
}

bool readFloatField(const JsonDocument& doc, const char* key, float& out) {
  const JsonVariantConst value = doc[key];
  if (value.isNull()) {
    return false;
  }

  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    if (text == nullptr || *text == '\0') {
      return false;
    }

    char* end = nullptr;
    errno = 0;
    const float parsed = strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(parsed)) {
      return false;
    }

    out = parsed;
    return true;
  }

  out = value.as<float>();
  return isfinite(out);
}

}  // namespace

WebUi::WebUi(WebServer& server, Delegate& delegate) : server_(server), delegate_(delegate) {}

void WebUi::begin() {
  registerRoutes();
  server_.begin();
  Serial.println("WEB: Server bezi na portu 80");
}

void WebUi::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/data", HTTP_GET, [this]() { handleApiData(); });
  server_.on("/api/history", HTTP_GET, [this]() { handleApiHistory(); });
  server_.on("/api/config", HTTP_GET, [this]() { handleApiConfigGet(); });
  server_.on("/api/config", HTTP_POST, [this]() { handleApiConfigPost(); });
  server_.on("/api/display/runtime", HTTP_POST, [this]() { handleApiDisplayRuntimePost(); });
  server_.on("/api/wifi/reconnect", HTTP_POST, [this]() { handleApiWifiReconnect(); });
  server_.on("/api/wifi/save", HTTP_POST, [this]() { handleApiWifiSave(); });
  server_.on("/api/wifi/forget", HTTP_POST, [this]() { handleApiWifiForget(); });
  server_.on("/api/tmep/send", HTTP_POST, [this]() { handleApiTmepSend(); });

  server_.on("/generate_204", HTTP_ANY, [this]() { handleCaptiveRedirect(); });
  server_.on("/hotspot-detect.html", HTTP_ANY, [this]() { handleCaptiveRedirect(); });
  server_.on("/connecttest.txt", HTTP_ANY, [this]() { handleCaptiveRedirect(); });
  server_.on("/ncsi.txt", HTTP_ANY, [this]() { handleCaptiveRedirect(); });
  server_.on("/redirect", HTTP_ANY, [this]() { handleCaptiveRedirect(); });

  server_.onNotFound([this]() {
    if (delegate_.isWebUiCaptiveMode()) {
      handleCaptiveRedirect();
      return;
    }
    server_.send(404, "text/plain", "Not found");
  });
}

void WebUi::maybeRestart(const WebUiActionResult& result) const {
  if (!result.ok || !result.restartRequired) {
    return;
  }

  delay(300);
  ESP.restart();
}

void WebUi::handleRoot() { server_.send_P(200, "text/html; charset=utf-8", kRootHtml); }

void WebUi::handleApiData() {
  JsonDocument doc;
  fillDataDocument(doc, delegate_.buildWebUiData());
  sendJson(server_, 200, doc);
}

void WebUi::handleApiConfigGet() {
  JsonDocument doc;
  fillConfigDocument(doc, delegate_.webUiConfig());
  sendJson(server_, 200, doc);
}

void WebUi::handleApiHistory() {
  airmon::HistoryRange range = airmon::HistoryRange::Range24H;
  if (!parseHistoryRange(server_.arg("range"), range)) {
    server_.send(400, "text/plain", "Neplatny range");
    return;
  }

  uint16_t metricMask = 0U;
  if (!parseMetricMask(server_.arg("metrics"), metricMask)) {
    server_.send(400, "text/plain", "Neplatne metrics");
    return;
  }

  size_t bins = kDefaultHistoryBins;
  if (!parseHistoryBins(server_.arg("bins"), bins)) {
    server_.send(400, "text/plain", "Neplatne bins");
    return;
  }

  JsonDocument doc;
  doc["range"] = static_cast<uint8_t>(range);
  doc["rangeLabel"] = airmon::rangeLabel(range);
  doc["slotCount"] = historySlotCount(range);
  doc["bins"] = bins;

  String revisionKey;
  revisionKey.reserve(64);
  revisionKey += String(static_cast<unsigned int>(static_cast<uint8_t>(range)));
  revisionKey += '|';

  JsonArray seriesArray = doc["series"].to<JsonArray>();
  for (uint8_t metricValue = 0U; metricValue < airmon::kMetricCountU8; ++metricValue) {
    const uint16_t bit = static_cast<uint16_t>(1U << metricValue);
    if ((metricMask & bit) == 0U) {
      continue;
    }

    JsonObject seriesDoc = seriesArray.add<JsonObject>();
    fillHistorySeriesDocument(seriesDoc, delegate_, static_cast<airmon::MetricId>(metricValue), range, bins,
                              revisionKey);
  }

  doc["revisionKey"] = revisionKey;
  sendJson(server_, 200, doc);
}

void WebUi::handleApiConfigPost() {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, server_.arg("plain"));
  if (err) {
    server_.send(400, "text/plain", "Neplatny JSON");
    return;
  }

  AppConfig updated = delegate_.webUiConfig();
  if (doc["wifiSsid"].is<const char*>()) updated.wifiSsid = doc["wifiSsid"].as<String>();
  if (doc["wifiPassword"].is<const char*>()) updated.wifiPassword = doc["wifiPassword"].as<String>();
  const JsonVariantConst mqttServerValue = doc["mqttServer"];
  if (mqttServerValue.is<const char*>()) {
    updated.mqttServer = mqttServerValue.as<String>();
  } else if (!mqttServerValue.isUnbound() && mqttServerValue.isNull()) {
    updated.mqttServer = "";
  }
  if (doc["mqttUser"].is<const char*>()) updated.mqttUser = doc["mqttUser"].as<String>();
  if (doc["mqttPassword"].is<const char*>()) updated.mqttPassword = doc["mqttPassword"].as<String>();
  if (doc["mqttClientId"].is<const char*>()) updated.mqttClientId = doc["mqttClientId"].as<String>();
  if (doc["tmepDomain"].is<const char*>()) updated.tmepDomain = doc["tmepDomain"].as<String>();
  if (doc["tmepParams"].is<const char*>()) updated.tmepParams = doc["tmepParams"].as<String>();

  int intValue = 0;
  unsigned long ulongValue = 0;
  float floatValue = 0.0f;

  if (readIntField(doc, "mqttPort", intValue)) updated.mqttPort = intValue;
  if (readIntField(doc, "displayRotation", intValue)) updated.displayRotation = static_cast<uint8_t>(intValue);
  if (readIntField(doc, "displayInvertRequested", intValue)) updated.displayInvertRequested = intValue == 1;
  if (readIntField(doc, "displayMode", intValue)) updated.displayMode = static_cast<uint8_t>(intValue);
  if (readIntField(doc, "displayScreen", intValue)) updated.displayScreen = static_cast<uint8_t>(intValue);
  if (readIntField(doc, "displayGraphMetric", intValue)) updated.displayGraphMetric = static_cast<uint8_t>(intValue);
  if (readIntField(doc, "displayGraphRange", intValue)) updated.displayGraphRange = static_cast<uint8_t>(intValue);
  if (readUnsignedLongField(doc, "displayRefreshInterval", ulongValue)) {
    updated.displayRefreshInterval = ulongValue;
  }
  if (readUnsignedLongField(doc, "displayCycleInterval", ulongValue)) {
    updated.displayCycleInterval = ulongValue;
  }
  if (readUnsignedLongField(doc, "mqttPublishInterval", ulongValue)) {
    updated.mqttPublishInterval = ulongValue;
  }
  if (readUnsignedLongField(doc, "tmepRequestInterval", ulongValue)) {
    updated.tmepRequestInterval = ulongValue;
  }
  if (readUnsignedLongField(doc, "mqttWarmupDelay", ulongValue)) {
    updated.mqttWarmupDelay = ulongValue;
  }
  if (readFloatField(doc, "temperatureOffset", floatValue)) updated.temperatureOffset = floatValue;

  const WebUiActionResult result = delegate_.applyWebUiConfig(updated);
  server_.send(result.statusCode, "text/plain", result.message);
  maybeRestart(result);
}

void WebUi::handleApiDisplayRuntimePost() {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, server_.arg("plain"));
  if (err) {
    server_.send(400, "text/plain", "Neplatny JSON");
    return;
  }

  WebUiDisplayConfig displayConfig = buildDisplayConfig(delegate_.webUiConfig());
  displayConfig.resetToSaved = doc["reset"] | false;

  if (!displayConfig.resetToSaved) {
    int intValue = 0;
    if (readIntField(doc, "displayMode", intValue)) displayConfig.displayMode = static_cast<uint8_t>(intValue);
    if (readIntField(doc, "displayScreen", intValue)) {
      displayConfig.displayScreen = static_cast<uint8_t>(intValue);
    }
    if (readIntField(doc, "displayGraphMetric", intValue)) {
      displayConfig.displayGraphMetric = static_cast<uint8_t>(intValue);
    }
    if (readIntField(doc, "displayGraphRange", intValue)) {
      displayConfig.displayGraphRange = static_cast<uint8_t>(intValue);
    }

    if (!validateDisplayConfig(displayConfig)) {
      server_.send(400, "text/plain", "Neplatne display hodnoty");
      return;
    }
  }

  const WebUiActionResult result = delegate_.applyWebUiDisplayConfig(displayConfig);
  server_.send(result.statusCode, "text/plain", result.message);
}

void WebUi::handleApiWifiReconnect() {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, server_.arg("plain"));
  if (err) {
    server_.send(400, "application/json", "{\"ok\":false,\"message\":\"Neplatny JSON\"}");
    return;
  }

  const WebUiActionResult result =
      delegate_.reconnectWebUiWifi(doc["wifiSsid"].as<String>(), doc["wifiPassword"].as<String>());

  JsonDocument out;
  out["ok"] = result.ok;
  out["message"] = result.message;
  out["wifiMode"] = result.wifiMode;
  out["restartRequired"] = result.restartRequired;
  sendJson(server_, result.statusCode, out);
  maybeRestart(result);
}

void WebUi::handleApiWifiSave() {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, server_.arg("plain"));
  if (err) {
    server_.send(400, "application/json", "{\"ok\":false,\"message\":\"Neplatny JSON\"}");
    return;
  }

  const WebUiActionResult result =
      delegate_.saveWebUiWifi(doc["wifiSsid"].as<String>(), doc["wifiPassword"].as<String>());

  JsonDocument out;
  out["ok"] = result.ok;
  out["message"] = result.message;
  out["wifiMode"] = result.wifiMode;
  out["restartRequired"] = result.restartRequired;
  sendJson(server_, result.statusCode, out);
  maybeRestart(result);
}

void WebUi::handleApiWifiForget() {
  const WebUiActionResult result = delegate_.forgetWebUiWifi();

  JsonDocument out;
  out["ok"] = result.ok;
  out["message"] = result.message;
  out["wifiMode"] = result.wifiMode;
  out["restartRequired"] = result.restartRequired;
  sendJson(server_, result.statusCode, out);
  maybeRestart(result);
}

void WebUi::handleApiTmepSend() {
  const WebUiActionResult result = delegate_.sendWebUiTmep();
  server_.send(result.statusCode, "text/plain", result.message);
}

void WebUi::handleCaptiveRedirect() {
  if (!delegate_.isWebUiCaptiveMode()) {
    server_.send(404, "text/plain", "Not found");
    return;
  }

  server_.sendHeader("Location", String("http://") + delegate_.webUiCaptiveIp() + "/", true);
  server_.send(302, "text/plain", "Redirecting to captive portal");
}

}  // namespace sharp
