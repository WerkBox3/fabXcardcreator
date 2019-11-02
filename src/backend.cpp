#include "backend.h"
#include <HTTPClient.h>

void Backend::begin() {
    // read device mac into String
    byte mac[6];
    WiFi.macAddress(mac);
    char macBuffer[13];
    sprintf(macBuffer, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    deviceMac = String(macBuffer);
}