#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Deklarasi variabel sistem sesuai file original
FS* fileSystem = &LittleFS;
long used_spiffs, free_spiffs, total_spiffs;
char eviltwinpath[32], pishingpath[32], loadingpath[32];
int loading_enable = 0, beacon_size = 20, deauth_speed = 1000, chnl = 8;
char ssid[32] = "WifiX v1.3", password[64] = "12345678";
char ip[32] = "192.168.4.1", dns[32] = "192.168.4.1", rogueap[32] = "free wifi";
char ssid1[32], ssid2[32], ssid3[32], ssid4[32];

ESP8266WebServer webServer(80);

// Helper untuk PROGMEM
String str(const char* ptr) {
    char keyword[strlen_P(ptr) + 1];
    strcpy_P(keyword, ptr);
    return String(keyword);
}

// ==========================================
// UI MODERN ASSETS (CSS & JS)
// ==========================================

static const char style_css[] PROGMEM = R"=====(
:root {
  --bg: #0f172a; --card: #1e293b; --accent: #38bdf8;
  --text: #f8fafc; --danger: #ef4444; --success: #22c55e;
}
body { 
  font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); 
  margin: 0; padding: 0; display: flex; flex-direction: column; min-height: 100vh;
}
nav {
  background: rgba(30, 41, 59, 0.9); backdrop-filter: blur(10px);
  padding: 1rem 1.5rem; display: flex; justify-content: space-between; align-items: center;
  border-bottom: 1px solid rgba(255,255,255,0.1); position: sticky; top: 0; z-index: 100;
}
nav h2 { margin: 0; color: var(--accent); font-size: 1.3rem; }
.nav-links { display: flex; gap: 15px; }
.nav-links a { color: var(--text); text-decoration: none; font-size: 1.2rem; opacity: 0.7; transition: 0.3s; }
.nav-links a:hover { opacity: 1; color: var(--accent); }
.content { padding: 20px; max-width: 800px; margin: auto; width: 100%; box-sizing: border-box; }
.card { 
  background: var(--card); border-radius: 12px; padding: 20px; margin-bottom: 20px;
  border: 1px solid rgba(255,255,255,0.05); box-shadow: 0 10px 15px -3px rgba(0,0,0,0.3);
}
.card-title { font-size: 1.1rem; font-weight: 600; margin-bottom: 15px; color: var(--accent); display: flex; justify-content: space-between; }
input, select {
  width: 100%; padding: 12px; margin: 8px 0; background: #334155; border: 1px solid #475569;
  border-radius: 8px; color: white; box-sizing: border-box;
}
.btn {
  cursor: pointer; border: none; border-radius: 8px; padding: 12px; font-weight: bold;
  transition: 0.3s; width: 100%; margin-top: 10px; display: block; text-align: center;
}
.btn-primary { background: var(--accent); color: var(--bg); }
.btn-danger { background: var(--danger); color: white; }
.status-badge { font-size: 0.7rem; padding: 2px 8px; border-radius: 10px; background: var(--success); color: white; }
)=====";

static const char main_js[] PROGMEM = R"=====(
<script>
function apiCall(path) {
  const st = document.getElementById('stat');
  st.innerText = "WAIT...";
  fetch(path).then(r => {
    st.innerText = r.ok ? "DONE" : "ERR";
    setTimeout(() => { st.innerText = "READY"; }, 2000);
  });
}
function saveSet() {
  const f = document.getElementById('setForm');
  const d = new URLSearchParams(new FormData(f)).toString();
  fetch('/savesetting?' + d).then(r => alert('Settings Saved!'));
}
</script>
)=====";

// ==========================================
// CORE FUNCTIONS
// ==========================================

void initFS() {
  if (!LittleFS.begin()) Serial.println("FS Error");
}

String readFile(fs::FS &fs, const char * path){
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()) return String();
  String content = file.readString();
  file.close();
  return content;
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  File file = fs.open(path, "w");
  if(!file) return;
  file.print(message);
  file.close();
}

void savesetting(){
  StaticJsonBuffer<1000> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["ssid"] = ssid; root["password"] = password;
  root["channel"] = chnl; root["ip"] = ip;
  root["dns"] = dns; root["rogueap"] = rogueap;
  // ... tambahkan variabel lain sesuai kebutuhan
  
  String output;
  root.printTo(output);
  writeFile(LittleFS, "/setting.txt", output.c_str());
}

// ==========================================
// WEB SERVER HANDLERS (SIMPLE & COOL)
// ==========================================

void handleRoot() {
  String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><style>";
  h += str(style_css);
  h += "</style><link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css'></head><body>";
  h += "<nav><h2><i class='fas fa-ghost'></i> WifiX</h2><div class='nav-links'>";
  h += "<a href='/'><i class='fas fa-home'></i></a><a href='/setting'><i class='fas fa-cog'></i></a></div></nav>";
  h += "<div class='content'>";
  
  // Status Card
  h += "<div class='card'><div class='card-title'>SYSTEM STATUS <span class='status-badge' id='stat'>READY</span></div>";
  h += "<p style='font-size:0.9rem'>Target: <b color='#38bdf8'>" + String(rogueap) + "</b></p></div>";

  // Attack Card
  h += "<div class='card'><div class='card-title'>ATTACK CONTROL</div>";
  h += "<button class='btn btn-primary' onclick=\"apiCall('/start')\">START ATTACK</button>";
  h += "<button class='btn btn-danger' onclick=\"apiCall('/stop')\">STOP ALL</button></div>";

  h += "</div>" + str(main_js) + "</body></html>";
  webServer.send(200, "text/html", h);
}
