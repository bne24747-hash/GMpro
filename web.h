const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<style>
  body { background:#000; color:#0f0; font-family:monospace; padding:10px; }
  .tab { display:none; border:1px solid #0f0; padding:10px; margin-top:5px; }
  .active { display:block; }
  .t-btn { padding:10px; background:#111; border:1px solid #0f0; color:#0f0; cursor:pointer; }
  .t-btn.sel { background:#0f0; color:#000; }
  table { width:100%; border-collapse:collapse; margin:10px 0; }
  th, td { border:1px solid #444; padding:5px; text-align:center; }
  .rusuh { border-color:red; color:red; }
</style>
</head>
<body>
  <div style="display:flex; gap:5px;">
    <button class="t-btn sel" id="b1" onclick="st(1)">TAB 1: ATTACK</button>
    <button class="t-btn" id="b2" onclick="st(2)">TAB 2: SETTINGS</button>
  </div>

  <div id="p1" class="tab active">
    <button class="t-btn" onclick="cmd('scan')">SCAN WIFI</button>
    <table>
      <thead><tr><th>SSID</th><th>CH</th><th>USR</th><th>SIG%</th><th>SELECT</th></tr></thead>
      <tbody id="l"></tbody>
    </table>
    <button class="t-btn" onclick="cmd('deauth')">DEAUTH</button>
    <button class="t-btn" onclick="cmd('etwin')">EVIL TWIN</button>
    <button class="t-btn" onclick="cmd('beacon')">BEACON SPAM</button>
    <button class="t-btn rusuh" onclick="cmd('rusuh')">MASS DEAUTH RUSUH</button>
    <textarea id="log" style="width:100%; height:100px; background:#111; color:#0f0; margin-top:10px;" readonly></textarea>
  </div>

  <div id="p2" class="tab">
    SSID: <input type="text" id="ss" value="GMpro2"><br>
    HIDDEN ADMIN: <select id="ha"><option value="0">OFF</option><option value="1">ON</option></select><br>
    MAX SIGNAL: 20.5 dBm (Fixed)<br><hr>
    FILE: <select id="sl"><option>etwin1.html</option><option>etwin2.html</option><option>etwin3.html</option><option>etwin4.html</option><option>etwin5.html</option></select>
    <input type="file" id="fi"><button onclick="up()">UPLOAD</button><br>
    <textarea id="pt" placeholder="pass.txt content..."></textarea>
  </div>

<script>
  function st(n){
    document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));
    document.querySelectorAll('.t-btn').forEach(x=>x.classList.remove('sel'));
    document.getElementById('p'+n).classList.add('active');
    document.getElementById('b'+n).classList.add('sel');
  }
  function cmd(a){
    fetch('/api?do='+a).then(r=>r.text()).then(t=>{document.getElementById('log').value += "> "+t+"\n";});
  }
  function up(){
    let f = document.getElementById('fi').files[0];
    let d = new FormData(); d.append("file", f, document.getElementById('sl').value);
    fetch('/upload',{method:"POST",body:d}).then(()=>alert("Uploaded!"));
  }
</script>
</body>
</html>
)=====";
