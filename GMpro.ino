<html>
<head>
    <meta name='viewport' content='width=device-width,initial-scale=1.0'>
    <style>
        /* TEMA DARK CYBER GMPRO87 */
        body { background: #0a0a0a; color: #fff; font-family: sans-serif; text-align: center; margin: 0; padding: 10px; }
        .content { max-width: 500px; margin: auto; background: #151515; padding: 20px; border-radius: 12px; border: 1px solid #333; }
        h1 { color: #ee2e24; text-transform: uppercase; letter-spacing: 3px; margin-bottom: 20px; font-size: 1.5em; }
        
        /* SISTEM GRID 2x2 UNTUK 4 TOMBOL UTAMA */
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 20px; }
        
        /* TOMBOL KOMANDO */
        .btn { padding: 15px; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; font-size: 10px; transition: 0.3s; text-transform: uppercase; text-decoration: none; display: inline-block; }
        
        /* STATUS AKTIF (GLOW RED) */
        .on { background: #ee2e24; box-shadow: 0 0 15px #ee2e24; color: #fff; text-shadow: 0 0 5px #fff; } 
        /* STATUS MATI (OFF) */
        .off { background: #444; color: #888; } 

        /* TABEL LIST WIFI */
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th { color: #ee2e24; font-size: 11px; border-bottom: 2px solid #333; padding-bottom: 10px; }
        td { border-bottom: 1px solid #222; padding: 10px; font-size: 10px; }
        
        /* STATUS SELECT DI TABEL */
        .sel { background: #2e7d32; color: #fff; border-radius: 4px; padding: 5px; }
        .unsel { background: #555; color: #ccc; border-radius: 4px; padding: 5px; }

        /* AREA LOG SISTEM (HITAM HIJAU) */
        .log-container { margin-top: 20px; text-align: left; background: #000; padding: 10px; border: 1px solid #ee2e24; border-radius: 6px; font-family: monospace; font-size: 10px; overflow-y: auto; max-height: 150px; }
        .log-title { color: #ee2e24; font-weight: bold; margin-bottom: 5px; text-transform: uppercase; border-bottom: 1px solid #333; display: block; }
        .log-entry { color: #0f0; margin-bottom: 2px; line-height: 1.4; }
    </style>
</head>
<body>
    <div class='content'>
        <h1>GMpro87 V2</h1>

        <div class='grid'>
            <a href='/?act=deauth' class='btn on'>DEAUTH TARGET</a>  
            <a href='/?act=evil' class='btn off'>EVIL TWIN</a>
            <a href='/?act=beacon' class='btn off'>BEACON SPAM</a>
            <a href='/?act=mass' class='btn off'>MASS DEAUTH</a>
        </div>
        
        <a href='/?clear=1' class='btn' style='background:#800; width:100%; margin-bottom:15px;'>CLEAR SYSTEM LOG</a>

        <table>
            <tr>
                <th>SSID</th>
                <th>CH</th>
                <th>SIG</th>
                <th>SELECT</th>
            </tr>
            <tr>
                <td>TARGET_WIFI_A</td>
                <td>6</td>
                <td>85%</td>
                <td><a href='/?sel=0' class='btn sel'>ACTIVE</a></td>
            </tr>
            <tr>
                <td>FREE_WIFI</td>
                <td>11</td>
                <td>40%</td>
                <td><a href='/?sel=1' class='btn unsel'>SEL</a></td>
            </tr>
        </table>

        <div class='log-container'>
            <span class='log-title'>System & Password Log</span>
            <div class='log-entry'>
                [SYSTEM] AP_STA Mode Active<br>
                [ATTACK] Deauth started on TARGET_WIFI_A<br>
                [LOG] Waiting for handshake validation...
            </div>
        </div>
    </div>
</body>
</html>
