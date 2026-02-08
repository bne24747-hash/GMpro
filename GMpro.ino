#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <EEPROM.h>
#include "Mizer.h"
#include "Functions.h"

ESP8266WebServer server(80);
DNSServer dnsServer;

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  EEPROM.begin(512);
  
  // Set Max Power Wemos
  WiFi.setOutputPower(20.5);
  
  // Konfigurasi WiFi Admin (SSID: GMpro2)
  WiFi.softAP("GMpro2", "Sangkur87", 1, 0); // Start dengan Hidden OFF sesuai request
  
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Routes
  server.on("/", HTTP_GET, [](){
    server.send(200, "text/html", INDEX_HTML);
  });
  
  server.on("/api", HTTP_GET, handleAPI);
  server.on("/upload", HTTP_POST, [](){ server.send(200); }, handleFileUpload);
  
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("GMpro Ready!");
  
  // Auto-resume jika sebelumnya ada serangan aktif
  resumeLastAttack(); 
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  runAttackEngine(); // Logic deauth & beacon di sini
}
