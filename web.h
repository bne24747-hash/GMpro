#include <ESP8266WebServer.h>
#include <ArduinoJson.5.13.h>
FS* fileSystem = &LittleFS;
long used_spiffs;
long free_spiffs;
long total_spiffs;
char eviltwinpath[32];
char pishingpath[32];
char loadingpath[32];
int loading_enable = 0;
int beacon_size = 20;
int deauth_speed = 1000;
int chnl = 8;
char ssid[32] = "WifiX v1.3";
char password[64] = "12345678";
char ip[32] = "192.168.4.1";
char dns[32] = "192.168.4.1";
char rogueap[32] = "free wifi";
char ssid1[32];
char ssid2[32];
char ssid3[32];
char ssid4[32];
ESP8266WebServer webServer(80);
const char W_HTML[] PROGMEM = "text/html";
const char W_CSS[] PROGMEM = "text/css";
const char W_JS[] PROGMEM = "text/plain";

String str(const char* ptr) {
    char keyword[strlen_P(ptr)];

    strcpy_P(keyword, ptr);
    return String(keyword);
}

const char script[] PROGMEM = {0x1f,0x8b,0x08,0x08,0xa0,0x12,0x59,0x63,0x00,0xff,0x69,0x6e,0x64,0x65,0x78,0x2e,0x68,0x74,0x6d,0x6c,0x2e,0x67,0x7a,0x00,0x4d,0x90,0x41,0x6b,0x83,0x40,0x10,0x85,0xef,0xfe,0x8a,0x87,0x87,0xb0,0x42,0x10,0x09,0xbd,0x05,0x2f,0xa5,0xa1,0x09,0x24,0x97,0xd6,0x43,0x6f,0x65,0xd1,0x31,0x2e,0xa4,0xb3,0xd6,0x19,0xd3,0x84,0xe2,0x7f,0x8f,0x6b,0x13,0xea,0x65,0xd9,0x99,0xf9,0xde,0x9b,0xb7,0x2b,0xa4,0x3b,0x56,0xea,0xce,0xf6,0x64,0xea,0x9e,0x4b,0x75,0x9e,0x4d,0x82,0xe8,0x37,0x02,0x8e,0xa4,0x2f,0x56,0xad,0x49,0xd6,0xd1,0xb0,0xc4,0x2a,0x4b,0xd6,0x88,0x1e,0xcc,0xff,0x10,0x01,0x3d,0xdb,0x0e,0x97,0x46,0xb5,0x45,0x0e,0xa6,0x1f,0x7c,0x1c,0xf6,0xdb,0xb1,0x7a,0xa3,0xef,0x9e,0x44,0x83,0x03,0xfe,0xe6,0xa9,0xe7,0x8e,0x6c,0x75,0x15,0xb5,0x4a,0x65,0x63,0xf9,0x48,0xa3,0x64,0xb6,0x39,0xb8,0x01,0xae,0x86,0xd1,0xc6,0x49,0x3a,0xc1,0xef,0x01,0x46,0x9e,0xe3,0x09,0x8b,0x05,0xa6,0x7e,0xd0,0xf7,0x12,0x7a,0xab,0x2c,0x7b,0xa8,0x80,0xca,0x97,0xfd,0x17,0xb1,0xa6,0x63,0xbc,0xcd,0x89,0xc2,0xf5,0xf9,0xba,0xab,0x4c,0x2c,0xe2,0xaa,0x38,0x49,0x1d,0x33,0x75,0xdb,0xe2,0xb0,0x47,0x7e,0x57,0xdc,0xb7,0x48,0xeb,0x59,0xa8,0xa0,0x8b,0x86,0xa8,0xc0,0x30,0x9e,0xc3,0x2c,0x75,0x4b,0x6c,0xe2,0xd7,0x4d,0x11,0x2f,0x31,0x79,0x7d,0x3a,0xae,0xfd,0x58,0x68,0xd7,0xd3,0xec,0x75,0x42,0x5c,0x4d,0xff,0x75,0x03,0x82,0xf0,0x18,0x2f,0x58,0x01,0x00,0x00,};

void sendProgmem(const char* ptr, size_t size, const char* type) {
        webServer.sendHeader("Content-Encoding", "gzip");
        webServer.sendHeader("Cache-Control", "max-age=3600");
        webServer.send_P(200, str(type).c_str(), ptr, size);
    }
void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  else{
    Serial.println("LittleFS mounted successfully");
  }
}


// Read File from LittleFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;
  }
  file.close();
  return fileContent;
}

// Write file to LittleFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
  file.close();
}
void savesetting(){
 String temp = "";
 temp += "{\"ssid\":\""+String(ssid)+"\",";
 temp += "\"password\":\""+String(password)+"\",";
 temp += "\"channel\":\""+String(chnl)+"\",";
 temp += "\"ip\":\""+String(ip)+"\",";
 temp += "\"dns\":\""+String(dns)+"\",";
 temp += "\"rogueap\":\""+String(rogueap)+"\",";
 temp += "\"ssid1\":\""+String(ssid1)+"\",";
 temp += "\"ssid2\":\""+String(ssid2)+"\",";
 temp += "\"ssid3\":\""+String(ssid3)+"\",";
 temp += "\"ssid4\":\""+String(ssid4)+"\",";
 temp += "\"loading_enable\":\""+String(loading_enable)+"\",";
 temp += "\"beacon_size\":\""+String(beacon_size)+"\",";
 temp += "\"deauth_speed\":\""+String(deauth_speed)+"\",";
 temp += "\"loadingpath\":\""+String(loadingpath)+"\",";
 temp += "\"eviltwinpath\":\""+String(eviltwinpath)+"\",";
 temp += "\"pishingpath\":\""+String(pishingpath)+"\"";
 temp += "}";
 writeFile(LittleFS,"/setting.txt",temp.c_str());
}
void loaddefault(){
  String temp = "";
 temp += "{\"ssid\":\"WifiX v1.3\",";
 temp += "\"password\":\"12345678\",";
 temp += "\"channel\":\"8\",";
 temp += "\"ip\":\"192.168.4.1\",";
 temp += "\"dns\":\"192.168.4.1\",";
 temp += "\"rogueap\":\"free wifi\",";
 temp += "\"deauth_speed\":\"1000\",";
 temp += "\"ssid1\":\"null\",";
 temp += "\"ssid2\":\"null\",";
 temp += "\"ssid3\":\"null\",";
 temp += "\"ssid4\":\"null\",";
 temp += "\"loading_enable\":\"0\",";
 temp += "\"beacon_size\":\"20\",";
 temp += "\"loadingpath\":\"/loadingpath.html\",";
 temp += "\"eviltwinpath\":\"/contoh.html\",";
 temp += "\"pishingpath\":\"/contoh.html\"";
 temp += "}";
 Serial.println(temp);
 writeFile(LittleFS,"/setting.txt",temp.c_str());
}
void loadsetting(){
  Serial.print("\n \n \n");
  String temp = readFile(LittleFS,"/setting.txt");
  delay(200);
  char setting[315];
  temp.toCharArray(setting,315);
  Serial.println("Loading setting...");
  Serial.println(setting);
  StaticJsonBuffer<1000> JSONBuffer;
  JsonObject& parsed = JSONBuffer.parseObject(setting);
  if(!parsed.success()){
    Serial.println("load failed");
    loaddefault();
    return;
  }
  strcpy(eviltwinpath, (const char * )parsed["eviltwinpath"] );
  strcpy(pishingpath, (const char * )parsed["pishingpath"] );
  strcpy(loadingpath, (const char * )parsed["loadingpath"] );
  strcpy(ssid, (const char * )parsed["ssid"] );
  strcpy(password, (const char * )parsed["password"] );
  strcpy(ip, (const char * ) parsed["ip"]);
  strcpy(dns, (const char * )parsed["dns"] );
  strcpy(rogueap, (const char * )parsed["rogueap"] );
  strcpy(ssid1, (const char * ) parsed["ssid1"] );
  strcpy(ssid2, (const char * ) parsed["ssid2"]);
  strcpy(ssid3, (const char * )parsed["ssid3"] );
  strcpy(ssid4, (const char * )parsed["ssid4"] );
  chnl = parsed["channel"];
  loading_enable = parsed["loading_enable"];
  beacon_size = parsed["beacon_size"];
  deauth_speed = parsed["deauth_speed"];
  FSInfo fs_info;
  fileSystem->info(fs_info);
  total_spiffs = fs_info.totalBytes/1000;
  used_spiffs = fs_info.usedBytes/1000;
  free_spiffs = total_spiffs - used_spiffs;
  Serial.println("-------------FS------------");
  Serial.println("total      : "+String(total_spiffs));
  Serial.println("free       : "+String(free_spiffs));
  Serial.println("used       : "+String(used_spiffs));
  Serial.println("evil twin  : "+String(eviltwinpath));
  Serial.println("pishing    : "+String(pishingpath));
  Serial.println("load page  : "+String(loadingpath));
  Serial.println("----------------------------");
  Serial.println("----------Network----------");
  Serial.println("ssid       : "+String(ssid));
  Serial.println("password   : "+String(password));
  Serial.println("channel    : "+String(chnl));
  Serial.println("ip         : "+String(ip));
  Serial.println("dns        : "+String(dns));
  Serial.println("----------------------------");
  Serial.println("--------Attack/defense-----");
  Serial.println("rogue wifi : "+String(rogueap));
  Serial.println("beacon size: "+String(beacon_size));
  Serial.println("load enable: "+String(loading_enable));
  Serial.println("deauth/s   : "+String(deauth_speed));
  Serial.println("ssid1      : "+String(ssid1));
  Serial.println("ssid2      : "+String(ssid2));
  Serial.println("ssid3      : "+String(ssid3));
  Serial.println("ssid4      : "+String(ssid4));
  Serial.println("load page  : "+String(loadingpath));
  Serial.println("----------------------------");
  Serial.print("\n \n \n");
}

static const char style[] PROGMEM = R"=====(
html {
  font-family: Arial, Helvetica, sans-serif; 
  display: inline-block; 
  text-align: center;
}

h1 {
  font-size: 1.8rem; 
  color: white;
}

p { 
  font-size: 1.4rem;
}

.topnav { 
  overflow: hidden; 
  background-color: #0A1128;
}

body {  
  margin: 0;
}

.content { 
  padding: 5%;
}

.card-grid { 

  max-width: 800px; 
  margin: 0 auto; 
  display: grid; 
  grid-gap: 2rem; 
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
}

.card { 
  background-color: white; 
  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
}
nav {
  display: flex; /* 1 */
  justify-content: space-between; /* 2 */
  padding: 1rem 2rem; /* 3 */
  background: #cfd8dc; /* 4 */
}

nav ul {
  display: flex; /* 5 */
  list-style: none; /* 6 */
}

nav li {
  padding-left: 1rem; /* 7! */
}
.card-title { 
  font-size: 1.2rem;
  font-weight: bold;
  color: #034078
}

input[type=submit] {
  border: none;
  color: #FEFCFB;
  background-color: #034078;
  padding: 1px 1px;
  text-align: center;
  text-decoration: none;
  display: inline-block;
  font-size: 16px;
  width: 80px;
  height: 18px;
  margin-right: 5px;
  border-radius: 5px;
  transition-duration: 0.4s;
  }

input[type=submit]:hover {
  background-color: #1282A2;
}

input[type=text], input[type=number], select {
  width: 50%;
  padding: 12px 20px;
  margin: 18px;
  display: inline-block;
  border: 1px solid #ccc;
  border-radius: 4px;
  box-sizing: border-box;
}

label {
  font-size: 1.2rem; 
}
.value{
  font-size: 1.2rem;
  color: #1282A2;  
}
.state {
  font-size: 1.2rem;
  color: #1282A2;
}
button {
  border: none;
  color: #FEFCFB;
  padding: 15px 32px;
  text-align: center;
  font-size: 16px;
  width: 100px;
  border-radius: 4px;
  transition-duration: 0.4s;
}
.button-on {
  background-color: #034078;
}
.button-on:hover {
  background-color: #1282A2;
}
.button-off {
  background-color: #858585;
}
.button-off:hover {
  background-color: #252524;
)=====";
static const char Head[] PROGMEM = R"=====(<!DOCTYPE html><html><head><title>ESP8266</title><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' type='text/css' href='style.css'><link rel='icon' type='image/png' href='favicon.png'><link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.7.2/css/all.css' integrity='sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr' crossorigin='anonymous'></head><body><div class='topnav'><nav>
  <h2><img src='data:image/png;base64,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' alt='' /></h2> 
  <ul>
    <li><a href='/'>Attack</a></li>
    <li><a href='/filemanager'>FS</a></li>
    <li><a href='/sniffer'>Packet Mon.</a></li>
    <li><a href='/setting'>Setting</a></li>
    <li><a href='/log'>Log</a></li>
  </ul>
</nav></div><div class='content'><div class='card-grid'>)=====";
static const char Foot[] PROGMEM = R"=====( </div><br><br><a href='https://www.youtube.com/c/ASP29Tech' target='_blank'><img src='data:image/jpeg;base64,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' alt='' /></a></body></html>)=====";
String Header = FPSTR(Head);
String Footer = FPSTR(Foot);
