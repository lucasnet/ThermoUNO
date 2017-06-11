// Network Settings Esp8266 (WiFi + Lan)
// WIFI_SSID "Telecom-xxxxxxx" -> set by default [AT+CWJAP_DEF]
// WIFI_PSW  -> set by default [AT+CWJAP_DEF]
// IPADDR "192.168.1.100" -> set by default [AT+CIPSTA_DEF]
// NETMASK "255.255.255.0" -> set by default [AT+CIPSTA_DEF]
// GATEWAY "192.168.1.1" -> set by default [AT+CIPSTA_DEF]


#include <SoftwareSerial.h>
#include <SimpleDHT.h>

// CONSTANTS definition
#define RX_ESP 10							// Arduino RX pin (from Esp TX)
#define TX_ESP 11							// Arduino TX pin (to Esp RX)
#define RXTX_DHT11 2						// Arduino DHT11 pin
#define BRIKSDALL_URL "192.168.1.33"		// Briksdall Url
#define BRIKSDALL_PORT "81"					// Briksdall Port
#define BRIKSDALL_HUB "/ws/hub.ashx"		// Briksdall Hub Entry Point
#define BRIKSDALL_TIMEOUT 5000;				// Briksdall Connection Timeout (milliseconds)
#define POOL_INTERVAL 1;					// Briksdall send data interval (in minutes)
#define DEVICE_ID "auno_01";				// Device ID -> change it for every arduino board!!!
#define PORT  "8080"						// Web Server Port
#define DEBUG true;							// flag for debug

// GLOBAL VARIABLES definition
SoftwareSerial esp8266(RX_ESP, TX_ESP);		// Serial software for comunicating with esp8266
SimpleDHT11 dht11;							// global interface for comunicating with dht11 sensor
byte _temperature = 0;						// temperature value to send
byte _humidity = 0;							// humidity value to send
long _pool_interval = 0;					// Briksdall send data interval (in milliseconds)


void setup()
{
	Serial.begin(9600);								// Start Serial (for debug)
	esp8266.begin(115200);							// Start Esp8266 software serial (for communication)

	startClient();									// setup Arduino Client
}


void loop()
{
	long millisec = millis();

	// sending data to briksdall...
	if ((millisec % _pool_interval) == 0) {

		Serial.println("BRIKSDALL: it's time to send DHT11 data ...");

		readDHT11();		// reading values

		String names[2] = { "T", "H" };	
		int values[2] = { _temperature, _humidity };

		bool sent = send2Briksdall(names, values);
	}
}




// function: startWebServer
// Web Server configuration.
// Param -> none.
// Return value -> none.
void startClient() {

	long timeout = BRIKSDALL_TIMEOUT;

	_pool_interval = POOL_INTERVAL;					// setup briksdall pool interval...
	_pool_interval = _pool_interval * 60 * 1000;

	sendByEsp8266("ATE0", timeout);					// echo off
	sendByEsp8266("AT+CIPMUX=1", timeout);			// Multiple connections setting

	Serial.println("\nStarting Arduino client ");

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
	Serial.flush();

	return;
}

// function: send2Briksdall
// Data sending process (with retries if something goes wrong).
// Param -> type : "type" value
// Param -> value : "value" value
// Return value -> true if trasmission completes successfully, false otherwise.
bool send2Briksdall(String names[], int values[]) {
	String timestamp = getLifeTime();

	String logmessage = "BRIKSDALL: Sending Data, attempt ";
	bool transmissionOK = false;
	int i = 1;
	do {
		delay(1000);
		Serial.println(logmessage + String(i));
		transmissionOK = post2briksdall(timestamp, names, values);
		i++;
	} while ((!transmissionOK) && (i < 11));

	return transmissionOK;
}

// function: getLifeTime
// Get the life time ("mm:ss" format).
// Param -> none.
// Return value -> life time a string format.
String getLifeTime() {

	unsigned long msec = millis();
	long minutes = (msec / 1000) / 60;
	long sec = (msec / 1000) % 60;

	String str_minutes = String(minutes);
	if (minutes < 10) {
		str_minutes = "0" + str_minutes;
	}

	String str_secs = String(sec);
	if (sec < 10) {
		str_secs = "0" + str_secs;
	}
	
	String returnvalue = str_minutes + ":" + str_secs;

	return returnvalue;
}

// function: httppost
// Main function for send data to Briksdall via an http post request.
// Param -> timestamp : "timestamp" value
// Param -> type : "type" value
// Param -> value : "value" value
// Return value -> always true
bool post2briksdall(String timestamp, String names[], int values[]) {

	String server = BRIKSDALL_URL;
	String port = BRIKSDALL_PORT;
	String ws = BRIKSDALL_HUB;
	String device_id = DEVICE_ID;
	long timeout = BRIKSDALL_TIMEOUT;
	String datarcv = "";
	bool operationOK = true;
	String data2send = "";

	// Connection...
	Serial.println("BRIKSDALL: Connecting...");
	data2send = "AT+CIPSTART=1,\"TCP\",\"" + server + "\"," + port;
	datarcv = sendByEsp8266(data2send, timeout);

	// Connection Status...
	operationOK = received_ok(datarcv);
	if (!operationOK) {
		Serial.println("BRIKSDALL: TCP connection KO");
		closing_connection(timeout);
		return false;
	}
	Serial.println("BRIKSDALL: Connected to " + server + " on port " + port + ".");

	// Sending data step 1: length...
	String uri = ws + "/" + device_id;
	String rawdata = "";
	rawdata += "timestamp=" + timestamp;
	// in a microcontroller environment it's better to use fixed values than "sizeof" function
	// sizeof(names) function returns the size of pointer to "names" (it's a pointer, so 2 bytes)
	for (int i = 0; i < 2; i++) {
		rawdata += "&" + names[i] + "=" + String(values[i]);
	}
	String postRequest = "";
	postRequest += "POST " + uri + " HTTP/1.0\r\n";
	postRequest += "Host: " + server + ":" + port + "\r\n";
	postRequest += "Accept: */*\r\n";
	postRequest += "Content-Length: " + String(rawdata.length()) + "\r\n";
	postRequest += "Content-Type: application/x-www-form-urlencoded\r\n";
	postRequest += "\r\n" + rawdata;
	data2send = "AT+CIPSEND=1," + String(postRequest.length());
	datarcv = sendByEsp8266(data2send, timeout);
	operationOK = received_ok(datarcv);
	if (!operationOK) {
		Serial.println("BRIKSDALL: Not found \">\" -> closing connection.");
		closing_connection(timeout);
		return false;
	}
	Serial.println("BRIKSDALL: Sent data step 1 (data length): " + String(rawdata.length(), 0) + "bytes.");

	// Sending data step 2: raw data ...
	datarcv = sendByEsp8266(postRequest, timeout);
	// checking responses
	operationOK = (datarcv.length() > 0);
	if (!operationOK) {
		Serial.println("BRIKSDALL: Send error on postrequest -> closing connection.");
		closing_connection(timeout);
		return false;
	}
	Serial.println("BRIKSDALL: Sent data step 2 (raw data): " + rawdata);

	// close the connection
	closing_connection(timeout);
	Serial.println("BRIKSDALL: Connection closed, transmission ok.");

	return true;
}

// function: received_ok
// Find an "ok" response inside a message. The "ok" is identified by a "OK", "CONNECT", ">" strings.
// Param -> rawdata : the message (raw)
// Return value -> true if an "ok" is found inside the param "rawdata"
bool received_ok(String rawdata) {
	bool retval = false;
	const int len = 3;
	String ok_messages[len] = { "OK", "CONNECT", ">" };

	for (int i = 0; i < len; i++) {
		String element = ok_messages[i];
		retval = (rawdata.indexOf(element) > 0);
		if (retval)
			break;
	}

	return retval;
}

void closing_connection(int timeout) {
	String data2send = "AT+CIPCLOSE=1";
	sendByEsp8266(data2send, timeout);

	return;
}

// function: sendByEsp8266
// Send data on the air by Esp8266 and wait for a response...
// Param -> rawdata : data (raw) to send
// Param -> timeout : timeout for the response
// Return value -> data received as a response
String sendByEsp8266(String rawdata, long timeout) {
	String datarcv = "";
	bool debug = DEBUG;

	// Sending to esp8266..
	if (debug) {
		Serial.print("DEBUG. Send -> ");
		Serial.println(rawdata);
	}
	esp8266.println(rawdata);

	// Handling the response...
	if (debug) Serial.print("DEBUG. Receive -> ");
	long time = millis();
	while ((time + timeout) > millis()) {
		int raw_char = esp8266.read();
		char c = (char)raw_char;
		if (raw_char == 13) {
			datarcv += "\0";
			if (debug) Serial.println("");
			break;
		}
		else {
			if (debug) Serial.print(c);
			datarcv = datarcv + c;
		}

		delay(10);
	}

	esp8266.flush();
	Serial.flush();

	return datarcv;
}
