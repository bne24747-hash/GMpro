<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP-TERMINATOR V3</title>
    <style>
        :root { --c: #00f2ff; --r: #ff3e3e; --g: #00ff9d; --bg: #050505; --card: #111; }
        * { margin:0; padding:0; box-sizing:border-box; font-family:monospace; }
        body { background: var(--bg); color: #eee; padding: 10px; }
        
        /* Tab Navigation */
        .tabs { display: flex; gap: 5px; margin-bottom: 15px; }
        .tab-btn { flex: 1; padding: 12px; background: #1a1a1a; border: 1px solid #333; color: #888; cursor: pointer; font-weight: bold; border-radius: 4px; }
        .tab-btn.active { border-bottom: 3px solid var(--c); color: var(--c); background: #222; }
        .content { display: none; }
        .content.active { display: block; }

        /* Cards & UI Elements */
        .card { background: var(--card); border: 1px solid #222; padding: 15px; border-radius: 8px; margin-bottom: 12px; }
        h3 { color: var(--c); font-size: 0.9rem; margin-bottom: 12px; text-transform: uppercase; border-left: 3px solid var(--c); padding-left: 10px; }
        
        /* Buttons */
        .btn { width: 100%; padding: 14px; margin: 5px 0; background: #1a1a1a; border: 1px solid #444; color: #fff; font-weight: bold; cursor: pointer; text-align: left; position: relative; border-radius: 4px; }
        .btn.on { border-color: var(--c); color: var(--c); box-shadow: 0 0 10px rgba(0,242,255,0.1); }
        .btn.on::after { content: 'ON'; position: absolute; right: 15px; font-size: 10px; background: var(--c); color: #000; padding: 2px 6px; border-radius: 3px; }
        .btn.danger.on { border-color: var(--r); color: var(--r); }
        .btn.danger.on::after { background: var(--r); content: 'RUSUH'; }

        /* WiFi Table */
        .scroll { overflow-x: auto; margin-top: 10px; }
        table { width: 100%; border-collapse: collapse; font-size: 12px; }
        th { text-align: left; color: #666; padding: 10px; border-bottom: 1px solid #333; }
        td { padding: 10px; border-bottom: 1px solid #1a1a1a; }
        .sel-btn { background: none; border: 1px solid var(--c); color: var(--c); padding: 4px 8px; border-radius: 3px; cursor: pointer; }

        /* Logs & Loot */
        .log-box { background: #000; height: 120px; overflow-y: scroll; padding: 10px; font-size: 11px; color: var(--g); border: 1px solid #222; border-radius: 4px; }
        input[type=file] { margin: 10px 0; color: #888; font-size: 12px; }
    </style>
</head>
<body>

    <div class="tabs">
        <button class="tab-btn active" onclick="openTab(event, 'atk')">ATTACK</button>
        <button class="tab-btn" onclick="openTab(event, 'fil')">FILES</button>
        <button class="tab-btn" onclick="openTab(event, 'set')">SET</button>
    </div>

    <div id="atk" class="content active">
        <div class="card">
            <h3>WiFi Scanner</h3>
            <button class="btn" style="text-align:center" onclick="scan()">SCAN WIFI (INC. HIDDEN)</button>
            <div class="scroll">
                <table>
                    <thead><tr><th>SSID</th><th>CH</th><th>SIG</th><th>ACT</th></tr></thead>
                    <tbody id="list"></tbody>
                </table>
            </div>
        </div>

        <div class="card">
            <h3>Attack Engine</h3>
            <button id="mKill" class="btn danger" onclick="tg('kill', 'mKill')">MASS DEAUTH (RUSUH)</button>
            <button id="tDeauth" class="btn" onclick="tg('deauth', 'tDeauth')">DEAUTH TARGET</button>
            <button id="bSpam" class="btn" onclick="tg('spam', 'bSpam')">BEACON SPAM</button>
            <button id="eTwin" class="btn" onclick="tg('evil', 'eTwin')">EVIL TWIN PORTAL</button>
        </div>
        
        <div class="card">
            <h3>System Log</h3>
            <div class="log-box" id="logs">Ready to attack...</div>
        </div>
    </div>

    <div id="fil" class="content">
        <div class="card">
            <h3>File Manager</h3>
            <form method='POST' action='/upload' enctype='multipart/form-data'>
                <input type="file" name="f">
                <input type="submit" value="UPLOAD HTML" class="btn" style="text-align:center">
            </form>
        </div>
        <div class="card">
            <h3>Captured Passwords</h3>
            <div class="log-box" id="loot" style="color:#f1c40f">--- pass.txt ---</div>
            <button class="btn" style="text-align:center" onclick="loadLoot()">REFRESH LOG</button>
        </div>
    </div>

    <div id="set" class="content">
        <div class="card">
            <h3>Admin Config</h3>
            <input type="text" id="aS" placeholder="Admin SSID" class="btn" style="text-align:left">
            <input type="text" id="aP" placeholder="Admin Password" class="btn" style="text-align:left">
            <button class="btn on" style="text-align:center">SAVE & RESTART</button>
        </div>
    </div>

    <script>
        function openTab(e, n) {
            document.querySelectorAll('.content').forEach(c => c.classList.remove('active'));
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.getElementById(n).classList.add('active');
            e.currentTarget.classList.add('active');
        }

        function scan() {
            document.getElementById('logs').innerHTML += "<br>[SCAN] Scanning...";
            fetch('/scan').then(r => r.json()).then(data => {
                let h = '';
                data.forEach(n => {
                    h += `<tr><td>${n.s}</td><td>${n.c}</td><td>${n.r}%</td>
                    <td><button class="sel-btn" onclick="sel('${n.b}','${n.s}',${n.c})">SEL</button></td></tr>`;
                });
                document.getElementById('list').innerHTML = h;
            });
        }

        function sel(b, s, c) {
            fetch(`/select?b=${b}&s=${s}&c=${c}`).then(() => {
                alert("Locked: " + s);
                document.getElementById('logs').innerHTML += `<br>[TGT] Locked on ${s}`;
            });
        }

        function tg(m, id) {
            fetch('/toggle?m=' + m).then(() => {
                document.getElementById(id).classList.toggle('on');
            });
        }

        function loadLoot() {
            // Logika ambil isi pass.txt bisa lo tambah di backend
        }
    </script>
</body>
</html>
