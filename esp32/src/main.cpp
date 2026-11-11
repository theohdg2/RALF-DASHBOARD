#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#if __has_include("wifi_credentials.h")
#include "wifi_credentials.h"
#endif
#ifndef RALF_WIFI_SSID
#define RALF_WIFI_SSID ""
#define RALF_WIFI_PASSWORD ""
#endif

// Static assets and project data use different flash partitions.
fs::LittleFSFS stateFS;
WebServer server(80);
Preferences prefs;
constexpr size_t MAX_PROJECT_BYTES = 65536;
constexpr uint32_t RECORD_MAGIC = 0x52414C46;
struct RecordHeader { uint32_t magic, generation, length; uint8_t digest[32]; };
String editorKey, apPassword, ssid, currentSha, activePath;
uint32_t currentGeneration = 0;
bool ready = false;

String digestHex(const uint8_t *bytes) {
  static const char hex[] = "0123456789abcdef";
  String out; out.reserve(64);
  for (size_t i = 0; i < 32; ++i) { out += hex[bytes[i] >> 4]; out += hex[bytes[i] & 15]; }
  return out;
}
String randomSecret(size_t bytes) {
  String result; result.reserve(bytes * 2);
  for (size_t i = 0; i < bytes; ++i) { char buf[3]; snprintf(buf, sizeof(buf), "%02x", (uint8_t)esp_random()); result += buf; }
  return result;
}
bool constantEqual(const String &a, const String &b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0; for (size_t i = 0; i < a.length(); ++i) diff |= a[i] ^ b[i];
  return diff == 0;
}
void reply(int code, const char *error) {
  JsonDocument doc; doc["error"] = error;
  String body; serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", body);
}
bool authorized() {
  const String origin = server.header("Origin");
  if (origin.length() && origin != "http://" + server.hostHeader()) { Serial.println("API 403: page origin does not match this ESP32."); reply(403, "ORIGIN"); return false; }
  if (!constantEqual(server.header("Authorization"), "Bearer " + editorKey)) { Serial.println("API 401: editor key rejected."); reply(401, "AUTH"); return false; }
  return true;
}
bool validString(JsonVariantConst value, size_t maxLength = 20000) {
  return value.is<const char *>() && strlen(value.as<const char *>()) <= maxLength;
}
bool isOneOf(JsonVariantConst value, std::initializer_list<const char *> options) {
  if (!value.is<const char *>()) return false;
  for (const char *option : options) if (strcmp(value.as<const char *>(), option) == 0) return true;
  return false;
}
bool validConfig(JsonVariantConst c) {
  for (const char *key : {"before", "during", "after", "drop", "contrast"}) {
    if (!c[key].is<double>() || !isfinite(c[key].as<double>()) || c[key].as<double>() <= 0 || c[key].as<double>() > 100) return false;
  }
  for (const char *key : {"start", "end", "stop"}) if (!c[key].is<double>() || !isfinite(c[key].as<double>())) return false;
  const double start = c["start"], end = c["end"], stop = c["stop"];
  return stop > 0 && end - stop > 2 && start - end > 2 && start <= 320 &&
    isOneOf(c["mode"], {"DISTANCE_ONLY", "CONTRAST_OR_DISTANCE"});
}
bool validDate(JsonVariantConst v) {
  if (!v.is<const char *>()) return false;
  const char *s = v.as<const char *>();
  if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') return false;
  for (int i = 0; i < 10; ++i) if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
  int y = atoi(s), m = atoi(s + 5), d = atoi(s + 8);
  if (y < 1 || m < 1 || m > 12) return false;
  const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int maxDays = days[m-1] + (m == 2 && (y % 400 == 0 || (y % 4 == 0 && y % 100 != 0)));
  return d > 0 && d <= maxDays;
}
bool validProject(const String &body) {
  if (body.length() == 0 || body.length() > MAX_PROJECT_BYTES) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::NestingLimit(12))) return false;
  if (!doc["schema"].is<const char *>() || strcmp(doc["schema"], "ralf-project-v1") != 0 || !validConfig(doc["config"])) return false;
  for (const char *key : {"trials", "issues", "tasks"}) if (!doc[key].is<JsonArray>() || doc[key].size() > 1000) return false;
  for (JsonVariantConst t : doc["trials"].as<JsonArrayConst>()) {
    if (!validString(t["title"],300) || !validDate(t["date"]) || !validConfig(t["config"]) ||
        !validString(t["hypothesis"]) || !validString(t["observation"]) || !validString(t["version"],300) ||
        !isOneOf(t["type"], {"Terrain","Simulation"}) || !isOneOf(t["result"], {"À analyser","Conforme","Non conforme","Urgence"})) return false;
  }
  for (JsonVariantConst i : doc["issues"].as<JsonArrayConst>()) {
    if (!validString(i["id"],100) || !validString(i["title"],300) || !validString(i["desc"]) || !validString(i["owner"],300) ||
        !isOneOf(i["p"], {"P1","P2","P3"}) || !isOneOf(i["status"], {"À corriger","À analyser","À tester","À confirmer","En cours","Résolu"})) return false;
  }
  for (JsonVariantConst t : doc["tasks"].as<JsonArrayConst>()) {
    if (!validString(t["title"],300) || !validDate(t["start"]) || !validDate(t["end"]) ||
        strcmp(t["start"],t["end"]) > 0 || !isOneOf(t["status"], {"Prévu","En cours","Terminé","À replanifier"})) return false;
  }
  return true;
}
bool readRecord(const String &path, RecordHeader &header, String *payload = nullptr) {
  File file = stateFS.open(path, "r");
  if (!file || file.read((uint8_t *)&header, sizeof(header)) != sizeof(header) || header.magic != RECORD_MAGIC ||
      header.length == 0 || header.length > MAX_PROJECT_BYTES || file.size() != sizeof(header) + header.length) return false;
  String body; body.reserve(header.length + 1);
  while (file.available()) { char buffer[512]; const size_t n = file.read((uint8_t *)buffer, sizeof(buffer)); if (!n) return false; body.concat(buffer, n); yield(); }
  file.close();
  uint8_t actual[32]; mbedtls_sha256_ret((const uint8_t *)body.c_str(), body.length(), actual, 0);
  if (memcmp(actual, header.digest, 32) != 0 || !validProject(body)) return false;
  if (payload) *payload = body;
  return true;
}
bool storeProject(const String &body) {
  if (currentGeneration == UINT32_MAX) return false;
  RecordHeader header = {}; header.magic = RECORD_MAGIC; header.generation = currentGeneration + 1; header.length = body.length();
  mbedtls_sha256_ret((const uint8_t *)body.c_str(), body.length(), header.digest, 0);
  const String target = activePath == "/project-a.bin" ? "/project-b.bin" : "/project-a.bin";
  File file = stateFS.open("/pending.bin", "w");
  if (!file) return false;
  bool ok = file.write((const uint8_t *)&header, sizeof(header)) == sizeof(header) &&
    file.write((const uint8_t *)body.c_str(), body.length()) == body.length();
  file.flush(); file.close();
  RecordHeader verified;
  if (!ok || !readRecord("/pending.bin", verified)) return false;
  // The active slot stays valid through removal/rename of the inactive slot.
  if (stateFS.exists(target) && !stateFS.remove(target)) return false;
  if (!stateFS.rename("/pending.bin", target)) return false;
  activePath = target; currentGeneration = header.generation; currentSha = digestHex(header.digest);
  return true;
}
bool blankPartition(const esp_partition_t *part) {
  if (!part) return false;
  uint8_t buffer[256];
  for (size_t offset = 0; offset < part->size; offset += sizeof(buffer)) {
    if (esp_partition_read(part, offset, buffer, sizeof(buffer)) != ESP_OK) return false;
    for (uint8_t b : buffer) if (b != 0xff) return false;
    yield();
  }
  return true;
}
bool initStorage() {
  if (!LittleFS.begin(false, "/web", 8, "spiffs")) { Serial.println("Web filesystem missing. Run uploadfs."); return false; }
  if (!stateFS.begin(false, "/state", 8, "state")) {
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "state");
    if (!blankPartition(part) || !stateFS.begin(true, "/state", 8, "state")) {
      Serial.println("State filesystem unreadable. No automatic erase. Restore from JSON backup after servicing flash."); return false;
    }
  }
  RecordHeader a, b; const bool hasA = readRecord("/project-a.bin",a), hasB = readRecord("/project-b.bin",b);
  if (hasA || hasB) {
    const bool chooseA = hasA && (!hasB || a.generation >= b.generation);
    activePath = chooseA ? "/project-a.bin" : "/project-b.bin";
    const RecordHeader &chosen = chooseA ? a : b;
    currentGeneration = chosen.generation; currentSha = digestHex(chosen.digest);
    return true;
  }
  // Never replace existing but unreadable records with a seed.
  if (stateFS.exists("/project-a.bin") || stateFS.exists("/project-b.bin") || stateFS.exists("/pending.bin")) {
    Serial.println("No valid project record. Existing bytes preserved."); return false;
  }
  File seed = LittleFS.open("/seed.json", "r");
  if (!seed || seed.size() > MAX_PROJECT_BYTES) return false;
  String body = seed.readString(); seed.close();
  return validProject(body) && storeProject(body);
}
void getProject() {
  if (!ready) { reply(503, "STORAGE_UNAVAILABLE"); return; }
  RecordHeader header; String body;
  if (!readRecord(activePath, header, &body)) { reply(503, "STORAGE_UNAVAILABLE"); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", "{\"sha\":\"" + currentSha + "\",\"data\":" + body + "}");
}
void putProject() {
  if (!authorized()) return;
  if (!ready) { reply(503, "STORAGE_UNAVAILABLE"); return; }
  if (server.header("If-Match").length() == 0) { reply(428, "REVISION_REQUIRED"); return; }
  if (server.header("If-Match") != "\"" + currentSha + "\"") { reply(409, "CONFLICT"); return; }
  const String body = server.arg("plain");
  if (body.length() > MAX_PROJECT_BYTES) { reply(413, "PROJECT_TOO_LARGE"); return; }
  if (!validProject(body)) { reply(400, "INVALID_DATA"); return; }
  if (!storeProject(body)) { reply(507, "STORAGE_WRITE_FAILED"); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"sha\":\"" + currentSha + "\"}");
}
void asset(const char *path, const char *type) {
  if (!LittleFS.exists(path)) { reply(404, "ASSET_NOT_FOUND"); return; }
  File file = LittleFS.open(path, "r");
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  // streamFile() adds Content-Encoding: gzip for .gz files. Do not add it twice.
  server.streamFile(file, type); file.close();
}
void setup() {
  Serial.begin(115200); delay(1500);
  WiFi.mode(WIFI_STA); // Initialize hardware RNG without starting an unconfigured AP.
  if (!prefs.begin("ralf-web", false)) { Serial.println("NVS unavailable. Startup stopped."); return; }
  editorKey = prefs.getString("editor", ""); apPassword = prefs.getString("ap-pass", "");
  if (!editorKey.length()) { editorKey = randomSecret(16); prefs.putString("editor", editorKey); }
  if (!apPassword.length()) { apPassword = randomSecret(8); prefs.putString("ap-pass", apPassword); }
  ssid = "RALF-" + WiFi.macAddress().substring(12); ssid.replace(":", "");
  bool joined = false;
  if (strlen(RALF_WIFI_SSID)) {
    WiFi.setHostname("ralf"); WiFi.begin(RALF_WIFI_SSID, RALF_WIFI_PASSWORD);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) { delay(100); }
    joined = WiFi.status() == WL_CONNECTED;
  }
  if (!joined) {
    WiFi.softAP(ssid.c_str(), apPassword.c_str());
    Serial.printf("WiFi: %s\nWiFi password: %s\nURL: http://%s/\n",ssid.c_str(),apPassword.c_str(),WiFi.softAPIP().toString().c_str());
  } else {
    WiFi.mode(WIFI_STA);
    Serial.printf("WiFi connected. URL: http://%s/\n",WiFi.localIP().toString().c_str());
  }
  Serial.printf("Local editor key: %s\n",editorKey.c_str());
  Serial.printf("Flash: %u bytes; PSRAM: %u bytes\n",ESP.getFlashChipSize(),ESP.getPsramSize());
  if (MDNS.begin("ralf")) MDNS.addService("http", "tcp", 80);
  ready = initStorage();
  const char *headers[] = {"Authorization", "If-Match", "Origin"}; server.collectHeaders(headers,3);
  server.on("/api/project",HTTP_GET,getProject);
  server.on("/api/project",HTTP_PUT,putProject);
  server.on("/api/session",HTTP_POST,[]{if (authorized()) { server.sendHeader("Cache-Control","no-store"); server.send(200,"application/json","{\"authenticated\":true}"); }});
  server.on("/api/status",HTTP_GET,[]{JsonDocument d;d["storageReady"]=ready;d["generation"]=currentGeneration;d["maxProjectBytes"]=MAX_PROJECT_BYTES;d["freeHeap"]=ESP.getFreeHeap();d["psramBytes"]=ESP.getPsramSize();String body;serializeJson(d,body);server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",body);});
  server.on("/",HTTP_GET,[]{asset("/index.html.gz","text/html; charset=utf-8");});
  server.on("/index.html",HTTP_GET,[]{asset("/index.html.gz","text/html; charset=utf-8");});
  server.on("/app.js",HTTP_GET,[]{asset("/app.js.gz","application/javascript; charset=utf-8");});
  server.on("/config.js",HTTP_GET,[]{asset("/config.js.gz","application/javascript; charset=utf-8");});
  server.on("/esp32-sync.js",HTTP_GET,[]{asset("/esp32-sync.js.gz","application/javascript; charset=utf-8");});
  server.on("/style.css",HTTP_GET,[]{asset("/style.css.gz","text/css; charset=utf-8");});
  server.on("/favicon.ico",HTTP_GET,[]{server.send(204,"image/x-icon","");});
  server.onNotFound([]{Serial.printf("HTTP route missing: method=%d path=%s\n",(int)server.method(),server.uri().c_str());reply(404,"NOT_FOUND");});
  server.begin();
  Serial.println(ready ? "RALF dashboard ready." : "Storage unavailable. See messages above.");
}
void loop() { server.handleClient(); delay(2); }
