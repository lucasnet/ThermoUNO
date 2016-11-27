
#include <SoftwareSerial.h>
#include <SimpleDHT.h>

#define RX_ESP 10		// Arduino RX (from Esp TX)
#define TX_ESP 11		// Arduino TX (to Esp RX)
#define RXTX_DHT11 2	// Arduino DHT11

#define BRIKSDALL_URL "192.168.1.33"		// Briksdall Url
#define BRIKSDALL_PORT "81"					// Briksdall Port
#define BRIKSDALL_WS "/ws/hub.ashx"			// Briksdall Entry Point
#define BRIKSDALL_TIMEOUT 5000;				// Briksdall Connection Timeout (milliseconds)

SoftwareSerial esp8266(RX_ESP, TX_ESP);
SimpleDHT11 dht11;

byte current_T = 0;		// Temperature current value
byte current_H = 0;		// Humidity current value

// for DHT11, 
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 2

void setup()
{
	Serial.begin(9600);		// Log aulla seriale standard.
	esp8266.begin(115200);	// comunicazione con esp8266

	setupWiFi();			// setup WiFi
}



void loop()
{

	/* debug/echo seriale-esp */
	//while (esp8266.available()) {
	//	Serial.print((char) esp8266.read());
	//}

	//while (Serial.available()) {
	//	esp8266.write(Serial.read());
	//}

	//delay(2000);
	/* fine debug serial-esp */

	
	bool sendData = readDHT11();;
	Serial.println("DHT11: T=" + String(current_T) + " H=" + String(current_H));
	if (sendData) {
		String type = "T";
		String value = String(current_T);

		bool sent = send2Briksdall(type, value);
	}

}


bool readDHT11() {
	
	bool retVal = false;
	int pinDHT11 = RXTX_DHT11;
	delay(1000);	// DHT11 sampling rate is 1HZ.

	// read with raw sample data.
	byte temperature = 0;
	byte humidity = 0;
	byte data[40] = { 0 };
	if (dht11.read(pinDHT11, &temperature, &humidity, data)) {
		Serial.print("DHT11: Read failed.");
		return false;
	}

	retVal = (current_T != temperature);
	if (retVal) {
		current_T = temperature;
		current_H = humidity;
	}

	return retVal;
}



bool send2Briksdall(String type, String value) {
	String timestamp = String(millis());

	String logmessage = "Invio dati, tentativo ";
	bool transmissionOK = false;
	int i = 1;
	do {
		Serial.println("-----------------------");
		Serial.println(logmessage + String(i));
		transmissionOK = httppost(timestamp, type, value);
		i++;
	} while ((!transmissionOK) && (i < 11));

	if (transmissionOK) {
		Serial.println("Invio dati completato con successo.");
	}
	else {
		Serial.println("Invio dati non riuscito.");
	}

	return transmissionOK;
}

// Configurazione WiFi
void setupWiFi() {
	// Impostazioni network Esp8266 (WiFi + Lan)
	// WIFI_SSID "Telecom-33318697" -> non serve perche impostato di default (attraverso AT+CWJAP_DEF)
	// WIFI_PSW "qwertyuiopASDFGHJKL123456789 -> non serve perche impostato di default (attraverso AT+CWJAP_DEF)
	// IPADDR "192.168.1.100" -> non serve perche impostato di default (attraverso AT+CIPSTA_DEF)
	// NETMASK "255.255.255.0" -> non serve perche impostato di default (attraverso AT+CIPSTA_DEF)
	// GATEWAY "192.168.1.1" -> non serve perche impostato di default (attraverso AT+CIPSTA_DEF)

	long timeout = BRIKSDALL_TIMEOUT;
	sendByEsp8266("ATE0", timeout);				// echo off
	sendByEsp8266("AT+CIPMUX=1", timeout);		// configura per connessioni multiple
}

bool httppost(String timestamp, String type, String value) {

	String server = BRIKSDALL_URL;
	String port = BRIKSDALL_PORT;
	String ws = BRIKSDALL_WS;
	long timeout = BRIKSDALL_TIMEOUT;
	String datarcv = "";
	bool operationOK = true;
	String data2send = "";

	// Connessione...
	Serial.println("ESP8266: Connecting to: " + server + " on port " + port + " ...");
	data2send = "AT+CIPSTART=1,\"TCP\",\"" + server + "\"," + port;
	datarcv = sendByEsp8266(data2send, timeout);
	Serial.println("ESP8266: Data Received: " + datarcv);
	// fine connessione

	// Stato Connessione...
	operationOK = received_ok(datarcv);
	if (!operationOK) {
		Serial.println("ESP8266: TCP connection KO");
		data2send = "AT+CIPCLOSE=1";
		sendByEsp8266(data2send, timeout);
		return false;
	}
	Serial.println("ESP8266: Connected to " + server + " on port " + port + ".");
	// Fine stato connessione


	// invio dati step 1 lunghezza ...
	String uri = ws + "/esp8266";
	String rawdata = "timestamp=" + timestamp + "&" + type + "=" + value;
	String postRequest =
		"POST " + uri + " HTTP/1.0\r\n" +
		"Host: " + server + ":" + port + "\r\n" +
		"Accept: */*\r\n" +
		"Content-Length: " + String(rawdata.length()) + "\r\n" +
		"Content-Type: application/x-www-form-urlencoded\r\n" +
		"\r\n" + rawdata;
	Serial.println("ESP8266: Sending data (" + String(rawdata.length()) + "/" + String(postRequest.length()) + ") to: " + uri + " ...");
	data2send = "AT+CIPSEND=1," + String(postRequest.length());
	datarcv = sendByEsp8266(data2send, timeout);
	Serial.println("ESP8266: Data Received: " + datarcv);
	// fine invio dati step 1 lunghezza ...


	// controllo invio lunghezza dati
	operationOK = received_ok(datarcv);
	if (!operationOK) {
		Serial.println("ESP8266: Not found \">\" -> closing connection.");
		data2send = "AT+CIPCLOSE=1";
		sendByEsp8266(data2send, timeout);
		return false;
	}
	Serial.println("ESP8266: Sent length of " + String(postRequest.length()) + " bytes.");
	// fine controllo invio lunghezza dati


	// invio dati step 2 dati veri e propri ...
	Serial.println("ESP8266: Post data: " + postRequest + " (" + String(postRequest.length()) + ")");
	datarcv = sendByEsp8266(postRequest, timeout);
	Serial.println("ESP8266: Data Received: " + datarcv);
	// fine invio dati step 2 dati veri e propri ...


	// controllo invio dati
	operationOK = (datarcv.length() > 0);
	if (!operationOK) {
		Serial.println("ESP8266: Send error on postrequest -> closing connection.");
		data2send = "AT+CIPCLOSE=1";
		sendByEsp8266(data2send, timeout);
		return false;
	}
	Serial.println("ESP8266: Sent ok.");
	// fine controllo invio dati


	// close the connection
	data2send = "AT+CIPCLOSE=1";
	sendByEsp8266(data2send, timeout);
	Serial.println("ESP8266: Connection closed, transmission ok.");

	return true;
}

String sendByEsp8266(String rawdata, long timeout) {
	String datarcv = "";

	// Sending to esp8266...
	esp8266.println(rawdata);

	// Handling the response...
	long time = millis();
	while ((time + timeout) > millis()) {
		while (esp8266.available() > 0) {
			datarcv = datarcv + (char)esp8266.read();
		}
	}

	return datarcv;
}

bool received_ok(String rawdata) {
	bool retval = false;
	const int len = 3;
	String ok_messages[len] = { "OK", "CONNECT", ">" };

	// int len = sizeof(ok_messages) / sizeof(String);	// lunghezza dell'array

	for (int i = 0; i < len; i++) {

		String element = ok_messages[i];
		retval = (rawdata.indexOf(element) > 0);

		if (retval)
			break;
	}

	return retval;
}