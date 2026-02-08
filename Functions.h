/* GMpro Backend Logic */
bool deauthActive = false;
bool beaconActive = false;
bool rusuhMode = false;
String targetMAC = "";
int beaconTotal = 10;

void handleAPI() {
  String action = server.arg("do");
  
  if (action == "scan") {
    // Mode unmask hidden SSID
    WiFi.scanNetworks(false, true); 
    server.send(200, "text/plain", "Scan Complete");
  } 
  else if (action == "rusuh") {
    rusuhMode = !rusuhMode;
    deauthActive = rusuhMode;
    server.send(200, "text/plain", rusuhMode ? "RUSUH ON" : "RUSUH OFF");
  }
  else if (action == "deauth") {
    deauthActive = !deauthActive;
    server.send(200, "text/plain", "Deauth Toggled");
  }
  // Tambahkan logic save/load EEPROM di sini agar restart 1000x tetap jalan
}

void runAttackEngine() {
  if (deauthActive) {
    if (rusuhMode) {
      // Mass Deauth: Semua channel disikat
      for (int ch = 1; ch <= 13; ch++) {
        // Proteksi: Lewati MAC Wemos sendiri agar admin tidak logout
        sendRawDeauth("FF:FF:FF:FF:FF:FF", ch);
      }
    } else {
      sendRawDeauth(targetMAC, WiFi.channel());
    }
  }
  
  if (beaconActive) {
    // Beacon spam sesuai jumlah set
    for (int i=0; i < beaconTotal; i++) {
       // logic beacon frames
    }
  }
}

void sendRawDeauth(String mac, int ch) {
  // Logic injeksi paket 802.11 deauth
}

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  File file = LittleFS.open("/" + upload.filename, "w");
  if (file) {
    file.write(upload.buf, upload.currentSize);
    file.close();
  }
}

void resumeLastAttack() {
  // Baca status dari EEPROM, jika 1 maka set deauthActive = true
}
