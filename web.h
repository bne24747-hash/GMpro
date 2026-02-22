#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// Deklarasi File System
FS* fileSystem = &LittleFS;

// Variabel Global
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

// --- DEFAULT SETTINGS GMpro87 ---
char ssid[32] = "GMpro";
char password[64] = "Sangkur87";
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

// GZIP Script Data (Jangan Diubah)
const char script[] PROGMEM = {0x1f,0x8b,0x08,0x08,0xa0,0x12,0x59,0x63,0x00,0xff,0x69,0x6e,0x64,0x65,0x78,0x2e,0x68,0x74,0x6d,0x6c,0x2e,0x67,0x7a,0x00,0x4d,0x90,0x41,0x6b,0x83,0x40,0x10,0x85,0xef,0xfe,0x8a,0x87,0x87,0xb0,0x42,0x10,0x09,0xbd,0x05,0x2f,0xa5,0xa1,0x09,0x24,0x97,0xd6,0x43,0x6f,0x65,0xd1,0x31,0x2e,0xa4,0xb3,0xd6,0x19,0xd3,0x84,0xe2,0x7f,0x8f,0x6b,0x13,0xea,0x65,0xd9,0x99,0xf9,0xde,0x9b,0xb7,0x2b,0xa4,0x3b,0x56,0xea,0xce,0xf6,0x64,0xea,0x9e,0x4b,0x75,0x9e,0x4d,0x82,0xe8,0x37,0x02,0x8e,0xa4,0x2f,0x56,0xad,0x49,0xd6,0xd1,0xb0,0xc4,0x2a,0x4b,0xd6,0x88,0x1e,0xcc,0xff,0x10,0x01,0x3d,0xdb,0x0e,0x97,0x46,0xb5,0x45,0x0e,0xa6,0x1f,0x7c,0x1c,0xf6,0xdb,0xb1,0x7a,0xa3,0xef,0x9e,0x44,0x83,0x03,0xfe,0xe6,0xa9,0xe7,0x8e,0x6c,0x75,0x15,0xb5,0x4a,0x65,0x63,0xf9,0x48,0xa3,0x64,0xb6,0x39,0xb8,0x01,0xae,0x86,0xd1,0xc6,0x49,0x3a,0xc1,0xef,0x01,0x46,0x9e,0xe3,0x09,0x8b,0x05,0xa6,0x7e,0xd0,0xf7,0x12,0x7a,0xab,0x2c,0x7b,0xa8,0x80,0xca,0x97,0xfd,0x17,0xb1,0xa6,0x63,0xbc,0xcd,0x89,0xc2,0xf5,0xf9,0xba,0xab,0x4c,0x2c,0xe2,0xaa,0x38,0x49,0x1d,0x33,0x75,0xdb,0xe2,0xb0,0x47,0x7e,0x57,0xdc,0xb7,0x48,0xeb,0x59,0xa8,0xa0,0x8b,0x86,0xa8,0xc0,0x30,0x9e,0xc3,0x2c,0x75,0x4b,0x6c,0xe2,0xd7,0x4d,0x11,0x2f,0x31,0x79,0x7d,0x3a,0xae,0xfd,0x58,0x68,0xd7,0xd3,0xec,0x75,0x42,0x5c,0x4d,0xff,0x75,0x03,0x82,0xf0,0x18,0x2f,0x58,0x01,0x00,0x00};

void sendProgmem(const char* ptr, size_t size, const char* type) {
    webServer.sendHeader("Content-Encoding", "gzip");
    webServer.sendHeader("Cache-Control", "max-age=3600");
    webServer.send_P(200, str(type).c_str(), ptr, size);
}

void initFS() {
    if (!LittleFS.begin()) { Serial.println("LittleFS mount error"); }
    else { Serial.println("LittleFS mounted"); }
}

String readFile(fs::FS &fs, const char * path){
    File file = fs.open(path, "r");
    if(!file || file.isDirectory()) return String();
    String fileContent = file.readStringUntil('\n');
    file.close();
    return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    File file = fs.open(path, "w");
    if(!file) return;
    file.print(message);
    file.close();
}

void savesetting(){
    String temp = "{\"ssid\":\""+String(ssid)+"\",\"password\":\""+String(password)+"\",\"channel\":\""+String(chnl)+"\",\"ip\":\""+String(ip)+"\",\"dns\":\""+String(dns)+"\",\"rogueap\":\""+String(rogueap)+"\",\"ssid1\":\""+String(ssid1)+"\",\"ssid2\":\""+String(ssid2)+"\",\"ssid3\":\""+String(ssid3)+"\",\"ssid4\":\""+String(ssid4)+"\",\"loading_enable\":\""+String(loading_enable)+"\",\"beacon_size\":\""+String(beacon_size)+"\",\"deauth_speed\":\""+String(deauth_speed)+"\",\"loadingpath\":\""+String(loadingpath)+"\",\"eviltwinpath\":\""+String(eviltwinpath)+"\",\"pishingpath\":\""+String(pishingpath)+"\"}";
    writeFile(LittleFS,"/setting.txt",temp.c_str());
}

void loaddefault(){
    String temp = "{\"ssid\":\"GMpro\",\"password\":\"Sangkur87\",\"channel\":\"8\",\"ip\":\"192.168.4.1\",\"dns\":\"192.168.4.1\",\"rogueap\":\"free wifi\",\"deauth_speed\":\"1000\",\"ssid1\":\"null\",\"ssid2\":\"null\",\"ssid3\":\"null\",\"ssid4\":\"null\",\"loading_enable\":\"0\",\"beacon_size\":\"20\",\"loadingpath\":\"/loadingpath.html\",\"eviltwinpath\":\"/contoh.html\",\"pishingpath\":\"/contoh.html\"}";
    writeFile(LittleFS,"/setting.txt",temp.c_str());
}

void loadsetting(){
    String temp = readFile(LittleFS,"/setting.txt");
    StaticJsonBuffer<1000> JSONBuffer;
    JsonObject& parsed = JSONBuffer.parseObject(temp.c_str());
    if(!parsed.success()){ loaddefault(); return; }
    strcpy(eviltwinpath, parsed["eviltwinpath"]);
    strcpy(pishingpath, parsed["pishingpath"]);
    strcpy(loadingpath, parsed["loadingpath"]);
    strcpy(ssid, parsed["ssid"]);
    strcpy(password, parsed["password"]);
    strcpy(ip, parsed["ip"]);
    strcpy(dns, parsed["dns"]);
    strcpy(rogueap, parsed["rogueap"]);
    strcpy(ssid1, parsed["ssid1"]);
    strcpy(ssid2, parsed["ssid2"]);
    strcpy(ssid3, parsed["ssid3"]);
    strcpy(ssid4, parsed["ssid4"]);
    chnl = parsed["channel"];
    loading_enable = parsed["loading_enable"];
    beacon_size = parsed["beacon_size"];
    deauth_speed = parsed["deauth_speed"];
}

// ==========================================
// CUSTOM UI CSS (CYBER DARK BLUE)
// ==========================================
static const char style[] PROGMEM = R"=====(
:root {
  --bg: #0a0c10; --card: #161b22; --primary: #58a6ff; 
  --accent: #7ee787; --text: #c9d1d9; --border: #30363d;
}
html { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); text-align: center; }
body { margin: 0; padding-bottom: 50px; }
.topnav { 
  background: rgba(22, 27, 34, 0.95); backdrop-filter: blur(10px); 
  padding: 15px 10px; border-bottom: 2px solid var(--primary); position: sticky; top: 0; z-index: 100;
}
.logo-container { display: flex; flex-direction: column; align-items: center; margin-bottom: 10px; }
.logo-main { color: var(--primary); font-size: 1.5rem; font-weight: 800; letter-spacing: 2px; margin: 0; display: flex; align-items: center; gap: 10px; }
.logo-dev { color: #8b949e; font-size: 0.65rem; font-family: monospace; letter-spacing: 1px; margin-top: -2px; }

nav ul { display: flex; list-style: none; padding: 0; gap: 8px; justify-content: center; flex-wrap: wrap; margin-top: 10px; }
nav li a { 
  color: var(--text); text-decoration: none; font-size: 0.75rem; font-weight: bold;
  padding: 8px 12px; border-radius: 6px; background: #21262d; border: 1px solid var(--border); transition: 0.3s;
}
nav li a:hover { background: var(--primary); color: #fff; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(88, 166, 255, 0.3); }

.content { padding: 20px; max-width: 900px; margin: auto; }
.card { 
  background: var(--card); border: 1px solid var(--border); border-radius: 15px; 
  padding: 20px; margin-bottom: 25px; box-shadow: 0 8px 24px rgba(0,0,0,0.5);
}
.card-title { color: var(--primary); font-size: 1rem; font-weight: bold; margin-bottom: 15px; text-transform: uppercase; border-bottom: 1px solid var(--border); padding-bottom: 10px; }
input[type=text], input[type=number], select {
  width: 90%; padding: 12px; margin: 10px 0; background: #0d1117; 
  border: 1px solid var(--border); border-radius: 8px; color: var(--accent); font-family: monospace;
}
input[type=submit], button {
  background: var(--primary); border: none; color: #fff; padding: 12px 25px;
  border-radius: 8px; font-weight: bold; cursor: pointer; transition: 0.3s; margin: 5px;
}
button:hover { filter: brightness(1.2); }
.button-on { background: #238636; } 
.button-off { background: #da3633; }
.state { color: var(--accent); font-weight: bold; }
)=====";

static const char Head[] PROGMEM = R"=====(<!DOCTYPE html><html><head><title>GMpro87 Admin</title><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' type='text/css' href='style.css'></head><body><div class='topnav'><nav><div class='logo-container'><h2 class='logo-main'><svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"></path></svg> GMpro87</h2><span class='logo-dev'>dev : 9u5M4n9</span></div><ul><li><a href='/'>ATTACK</a></li><li><a href='/filemanager'>FS</a></li><li><a href='/sniffer'>PACKET</a></li><li><a href='/setting'>SETTING</a></li><li><a href='/log'>LOG</a></li></ul></nav></div><div class='content'><div class='card-grid'>)=====";

static const char Foot[] PROGMEM = R"=====(</div></div><div style='margin-top:30px; font-size:0.7rem; color:#8b949e; letter-spacing:1px;'>GMpro Project &copy; 2024 | Powered by 9u5M4n9</div></body></html>)=====";

String Header = FPSTR(Head);
String Footer = FPSTR(Foot);
