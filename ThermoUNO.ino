
#include <SoftwareSerial.h>
#include <SimpleDHT.h>

#define RX_ESP 10		// Arduino RX (from Esp TX)
#define TX_ESP 11		// Arduino TX (to Esp RX)
#define RXTX_DHT11 2	// Arduino DHT11

#define BRIKSDALL_URL "192.168.1.33"		// Briksdall Url
#define BRIKSDALL_PORT "81"					// Briksdall Port
#define BRIKSDALL_HUB "/ws/hub.ashx"		// Briksdall Hub Entry Point
#define BRIKSDALL_TIMEOUT 5000;				// Briksdall Connection Timeout (milliseconds)

#define DEVICE_ID "auno_01";				// Device ID -> change it for every arduino board!!!

#define POOL_INTERVAL 30;					// Query DHT11 interval (in minutes)


SoftwareSerial esp8266(RX_ESP, TX_ESP);		// Serial software for comunicating with esp8266
SimpleDHT11 dht11;							// global interface for comunicating with dht11 sensor


byte _temperature = 0;		// temperature value to send
byte _humidity = 0;			// humidity value to send




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

	long pool_interval = POOL_INTERVAL;
	pool_interval = pool_interval * 60 * 1000;
	long millisec = millis();

	if ((millisec % pool_interval) == 0) {
		readDHT11();

		String type = "T";
		String value = String(_temperature);
		bool sent = send2Briksdall(type, value);
	}

}





// function: readDHT11
// Read data from DHT11. Fill global variables...
// Param -> none.
// Return value -> none.
void readDHT11() {
		
	int pinDHT11 = RXTX_DHT11;

	byte temperatures[5] = { 0,0,0,0,0 };		// Temperature values
	byte humidities[5] = { 0,0,0,0,0 };			// Humidity values
	byte position = 0;
	
	// read with raw sample data.
	for (int i = 0; i < 5; i++) {
		
		byte temperature = 0;
		byte humidity = 0;
		byte data[40] = { 0 };

		if (dht11.read(pinDHT11, &temperature, &humidity, data)) {
			Serial.print("DHT11: Read failed.");
			temperatures[i] = 0;
			humidities[i] = 0;
		}

		temperatures[i] = temperature;
		humidities[i] = humidity;

		delay(1000);	// DHT11 sampling rate is 1HZ.
	}



	// average of values 
	int sumT = 0;
	String logT = "";
	int sumH = 0;
	String logH = "";

	for (int i = 0; i < 5; i++) {
		sumT += temperatures[i];
		sumH += humidities[i];

		logT += String(temperatures[i]);
		logT += " ";
		logH += String(humidities[i]);
		logH += " ";
	}

	_temperature = sumT / 5;
	_humidity = sumH / 5;

	Serial.println("DHT11: Temperature values [ " + logT + "] - Av: " + String(_temperature));
	Serial.println("DHT11: Humidity values [ " + logH + "] - Av: " + String(_humidity));

	return;
}


// function: send2Briksdall
// Data sending process (with retries if something goes wrong).
// Param -> type : "type" value
// Param -> value : "value" value
// Return value -> true if trasmission completes successfully, false otherwise.
bool send2Briksdall(String type, String value) {
	//String timestamp = String(millis());
	String timestamp = "0";	// no timestamp because Arduino doesn't know what time is it...

	String logmessage = "ESP8266: Sending Data, attempt ";
	bool transmissionOK = false;
	int i = 1;
	do {
		Serial.println(logmessage + String(i));
		transmissionOK = httppost(timestamp, type, value);
		i++;
	} while ((!transmissionOK) && (i < 11));

	if (transmissionOK) {
		Serial.println("ESP8266: Operation completed.");
	}
	else {
		Serial.println("ESP8266: Warning Data NOT sent.");
	}

	return transmissionOK;
}

// function: setupWiFi
// WiFi configuration.
// Param -> none.
// Return value -> none.
void setupWiFi() {
	// Impostazioni network Esp8266 (WiFi + Lan)
	// WIFI_SSID "Telecom-xxxxxxx" -> non serve perche impostato di default (attraverso AT+CWJAP_DEF)
	// WIFI_PSW  -> non serve perche impostato di default (attraverso AT+CWJAP_DEF)
	// IPADDR "192.168.1.100" -> non serve perche impostato di default (attraverso AT+CIPSTA_DEF)
	// NETMASK "255.255.255.0" -> non serve perche impostato di default (attraverso AT+CIPSTA_DEF)
	// GATEWAY "192.168.1.1" -> non serve perche impostato di default (attraverso AT+CIPSTA_DEF)

	long timeout = BRIKSDALL_TIMEOUT;
	sendByEsp8266("ATE0", timeout);				// echo off
	sendByEsp8266("AT+CIPMUX=1", timeout);		// configura per connessioni multiple
}

// function: httppost
// Main function for send data to Briksdall via an http post request.
// Param -> timestamp : "timestamp" value
// Param -> type : "type" value
// Param -> value : "value" value
// Return value -> always true
bool httppost(String timestamp, String type, String value) {

	String server = BRIKSDALL_URL;
	String port = BRIKSDALL_PORT;
	String ws = BRIKSDALL_HUB;
	String device_id = DEVICE_ID;
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
	String uri = ws + "/" + device_id;
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

// function: sendByEsp8266
// Send data on the air by Esp8266 and wait for a response...
// Param -> rawdata : data (raw) to send
// Param -> timeout : timeout for the response
// Return value -> data received as a response
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

// function: received_ok
// Find an "ok" response inside a message. The "ok" is identified by a "OK", "CONNECT", ">" strings.
// Param -> rawdata : the message (raw)
// Return value -> true if an "ok" is found inside the param "rawdata"
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

