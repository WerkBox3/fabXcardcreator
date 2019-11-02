#ifndef MAIN_H
#define MAIN_H

#define WIFI_RECONNECT_TIME 5000 // how long the ESP should wait until it disables and reenables WiFi if it cannot connect
#define NTP_SERVER "de.pool.ntp.org"
#define TZ_INFO "WEST-1DWEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00" // Western European Time

#include <Arduino.h>
#include <SPI.h>
#include <M5Stack.h>
#include <WiFi.h>
#include <esp32-hal-bt.c>
#include <EEPROM.h>
#include <MFRC522.h>
#include "backend.h"
#include "config.h"

bool loop_wifi();
void loop_idle();
void loop_provisionCard();
void loop_clearCard();
void loop_debug();
void menuColor(bool selected);

boolean initCardReader();
boolean wakeupAndSelect();
boolean authWithDefaultKey();
boolean authWithSecretKey();
boolean writeCardSecret();
boolean writeSecretKey();
boolean writeZeros();
boolean writeDefaultKey();
boolean writeAuth0(byte auth0);
boolean provisionCard();
boolean clearCard();
boolean debugCard();
void endCard();

void error(String msg);
void info(String msg);
void infoByteArray(byte *buffer, byte bufferSize);
void debug(String msg);
void debugByteArray(byte *buffer, byte bufferSize);

enum State { IDLE, PROVISION_CARD, CLEAR_CARD, DEBUG };

#endif //MAIN_H