#include <ESP8266WebServer.h>

ESP8266WebServer webServer(80);
extern int packetCounter;
extern bool modeMassDeauth;

static const char style[] PROGMEM = R"=====(
html { font-family: 'Courier New', monospace; background-color: #0d1117; color: #00ff41; text-align: center; }
body { margin: 0; padding-bottom: 30px; }
.topnav { background-color: #161b22; border-bottom: 2px solid #ff3e3e; padding: 10px; }
nav h2 { margin: 0; font-size: 1.4rem; color: #ff3e3e; text-shadow: 0 0 10px #ff3e3e; }
nav ul { display: flex; list-style: none; margin: 0; padding: 0; justify-content: center; }
nav li { padding: 0 10px; }
nav a { color: #fff; text-decoration: none; font-size: 0.8rem; border: 1px solid #444; padding: 5px 10px; border-radius: 4px; }
.content { padding: 20px; }
.card { background-color: #010409; border: 1px solid #30363d; border-radius: 8px; padding: 20px; max-width: 500px; margin: 10px auto; }
.card-title { font-size: 1.1rem; font-weight: bold; color: #ff3e3e; border-bottom: 1px solid #444; margin-bottom: 15px; padding-bottom: 5px; }
button { background-color: transparent; border: 1px solid #ff3e3e; color: #ff3e3e; padding: 12px; font-weight: bold; cursor: pointer; width: 100%; margin-top: 10px; text-transform: uppercase; }
button:hover { background-color: #ff3e3e; color: #000; }
.value { color: #fff; background: #21262d; padding: 2px 6px; border-radius: 3px; }
)=====";

static const char Head[] PROGMEM = R"=====(<!DOCTYPE html><html><head><title>GMpro RIOT</title><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' type='text/css' href='style.css'></head><body><div class='topnav'><nav><h2>GMpro <span style="color:#fff">RIOT</span></h2><ul><li><a href='/'>DASH</a></li><li><a href='/mass'>MASS DEAUTH</a></li><li><a href='/stop'>STOP</a></li></ul></nav></div><div class='content'>)=====";
static const char Foot[] PROGMEM = R"=====(</div><div style="font-size:0.7rem; color:#444; margin-top:20px;">OLED 0.66 Active | GMpro v1.3</div></body></html>)=====";

String Header = FPSTR(Head);
String Footer = FPSTR(Foot);
