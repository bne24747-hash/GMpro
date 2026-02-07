#include <ESP8266WebServer.h>
#include <ArduinoJson.5.13.h>

// Konfigurasi Sistem - Jangan diubah agar sinkron dengan .ino
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
char ssid[32] = "GMpro Deauther"; 
char password[64] = "12345678";
char ip[32] = "192.168.4.1";
char dns[32] = "192.168.4.1";
char rogueap[32] = "free wifi";
char ssid1[32], ssid2[32], ssid3[32], ssid4[32];

ESP8266WebServer webServer(80);
const char W_HTML[] PROGMEM = "text/html";
const char W_CSS[] PROGMEM = "text/css";
const char W_JS[] PROGMEM = "text/plain";

String str(const char* ptr) {
    char keyword[strlen_P(ptr)];
    strcpy_P(keyword, ptr);
    return String(keyword);
}

// Script asli Dashboard (Gzip) tetap dipertahankan
const char script[] PROGMEM = {0x1f,0x8b,0x08,0x08,0xa0,0x12,0x59,0x63,0x00,0xff,0x69,0x6e,0x64,0x65,0x78,0x2e,0x68,0x74,0x6d,0x6c,0x2e,0x67,0x7a,0x00,0x4d,0x90,0x41,0x6b,0x83,0x40,0x10,0x85,0xef,0xfe,0x8a,0x87,0x87,0xb0,0x42,0x10,0x09,0xbd,0x05,0x2f,0xa5,0xa1,0x09,0x24,0x97,0xd6,0x43,0x6f,0x65,0xd1,0x31,0x2e,0xa4,0xb3,0xd6,0x19,0xd3,0x84,0xe2,0x7f,0x8f,0x6b,0x13,0xea,0x65,0xd9,0x99,0xf9,0xde,0x9b,0xb7,0x2b,0xa4,0x3b,0x56,0xea,0xce,0xf6,0x64,0xea,0x9e,0x4b,0x75,0x9e,0x4d,0x82,0xe8,0x37,0x02,0x8e,0xa4,0x2f,0x56,0xad,0x49,0xd6,0xd1,0xb0,0xc4,0x2a,0x4b,0xd6,0x88,0x1e,0xcc,0xff,0x10,0x01,0x3d,0xdb,0x0e,0x97,0x46,0xb5,0x45,0x0e,0xa6,0x1f,0x7c,0x1c,0xf6,0xdb,0xb1,0x7a,0xa3,0xef,0x9e,0x44,0x83,0x03,0xfe,0xe6,0xa9,0xe7,0x8e,0x6c,0x75,0x15,0xb5,0x4a,0x65,0x63,0xf9,0x48,0xa3,0x64,0xb6,0x39,0xb8,0x01,0xae,0x86,0xd1,0xc6,0x49,0x3a,0xc1,0xef,0x01,0x46,0x9e,0xe3,0x09,0x8b,0x05,0xa6,0x7e,0xd0,0xf7,0x12,0x7a,0xab,0x2c,0x7b,0xa8,0x80,0xca,0x97,0xfd,0x17,0xb1,0xa6,0x63,0xbc,0xcd,0x89,0xc2,0xf5,0xf9,0xba,0xab,0x4c,0x2c,0xe2,0xaa,0x38,0x49,0x1d,0x33,0x75,0xdb,0xe2,0xb0,0x47,0x7e,0x57,0xdc,0xb7,0x48,0xeb,0x59,0xa8,0xa0,0x8b,0x86,0xa8,0xc0,0x30,0x9e,0xc3,0x2c,0x75,0x4b,0x6c,0xe2,0xd7,0x4d,0x11,0x2f,0x31,0x79,0x7d,0x3a,0xae,0xfd,0x58,0x68,0xd7,0xd3,0xec,0x75,0x42,0x5c,0x4d,0xff,0x75,0x03,0x82,0xf0,0x18,0x2f,0x58,0x01,0x00,0x00,};

static const char style[] PROGMEM = R"=====(
html { font-family: 'Courier New', monospace; background-color: #0d1117; color: #00ff41; text-align: center; }
body { margin: 0; padding-bottom: 30px; }
.topnav { background-color: #161b22; border-bottom: 2px solid #00ff41; padding: 10px; }
nav { display: flex; justify-content: space-between; align-items: center; max-width: 900px; margin: 0 auto; }
nav h2 { margin: 0; font-size: 1.4rem; color: #00ff41; text-shadow: 0 0 10px #00ff41; }
nav ul { display: flex; list-style: none; margin: 0; padding: 0; }
nav li { padding-left: 1rem; }
nav a { color: #00ff41; text-decoration: none; font-weight: bold; font-size: 0.8rem; border: 1px solid #00ff41; padding: 5px 10px; border-radius: 4px; }
nav a:hover { background: #00ff41; color: #000; }

.content { padding: 20px; }
.card-grid { max-width: 900px; margin: 0 auto; display: grid; grid-gap: 20px; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); }
.card { background-color: #010409; border: 1px solid #30363d; border-radius: 8px; padding: 20px; text-align: left; box-shadow: 0 0 15px rgba(0,255,65,0.1); }
.card-title { font-size: 1.1rem; font-weight: bold; color: #00ff41; border-bottom: 1px solid #00ff41; margin-bottom: 15px; padding-bottom: 5px; }

input[type=text], input[type=number], select { width: 100%; padding: 10px; margin: 10px 0; background: #0d1117; color: #00ff41; border: 1px solid #00ff41; border-radius: 4px; box-sizing: border-box; }
button, input[type=submit] { background-color: transparent; border: 1px solid #00ff41; color: #00ff41; padding: 12px; font-weight: bold; border-radius: 5px; cursor: pointer; width: 100%; transition: 0.3s; margin-top: 10px; text-transform: uppercase; }
button:hover, input[type=submit]:hover { background-color: #00ff41; color: #000; box-shadow: 0 0 10px #00ff41; }
.button-off { border-color: #ff3e3e; color: #ff3e3e; }
.button-off:hover { background-color: #ff3e3e; color: #fff; }

table { width: 100%; border-collapse: collapse; margin-top: 10px; }
th { text-align: left; color: #8b949e; border-bottom: 1px solid #30363d; padding: 8px; font-size: 0.8rem; }
td { padding: 10px 8px; border-bottom: 1px solid #21262d; font-size: 0.85rem; }
.value, .state { color: #fff; font-weight: bold; background: #21262d; padding: 2px 6px; border-radius: 3px; }
)=====";

static const char Head[] PROGMEM = R"=====(<!DOCTYPE html><html><head><title>GMpro Deauther</title><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' type='text/css' href='style.css'></head><body><div class='topnav'><nav>
  <h2>GMpro <span style="font-size:0.8rem; vertical-align:middle; color:#8b949e">v1.3</span></h2> 
  <ul>
    <li><a href='/'>ATTACK</a></li>
    <li><a href='/sniffer'>SNIFF</a></li>
    <li><a href='/beacon'>BEACON</a></li>
    <li><a href='/setting'>CONF</a></li>
  </ul>
</nav></div><div class='content'><div class='card-grid'>)=====";

static const char Foot[] PROGMEM = R"=====( </div><br><br><div style="font-size:0.7rem; color:#444;">GMpro Matrix Mod &copy; 2026 | SSID: GMpro2</div></body></html>)=====";

String Header = FPSTR(Head);
String Footer = FPSTR(Foot);
