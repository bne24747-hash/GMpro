#include <ESP8266WiFi.h>

void setup_sniffer() {
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(true);
}

void stop_sniffer() {
  wifi_promiscuous_enable(false);
}
