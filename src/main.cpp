#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#include "credentials.h"

FirebaseData fbdo;
FirebaseData stream;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis1;

char data[100];
char error_code = 0;

void wait_for_ok(void);
void write_data(char data[100], int len);
void write_error(char error_code);
int connect_to_internet(void);
void str_to_bytes(const char *str, char data[95]);
char hexstr_to_byte(char hexstr[3]);

void setup() {
  	Serial.begin(115200);
	Serial.setTimeout(200);

	delay(2000);

	pinMode(2, OUTPUT);

	WiFi.mode(WIFI_STA);
	while (true) {
		delay(2000);
		int conn = connect_to_internet();
		if (conn == 0) {
			break;
		}

		if (conn == 2) {
			error_code = 1;
			break;
		}
	}

	if (error_code == 1)
		goto SETUP_DONE;

	auth.user.email = USER_EMAIL;
	auth.user.password = USER_PASSWORD;

	config.api_key = API_KEY;
	config.database_url = DATABASE_URL;

	Firebase.begin(&config, &auth);
	Firebase.reconnectWiFi(true);

	if (!Firebase.RTDB.beginStream(&stream, "/bigtimer/state")) {
		error_code = 3;
		Serial.printf("sream begin error, %s\n", stream.errorReason().c_str());
	}

SETUP_DONE:
	Serial.println("WIFI OK");
}

void loop() {
	if (!Firebase.ready()) {
		if (error_code > 0)
			write_error(error_code);

		return;
	}

	if (!stream.httpConnected()) {
		error_code = 4;
		Serial.printf("error code: %d, reason: %s\n", stream.httpCode(), stream.errorReason().c_str());
	} else if (!Firebase.RTDB.readStream(&stream)) {
		error_code = 5;
		Serial.printf("stream read error, %s\n", stream.errorReason().c_str());
	}

	if (error_code > 0) {
		write_error(error_code);
	} else if (stream.streamAvailable()) {
		memset(data, 0, 100);
		String state = stream.to<String>();

		const char *state_c = state.c_str();
		memcpy(data, state_c, 5);
		str_to_bytes(&state_c[5], &data[5]);

		int len = 5 + (state.length() - 5) / 2;
		write_data(data, len);
	}

	if (error_code > 3)
		error_code = 0;
}

void wait_for_ok(void) {
	char buf[5];
	while (strncmp(buf, "OK", 2) != 0) {
		Serial.readBytes(buf, 3);
	}
}

void write_data(char data[100], int len) {
	wait_for_ok();

	for (int i = 0; i < len; i++) {
		Serial.write(data[i]);
	}
}

void write_error(char code) {
	memcpy(data, "ERROR", 5);
	data[5] = error_code;
	write_data(data, 6);
}

void str_to_bytes(const char *str, char data[95]) {
	char hexstr[3] = { 0, 0, 0 };

	for (int i = 0; str[i] != 0 && str[i + 1] != 0; i += 2) {
		hexstr[0] = str[i];
		hexstr[1] = str[i + 1];

		data[i / 2] = hexstr_to_byte(hexstr);
	}
}

char hexstr_to_byte(char hexstr[3]) {
	return strtoul(hexstr, nullptr, 16);
}

/*
	ret 0 = connected
	1 = error, retry
	2 = error, don't retry
*/
int connect_to_internet(void) {
	WiFi.begin(SSID, PWD);

	while (WiFi.status() != WL_CONNECTED) {
		digitalWrite(2, LOW);
		delay(1000);
		digitalWrite(2, HIGH);
		delay(1000);

		wl_status_t stat = WiFi.status();
		if (stat == 4) {
			return 1;
		}

		// TODO no ssid error
		// TODO connection lost error
		// TODO wrong password error
	}

	return 0;
}
