#include "main.h"

// for standard setup with pcb
#define SS_PIN 2
#define RST_PIN 5

// for compact programmer
// #define SS_PIN 22
// #define RST_PIN 21

const char* ssid = WIFI_SSID;
const char* password = WIFI_PSK;

wl_status_t wifiStatus = WL_IDLE_STATUS;
unsigned long lastWifiReconnect = 0;

MFRC522 mfrc522;

MFRC522::Uid uid;
MFRC522::StatusCode status; 

byte defaultKey[16] = {0x49, 0x45, 0x4D, 0x4B, 0x41, 0x45, 0x52, 0x42, // K7 ... K0 key 1
                       0x21, 0x4E, 0x41, 0x43, 0x55, 0x4F, 0x59, 0x46};// K7 ... K0 key 2

byte secretKey[16] = PICC_PSK;

byte readBuffer[32];
byte readBufferSize;
byte writeBuffer[4];

byte cardSecret[32];

bool redrawRequest, redrawing;

Backend backend;

State state;

int idleSelection;
bool idleListRedraw;

boolean provisioningSuccessful;

void setup() {
	Serial.begin(115200);
	Serial.println("hello world");

	WiFi.begin(ssid, password);
	SPI.begin();
	M5.begin();

	M5.Lcd.begin();

	lastWifiReconnect = millis();

	//disable Bluetooth
	btStop();

	redrawRequest = true;
	redrawing = false;

	M5.Lcd.fillCircle(80, 120, 20, TFT_LIGHTGREY);
	M5.Lcd.fillCircle(160, 120, 20, TFT_LIGHTGREY);
	M5.Lcd.fillCircle(240, 120, 20, TFT_LIGHTGREY);

	state = IDLE;
	idleSelection = 0;
}

void loop() {
	M5.update();

	redrawing = redrawRequest;
	redrawRequest = false;

	if (redrawing) M5.Lcd.clearDisplay();

	loop_wifi();
	loop_idle();
	loop_provisionCard();
	loop_clearCard();
	loop_debug();

	delay(1);
	yield();
}

bool loop_wifi() {
	wl_status_t status = WiFi.status();
	if (status != wifiStatus) {
		redrawRequest = true;
		wifiStatus = status;
	}

	if (redrawing) {
		M5.Lcd.setTextColor(TFT_WHITE);
		M5.Lcd.setTextDatum(TR_DATUM);
		M5.Lcd.setTextSize(1);
		M5.Lcd.drawString(backend.deviceMac.c_str(), 320, 0);
	}

	if (WiFi.status() == WL_CONNECTED) {
		if (redrawing) {
			M5.Lcd.setTextColor(TFT_GREEN);
			M5.Lcd.setTextDatum(TR_DATUM);
			M5.Lcd.setTextSize(1);
			M5.Lcd.drawString("WiFi CONN", 320, 12);
		}
		
		return true;
	} else {
		if (redrawing) {
			M5.Lcd.setTextColor(TFT_RED);
			M5.Lcd.setTextDatum(TR_DATUM);
			M5.Lcd.setTextSize(1);
			M5.Lcd.drawString("WiFi DISC", 320, 12);
		}

		Serial.println("WiFi not connected.");

		if (millis() - lastWifiReconnect > WIFI_RECONNECT_TIME) {
			Serial.println("WiFi: Trying to reconnect...");

			WiFi.disconnect(true);
			WiFi.mode(WIFI_OFF);
			WiFi.mode(WIFI_STA);
			WiFi.begin(ssid, password);

			lastWifiReconnect = millis();
		} else {
			Serial.println("WiFi: Last reconnect try within the last 5 seconds.");
		}

		return WiFi.status() == WL_CONNECTED;
	}
}

void loop_idle() {
	if (state == IDLE) {
		idleListRedraw = false;

		if (M5.BtnA.wasPressed()) {
			if (idleSelection > 0) --idleSelection;
			idleListRedraw = true;
		}
		if (M5.BtnB.wasPressed()) {
			if (idleSelection < 2) ++idleSelection;
			idleListRedraw = true;
		}
		if (M5.BtnC.wasPressed()) {
			if (idleSelection == 0) {
				provisioningSuccessful = false;
				state = PROVISION_CARD;
			} else if (idleSelection == 1) {
				provisioningSuccessful = false;
				state = CLEAR_CARD;
			} else if (idleSelection == 2) {
				provisioningSuccessful = false;
				state = DEBUG;
			}
			idleSelection = 0;
			redrawRequest = true;
		}

		if (redrawing || idleListRedraw) {
			M5.Lcd.setTextDatum(BC_DATUM);
			M5.Lcd.setTextColor(TFT_WHITE);
			M5.Lcd.setTextSize(1);
			M5.Lcd.drawString("[up]", 65, 240);
			M5.Lcd.drawString("[down]", 160, 240);
			M5.Lcd.drawString("[select]", 255, 240);


			M5.Lcd.setTextSize(3);
			int fontHeight = M5.Lcd.fontHeight(M5.Lcd.textfont);
			fontHeight = fontHeight + (fontHeight >> 1);// fontHeight *= 1.5

			M5.Lcd.setTextDatum(TL_DATUM);

			menuColor(idleSelection == 0);
			M5.Lcd.drawString("Provision Card", 0, 36 + (fontHeight * 0));
			
			menuColor(idleSelection == 1);
			M5.Lcd.drawString("Clear Card", 0, 36 + (fontHeight * 1));

			menuColor(idleSelection == 2);
			M5.Lcd.drawString("Debug", 0, 36 + (fontHeight * 2));
		}
	}
}

void menuColor(bool selected) {
	if (selected) {
		M5.Lcd.setTextColor(TFT_GREEN);
	} else {
		M5.Lcd.setTextColor(TFT_WHITE);
	}
}

void loop_provisionCard() {
	if (state == PROVISION_CARD) {
		if (redrawing) {
			M5.Lcd.setTextColor(TFT_WHITE);
			M5.Lcd.setTextSize(3);
			M5.Lcd.setTextDatum(TL_DATUM);
			M5.Lcd.drawString("Provision Card", 0, 0);

			M5.Lcd.setTextDatum(BC_DATUM);
			M5.Lcd.setTextColor(TFT_WHITE);
			M5.Lcd.setTextSize(1);
			M5.Lcd.drawString("[back]", 65, 240);
		}

		if (!provisioningSuccessful && !redrawRequest && !redrawing) {
			if (provisionCard()) {
				provisioningSuccessful = true;
				M5.Lcd.drawString("[new]", 255, 240);
			} else {
				if (M5.BtnA.isPressed()) {
					state = IDLE;
					redrawRequest = true;
				}
				debug("delay 500");
				delay(500);

				redrawRequest = true;
			}
		}

		if (provisioningSuccessful) {
			if (M5.BtnA.wasPressed()) {
				state = IDLE;
				redrawRequest = true;
			}
			if (M5.BtnC.wasPressed()) {
				provisioningSuccessful = false;
				state = PROVISION_CARD;
				redrawRequest = true;
			}
		}
	}
}

void loop_clearCard() {
	if (state == CLEAR_CARD) {
		if (redrawing) {
			M5.Lcd.setTextColor(TFT_WHITE);
			M5.Lcd.setTextSize(3);
			M5.Lcd.setTextDatum(TL_DATUM);
			M5.Lcd.drawString("Clear Card", 0, 0);

			M5.Lcd.setTextDatum(BC_DATUM);
			M5.Lcd.setTextColor(TFT_WHITE);
			M5.Lcd.setTextSize(1);
			M5.Lcd.drawString("[back]", 65, 240);
		}

		if (!provisioningSuccessful && !redrawRequest && !redrawing) {
			if (clearCard()) {
				provisioningSuccessful = true;
				M5.Lcd.drawString("[clear more]", 255, 240);
			} else {
				if (M5.BtnA.isPressed()) {
					state = IDLE;
					redrawRequest = true;
				}
				debug("delay 500");
				delay(500);

				redrawRequest = true;
			}
		}

		if (provisioningSuccessful) {
			if (M5.BtnA.wasPressed()) {
				state = IDLE;
				redrawRequest = true;
			}
			if (M5.BtnC.wasPressed()) {
				provisioningSuccessful = false;
				state = CLEAR_CARD;
				redrawRequest = true;
			}
		}
	}
}

void loop_debug() {
	if (state == DEBUG) {
		if (redrawing) {
			M5.Lcd.setTextColor(TFT_WHITE);
			M5.Lcd.setTextSize(3);
			M5.Lcd.setTextDatum(TL_DATUM);
			M5.Lcd.drawString("DEBUG", 0, 0);

			M5.Lcd.setTextDatum(BC_DATUM);
			M5.Lcd.setTextColor(TFT_WHITE);
			M5.Lcd.setTextSize(1);
			M5.Lcd.drawString("[back]", 65, 240);
		}

		if (!provisioningSuccessful && !redrawRequest && !redrawing) {
			if (debugCard()) {
				provisioningSuccessful = true;
			} else {
				if (M5.BtnA.isPressed()) {
					state = IDLE;
					redrawRequest = true;
				}
				debug("delay 500");
				delay(500);

				redrawRequest = true;
			}
		}

		if (provisioningSuccessful) {
			if (M5.BtnA.wasPressed()) {
				state = IDLE;
				redrawRequest = true;
			}
		}
	}
}

boolean initCardReader() {
	// hard reset PCD
    pinMode(RST_PIN, OUTPUT);
    digitalWrite(RST_PIN, LOW);
    delayMicroseconds(20); // 8.8.1 Reset timing requirements says about 100ns. Let us be generous: 2μs
    digitalWrite(RST_PIN, HIGH);

    // init PCD
    mfrc522.PCD_Init(SS_PIN, RST_PIN);

    // check PCD software version (0x92 is normal)
    byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    // When 0x00 or 0xFF is returned, communication probably failed
    if ((v == 0x00) || (v == 0xFF)) {
        debug(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
        return false;
    } else {
        info("MFRC522 version: 0x" + String(v, 16));
    }

    // leave antenna gain at default (0x40), more makes communication less reliable
    debug("Antenna Gain: 0x" + String(mfrc522.PCD_GetAntennaGain(), 16));

    // Reset baud rates
    mfrc522.PCD_WriteRegister(MFRC522::TxModeReg, 0x00);
    mfrc522.PCD_WriteRegister(MFRC522::RxModeReg, 0x00);
    // Reset ModWidthReg
    mfrc522.PCD_WriteRegister(MFRC522::ModWidthReg, 0x26);

    // wait for "startup"
    delay(5);

	//reset uid buffer
    uid.size = 0;
    uid.uidByte[0] = 0;
    uid.uidByte[1] = 0;
    uid.uidByte[2] = 0;
    uid.uidByte[3] = 0;
    uid.uidByte[4] = 0;
    uid.uidByte[5] = 0;
    uid.uidByte[6] = 0;
    uid.uidByte[7] = 0;
    uid.uidByte[8] = 0;
    uid.uidByte[9] = 0;
    uid.sak = 0;

	return true;
}

boolean wakeupAndSelect() {
	byte waBufferATQA[2];
    byte waBufferSize = 2;

    waBufferATQA[0] = 0x00;
    waBufferATQA[1] = 0x00;

    status = mfrc522.PICC_WakeupA(waBufferATQA, &waBufferSize);
    
    debug("CardReader::read WakeupA status = ");
    debug(MFRC522::GetStatusCodeName(status));

    if (status != MFRC522::STATUS_OK) {
        endCard();
        return false;
    }

    info("ATQA: ");
    infoByteArray(waBufferATQA, waBufferSize);

	if (waBufferATQA[0] != 0x44 || waBufferATQA[1] != 0x00) {
		error("Only Ultralight C supported!");
		endCard();
		return false;
	}

    status = mfrc522.PICC_Select(&uid, 0);
    debug("Select status = ");
    debug(MFRC522::GetStatusCodeName(status));

    if (status != MFRC522::STATUS_OK) {
        endCard();
        return 1;
    }

    info("Uid: ");
    infoByteArray(uid.uidByte, uid.size);

	return true;
}

boolean authWithDefaultKey() {
    status = mfrc522.MIFARE_UL_C_Auth(defaultKey);
    debug("auth with default key = ");
    debug(MFRC522::GetStatusCodeName(status));
    if (status != MFRC522::STATUS_OK) {
        endCard();
        return false;
    }
	info("authentication with default key success");
	return true;
}


boolean authWithSecretKey() {
    status = mfrc522.MIFARE_UL_C_Auth(secretKey);
    debug("auth with secret key = ");
    debug(MFRC522::GetStatusCodeName(status));
    if (status != MFRC522::STATUS_OK) {
        endCard();
        return false;
    }
	info("authentication with secret key success");
	return true;
}

boolean writeCardSecret() {
	for (int i = 0; i < 32; i++) {
		cardSecret[i] = esp_random();
	}

	debug("cardSecret generated: ");
	debugByteArray(cardSecret, 32);
	info("cardSecret generated");

	for (int p = 0x20; p <= 0x27; p++) {
		debug("writing cardSecret page " + String(p) + "...");
		memcpy(writeBuffer, &cardSecret[(p - 0x20) * 4], 4);
		debugByteArray(writeBuffer, 4);

		status = mfrc522.MIFARE_Ultralight_Write(p, writeBuffer, 4);
		if (status != MFRC522::STATUS_OK) {
			debug("Write status = ");
			debug(MFRC522::GetStatusCodeName(status));
			endCard();
			return false;
		}
	}

	info("cardSecret written");
	return true;
}

boolean writeSecretKey() {
	status = mfrc522.MIFARE_UL_C_WriteKey(secretKey);
    if (status != MFRC522::STATUS_OK) {
        debug("WriteKey status = ");
        debug(MFRC522::GetStatusCodeName(status));
        endCard();
        return false;
    }
	info("secret key written");
	return true;
}

void arrayToString(byte array[], unsigned int len, char buffer[]){
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

boolean writeZeros() {
	memset(writeBuffer, 0, 4);
	for (int p = 0x04; p <= 0x27; p++) {
		debug("zeroing page " + String(p) + "...");

		status = mfrc522.MIFARE_Ultralight_Write(p, writeBuffer, 4);
		if (status != MFRC522::STATUS_OK) {
			debug("Write status = ");
			debug(MFRC522::GetStatusCodeName(status));
			endCard();
			return false;
		}
	}

	if (!writeAuth0(0x30)) {
		error("Could not write auth0 (to 0x30)!");
		return false;
	}

	info("card zeroed");
	return true;
}

boolean writeDefaultKey() {
	status = mfrc522.MIFARE_UL_C_WriteKey(defaultKey);
    if (status != MFRC522::STATUS_OK) {
        debug("WriteKey status = ");
        debug(MFRC522::GetStatusCodeName(status));
        endCard();
        return false;
    }
	info("default key written");
	return true;
}

boolean writeAuth0(byte auth0) {
	writeBuffer[0] = auth0;
	writeBuffer[1] = 0;
	writeBuffer[2] = 0;
	writeBuffer[3] = 0;
	status = mfrc522.MIFARE_Ultralight_Write(0x2A, writeBuffer, 4);
	if (status != MFRC522::STATUS_OK) {
		debug("Write status = ");
		debug(MFRC522::GetStatusCodeName(status));
		endCard();
		return false;
	}
	info("auth0 written");
	return true;
}

boolean provisionCard() {
	M5.Lcd.setTextSize(1);
	M5.Lcd.setCursor(0, 40);
	info("privisionCard");

	if(!initCardReader()) {
		error("Could not initialize RC522!");
		return false;
	}

	if (!wakeupAndSelect()) {
		error("Could not select card!");
		return false;
	}

	if (!authWithDefaultKey()) {
		error("Could not authenticate with default key!");
		return false;
	}

	if (!writeCardSecret()) {
		error("Could not write card secret!");
		return false;
	}

	if (!writeSecretKey()) {
		error("Could not write secret key!");
		return false;
	}

	if (!writeAuth0(0x20)) {
		error("Could not write auth0 (to 0x20)!");
		return false;
	}

	// display qr code
	char qr[255] = "";
	arrayToString(uid.uidByte, 7, qr);

	qr[14] = '\n';
	arrayToString(cardSecret, 32, &qr[15]);

	M5.Lcd.qrcode(qr);
	return true;
}

boolean clearCard() {
	M5.Lcd.setTextSize(1);
	M5.Lcd.setCursor(0, 40);
	info("clearCard");
	debug("clearCard");

	if(!initCardReader()) {
		error("Could not initialize RC522!");
		return false;
	}

	if (!wakeupAndSelect()) {
		error("Could not select card!");
		return false;
	}

	if (!authWithSecretKey()) {
		error("Could not authenticate with secret key!");
		return false;
	}

	if (!writeZeros()) {
		error("Could not write zeros!");
		return false;
	}

	if (!writeDefaultKey()) {
		error("Could not write default key!");
		return false;
	}

	if (!writeAuth0(0x30)) {
		error("Could not write auth0 (to 0x30)!");
		return false;
	}

	return true;
}

boolean debugCard() {
	M5.Lcd.setTextSize(1);
	M5.Lcd.setCursor(0, 40);
	info("debugCard");
	debug("debugCard");

	if(!initCardReader()) {
		error("Could not initialize RC522!");
		return false;
	}

	if (!wakeupAndSelect()) {
		error("Could not select card!");
		return false;
	}

	if (!authWithSecretKey()) {
		error("Could not authenticate with secret key!");
		return false;
	}

	for (int p = 0; p <= 0x28; p += 4) {
		readBufferSize = 32;
		status = mfrc522.MIFARE_Read(p, readBuffer, &readBufferSize);
		if (status != MFRC522::STATUS_OK) {
			debug("Read status = ");
			debug(MFRC522::GetStatusCodeName(status));
			endCard();
			return false;
		}
		debugByteArray(readBuffer, readBufferSize);
	}

	if (status == MFRC522::STATUS_OK) {
		return true;
	}

	return true;
}

void endCard() {
    mfrc522.PCD_StopCrypto1();
    mfrc522.PICC_HaltA();
}

void error(String msg) {
	Serial.print("[E] ");
	Serial.println(msg);
	M5.Lcd.print("[E] ");
	M5.Lcd.println(msg);
}

void info(String msg) {
	Serial.print("[I] ");
	Serial.println(msg);
	M5.Lcd.print("[I] ");
	M5.Lcd.println(msg);
}

void infoByteArray(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
        M5.Lcd.print(buffer[i] < 0x10 ? " 0" : " ");
        M5.Lcd.print(buffer[i], HEX);
    }
    Serial.println();
    M5.Lcd.println();
}

void debug(String msg) {
	Serial.print("[D] ");
	Serial.println(msg);
}

void debugByteArray(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
    Serial.println();
}