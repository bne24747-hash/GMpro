#ifndef WEB_H
#define WEB_H

static const char Head[] PROGMEM = R"=====(<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>
html { font-family: 'Courier New', monospace; background: #050505; color: #00ff41; margin: 0; padding: 0; }
body { margin: 0; padding-bottom: 50px; }
.header { background: #111; padding: 20px; border-bottom: 2px solid #ff0000; text-align: center; box-shadow: 0 0 20px #ff0000; }
.logo { font-size: 2.2rem; font-weight: bold; color: #ff0000; text-shadow: 0 0 10px #ff0000; letter-spacing: 4px; }
.tabs { display: flex; background: #1a1a1a; position: sticky; top: 0; z-index: 99; border-bottom: 1px solid #333; }
.tab { flex: 1; padding: 15px; text-align: center; cursor: pointer; border-bottom: 3px solid #333; font-weight: bold; }
.tab.active { border-bottom: 3px solid #ff0000; color: #ff0000; background: #000; }
.content { padding: 10px; display: none; }
.content.active { display: block; }
.card { background: #0f0f0f; border: 1px solid #333; padding: 15px; margin-bottom: 12px; border-radius: 4px; }
.card-title { color: #ff0000; font-size: 0.8rem; margin-bottom: 12px; text-transform: uppercase; border-left: 4px solid #ff0000; padding-left: 10px; font-weight: bold; }
table { width: 100%; border-collapse: collapse; font-size: 0.75rem; color: #fff; }
th, td { border: 1px solid #222; padding: 10px; text-align: left; }
th { background: #1a1a1a; color: #00ff41; }
button { background: transparent; border: 1px solid #00ff41; color: #00ff41; padding: 14px; width: 100%; cursor: pointer; font-weight: bold; border-radius: 4px; margin: 5px 0; }
button.active { background: #ff0000 !important; color: #fff !important; border-color: #fff !important; box-shadow: 0 0 15px #ff0000; }
input, select { background: #1a1a1a; border: 1px solid #444; color: #fff; padding: 10px; width: 100%; box-sizing: border-box; margin-bottom: 10px; border-radius: 4px; }
</style></head><body>
<div class='header'><div class='logo'>GMpro87</div></div>
<div class='tabs'><div class='tab active' onclick="openTab(event, 'T1')">COMBAT</div><div class='tab' onclick="openTab(event, 'T2')">SETTING</div></div>
<div id='T1' class='content active'>)=====";

static const char Mid[] PROGMEM = R"=====(</div><div id='T2' class='content'>)=====";

static const char Foot[] PROGMEM = R"=====(</div>
<script>
function openTab(e, n) {
  var i, c, t;
  c = document.getElementsByClassName("content");
  for (i = 0; i < c.length; i++) c[i].style.display = "none";
  t = document.getElementsByClassName("tab");
  for (i = 0; i < t.length; i++) t[i].className = t[i].className.replace(" active", "");
  document.getElementById(n).style.display = "block";
  e.currentTarget.className += " active";
}
function tgl(btn, url) {
  fetch(url).then(r => r.text()).then(d => {
    if(d.includes("ON")) btn.classList.add("active");
    else btn.classList.remove("active");
  });
}
function fetchScan() {
  fetch('/get_scan').then(r => r.json()).then(data => {
    let b = document.getElementById('scan_body');
    b.innerHTML = '';
    data.forEach(w => {
      let row = `<tr><td>${w.ssid}</td><td>${w.ch}</td><td>${w.usr}</td><td>${w.rssi}%</td><td><input type='checkbox' onchange='sel("${w.mac}")'></td></tr>`;
      b.innerHTML += row;
    });
  });
}
setInterval(fetchScan, 5000);
</script></body></html>)=====";

#endif
