#include "WebUi.h"

#include <errno.h>
#include <stdlib.h>
#include <cmath>

#include <ArduinoJson.h>

namespace sharp {

namespace {

static const char kRootHtml[] PROGMEM = R"HTML(
<!doctype html><html lang="cs"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>SEN66 panel</title>
<style>
body{font-family:Arial,sans-serif;margin:0;background:#f3f5f7;color:#222}header{background:#0f172a;color:#fff;padding:12px 16px}main{padding:16px;max-width:980px;margin:0 auto}
.tabs{display:flex;gap:8px;margin-bottom:12px}.tab{padding:10px 14px;border:0;border-radius:8px;background:#dbe2ea;cursor:pointer}.tab.active{background:#2563eb;color:#fff}
.panel{display:none;background:#fff;padding:16px;border-radius:10px;box-shadow:0 1px 3px rgba(0,0,0,.15)}.panel.active{display:block}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px}.card{border:1px solid #e5e7eb;border-radius:8px;padding:10px}
label{display:block;font-size:.9rem;margin-top:8px}input,select{width:100%;padding:8px;border:1px solid #cbd5e1;border-radius:6px;background:#fff}
button.save,button.secondary,button.warn{margin-top:12px;padding:10px 14px;color:#fff;border:0;border-radius:8px;cursor:pointer}
button.save{background:#16a34a}button.secondary{background:#2563eb}button.warn{background:#b91c1c}
.muted{color:#666;font-size:.85rem}.ok{color:#166534}.err{color:#b91c1c}code.url{display:block;padding:8px;background:#f1f5f9;border-radius:6px;word-break:break-all}
</style></head><body><header><h2>SEN66 MQTT displej</h2></header><main>
<div class="tabs"><button class="tab active" data-tab="data">Aktuální data</button><button class="tab" data-tab="cfg">Konfigurace</button></div>
<section id="data" class="panel active"><div class="grid" id="cards"></div><p class="muted" id="status"></p></section>
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
<label>Graph veličina<select name="displayGraphMetric"><option value="0">CO2</option><option value="1">PM2.5</option><option value="2">Teplota</option><option value="3">Vlhkost</option><option value="4">VOC</option><option value="5">NOx</option></select></label>
<label>Graph rozsah<select name="displayGraphRange"><option value="0">24 hodin</option><option value="1">7 dní</option></select></label>
<p class="muted">Bez tlačítka můžeš nechat dashboard, fixní graph screen, nebo zapnout automatické cyklování.</p><button id="displayApplyBtn" class="secondary" type="button">Použít zobrazení dočasně</button><button id="displayResetBtn" class="secondary" type="button">Vrátit uložené zobrazení</button><p id="displayMsg" class="muted"></p>
<h3>Intervaly (ms)</h3><label>Překreslení displeje<input type="number" min="500" name="displayRefreshInterval" required></label><label>Auto-cycle displeje<input type="number" min="2000" name="displayCycleInterval" required></label><label>MQTT publish<input type="number" min="1000" name="mqttPublishInterval" required></label><label>TMEP request interval<input type="number" min="1000" name="tmepRequestInterval" required></label><label>MQTT warmup delay<input type="number" min="1000" name="mqttWarmupDelay" required></label><label>Temperature offset<input type="number" step="0.1" name="temperatureOffset" required></label><p class="muted">hodnota, kterou přičíst k naměřené teplotě</p>
<button class="save" type="submit">Uložit plnou konfiguraci</button><p id="cfgMsg" class="muted"></p></form></section></main>
<script>
const tabs=document.querySelectorAll('.tab');tabs.forEach(t=>t.onclick=()=>{tabs.forEach(x=>x.classList.remove('active'));document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));t.classList.add('active');document.getElementById(t.dataset.tab).classList.add('active')});
function setMsg(id,text,ok){const m=document.getElementById(id);m.textContent=text;m.className=ok?'ok':'err'}
async function loadData(){const r=await fetch('/api/data');const d=await r.json();const cards=document.getElementById('cards');cards.innerHTML='';for(const [k,v] of Object.entries(d.values)){const c=document.createElement('div');c.className='card';c.innerHTML=`<strong>${k}</strong><div>${v}</div>`;cards.appendChild(c)}
document.getElementById('status').textContent=`WiFi: ${d.wifi} | režim: ${d.wifiMode} | displej: ${d.displayMode}/${d.currentView}${d.currentView==='graph'?(' '+d.currentMetric+' '+d.currentRange):''}${d.displayTemporary?' (dočasně)':''} | TMEP: ${d.tmepStatus} | MQTT: ${d.mqtt} | validní data: ${d.valid} | uptime: ${d.uptime}s`;
document.getElementById('wifiMode').textContent=`Režim: ${d.wifiMode} ${d.apSsid?('| AP: '+d.apSsid+' @ '+d.apIp):''}`;
document.getElementById('wifiConn').textContent=`Aktuální SSID: ${d.currentSsid||'-'} | IP: ${d.currentIp||'-'} | RSSI: ${d.rssi||'-'} dBm`;
const tmepUrlEl=document.getElementById('tmepUrl');tmepUrlEl.textContent=d.tmepUrl||'Není dostupné';tmepUrlEl.className=d.tmepUrl?'url':'url muted'}
async function loadCfg(){const r=await fetch('/api/config');const c=await r.json();const f=document.getElementById('cfgForm');Object.keys(c).forEach(k=>{if(f[k])f[k].value=c[k]})}
function buildConfigPayload(form){const payload=Object.fromEntries(new FormData(form).entries());['mqttPort','displayRotation','displayInvertRequested','displayMode','displayScreen','displayGraphMetric','displayGraphRange','displayRefreshInterval','displayCycleInterval','mqttPublishInterval','tmepRequestInterval','mqttWarmupDelay','temperatureOffset'].forEach(k=>{if(payload[k]!==undefined&&payload[k]!==''&&!Number.isNaN(Number(payload[k])))payload[k]=Number(payload[k])});return payload}
function buildDisplayPayload(form){const payload=buildConfigPayload(form);return{displayMode:payload.displayMode,displayScreen:payload.displayScreen,displayGraphMetric:payload.displayGraphMetric,displayGraphRange:payload.displayGraphRange}}
document.getElementById('showPass').onchange=(e)=>{document.getElementById('wifiPass').type=e.target.checked?'text':'password'};
document.getElementById('cfgForm').onsubmit=async(e)=>{e.preventDefault();const f=e.target;const payload=buildConfigPayload(f);const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});setMsg('cfgMsg',await r.text(),r.ok)};
document.getElementById('displayApplyBtn').onclick=async()=>{const f=document.getElementById('cfgForm');const r=await fetch('/api/display/runtime',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(buildDisplayPayload(f))});setMsg('displayMsg',await r.text(),r.ok);await loadData()};
document.getElementById('displayResetBtn').onclick=async()=>{const r=await fetch('/api/display/runtime',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({reset:true})});setMsg('displayMsg',await r.text(),r.ok);await loadData()};
document.getElementById('wifiReconnectBtn').onclick=async()=>{const f=document.getElementById('cfgForm');const payload={wifiSsid:f.wifiSsid.value,wifiPassword:f.wifiPassword.value};const r=await fetch('/api/wifi/reconnect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});const d=await r.json();setMsg('wifiMsg',d.message||'?',r.ok)};
document.getElementById('wifiForgetBtn').onclick=async()=>{const r=await fetch('/api/wifi/forget',{method:'POST'});const d=await r.json();setMsg('wifiMsg',d.message||'?',r.ok);};
document.getElementById('tmepSendBtn').onclick=async()=>{const r=await fetch('/api/tmep/send',{method:'POST'});setMsg('tmepMsg',await r.text(),r.ok);await loadData()};
loadData();loadCfg();setInterval(loadData,2000);
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
  if (displayConfig.displayGraphMetric > 5) return false;
  if (displayConfig.displayGraphRange > 1) return false;
  return true;
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
