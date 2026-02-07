#ifndef WEB_H
#define WEB_H

static const char Head[] PROGMEM = R"=====(<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
body{background:#000;color:#fff;font-family:monospace;margin:0;text-align:center}
.header{border-bottom:2px solid #f00;padding:20px;box-shadow:0 0 15px #f00}
.logo{color:#f00;font-size:2.5rem;font-weight:700;text-shadow:0 0 10px #f00;letter-spacing:5px}
.tabs{display:flex;background:#222}
.tab{flex:1;padding:15px;cursor:pointer;font-weight:700;border-bottom:3px solid #333}
.tab.active{background:#111;border-bottom:4px solid #f00;color:#f00}
.tab.set{color:#0f0}
.content{padding:15px;display:none}
.active-c{display:block}
.card{background:#111;border:1px solid #333;padding:15px;margin-top:10px;border-radius:5px;text-align:left}
.card-t{color:#f00;font-size:12px;margin-bottom:10px;border-left:4px solid #f00;padding-left:10px;text-transform:uppercase}
table{width:100%;font-size:11px;border-collapse:collapse}
th,td{border:1px solid #222;padding:8px}
th{color:#0f0}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:10px}
button{background:#000;border:1px solid #0f0;color:#0f0;padding:15px;font-weight:700;border-radius:5px;cursor:pointer}
button.active{background:#f00!important;color:#fff!important;border-color:#fff!important;box-shadow:0 0 15px #f00}
</style></head><body>
<div class='header'><div class='logo'>GMpro87</div></div>
<div class='tabs'>
<div class='tab active' onclick="openT(event,'T1')">COMBAT</div>
<div class='tab set' onclick="openT(event,'T2')">SETTING</div>
</div>
<div id='T1' class='content active-c'>)=====";

static const char Mid[] PROGMEM = R"=====(</div><div id='T2' class='content'>)=====";

static const char Foot[] PROGMEM = R"=====(</div>
<script>
function openT(e,n){
var i,c,t;
c=document.getElementsByClassName("content");for(i=0;i<c.length;i++)c[i].style.display="none";
t=document.getElementsByClassName("tab");for(i=0;i<t.length;i++)t[i].className=t[i].className.replace(" active","");
document.getElementById(n).style.display="block";e.currentTarget.className+=" active";
}
function tgl(btn,u){
fetch(u).then(r=>r.text()).then(d=>{
if(d.includes("ON"))btn.classList.add("active");
else btn.classList.remove("active");
});
}
function scn(){
fetch('/get_scan').then(r=>r.json()).then(data=>{
let b=document.getElementById('sc');b.innerHTML='';
data.forEach(w=>{
b.innerHTML+=`<tr><td>${w.s}</td><td>${w.c}</td><td>0</td><td>${w.r}%</td><td><input type='checkbox' onclick='fetch("/sel?m="+${w.m})'></td></tr>`;
});
});
}
setInterval(scn,5000);
</script></body></html>)=====";

#endif
