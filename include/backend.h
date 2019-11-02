#ifndef BACKEND_H
#define BACKEND_H

#include <Arduino.h>
#include <M5Stack.h>
#include <WiFi.h>
#include <MFRC522.h>
#include "config.h"

class Backend {
    public:
        String deviceMac;
        String secret;

        void begin();
        
    private:
};

#endif //BACKEND_H