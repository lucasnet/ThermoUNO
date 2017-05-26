// Network Settings Esp8266 (WiFi + Lan)
// WIFI_SSID "Telecom-xxxxxxx" -> set by default [AT+CWJAP_DEF]
// WIFI_PSW  -> set by default [AT+CWJAP_DEF]
// IPADDR "192.168.1.100" -> set by default [AT+CIPSTA_DEF]
// NETMASK "255.255.255.0" -> set by default [AT+CIPSTA_DEF]
// GATEWAY "192.168.1.1" -> set by default [AT+CIPSTA_DEF]


// LIBRARIES definition

#include <SoftwareSerial.h>
#include <SimpleDHT.h>



// CONSTANTS definition

// Arduino PIN settings
#define RX_ESP 10							// Arduino RX pin (from Esp TX)
#define TX_ESP 11							// Arduino TX pin (to Esp RX)
#define RXTX_DHT11 2						// Arduino DHT11 pin

// Briksdall settings
#define BRIKSDALL_URL "192.168.1.33"		// Briksdall Url
#define BRIKSDALL_PORT "81"					// Briksdall Port
#define BRIKSDALL_HUB "/ws/hub.ashx"		// Briksdall Hub Entry Point
#define BRIKSDALL_TIMEOUT 5000;				// Briksdall Connection Timeout (milliseconds)

// Device settings
#define DEVICE_ID "auno_01";				// Device ID -> change it for every arduino board!!!

// Web Server settings
#define PORT  "8080"						// using port 8080 by default

#define BUFFER_SIZE 128

#define POOL_INTERVAL 30;					// Query DHT11 interval (in minutes)

#define DEBUG true;							// flag for debug


// GLOBAL VARIABLES definition

SoftwareSerial esp8266(RX_ESP, TX_ESP);		// Serial software for comunicating with esp8266
SimpleDHT11 dht11;							// global interface for comunicating with dht11 sensor
byte _temperature = 0;						// temperature value to send
byte _humidity = 0;							// humidity value to send





void setup()
{

	Serial.begin(9600);			// Start Serial (for debug)
	esp8266.begin(115200);		// Start Esp8266 software serial (for communication)

	startWebServer();			// setup WiFi

}



void loop()
{

	///* debug/echo seriale-esp */
	//while (esp8266.available()) {
	//	Serial.print((char) esp8266.read());
	//}

	//while (Serial.available()) {
	//	esp8266.write(Serial.read());
	//}

	//delay(2000);
	///* fine debug serial-esp */



	//long pool_interval = POOL_INTERVAL;
	//pool_interval = pool_interval * 60 * 1000;
	//long millisec = millis();

	//// sending data to briksdall...
	//if ((millisec % pool_interval) == 0) {

	//	Serial.println("DEBUG: Briksdall time -> send DHT11 data to briksdall ...");

	//	readDHT11();

	//	String type = "T";
	//	String value = String(_temperature);
	//	bool sent = send2Briksdall(type, value);
	//}


	// web server
	if (esp8266.available() > 0) {
		accept_client();
	}
}


// function: startWebServer
// Web Server configuration.
// Param -> none.
// Return value -> none.
void startWebServer() {

	String port = PORT;
	long timeout = BRIKSDALL_TIMEOUT;

	sendByEsp8266("ATE0", timeout);						// echo off
	sendByEsp8266("AT+CIPMUX=1", timeout);				// Multiple connections setting												
	sendByEsp8266("AT+CIPSERVER=1," + port, timeout);	// start web server and listen on port PORT
	
	Serial.println("\nStarting Arduino web server on port " + port);
}


void accept_client() {
	bool debug = DEBUG;

	// new data available...
	Serial.println("WEB SERVER. new request incoming ...");

	String rawData = "";	// raw data from esp8266

	// get data
	while (esp8266.available() > 0) {

		int iRead = esp8266.read();
		char cRead = (char)iRead;

		if (iRead == 13) {
			//parse string....
			rawData += "\0";
			if (debug) {
				Serial.print("DEBUG. Received -> ");
				Serial.println(rawData);
			}
		}
		else {
			rawData += cRead;
		}

		delay(10);
	}

	esp8266.flush();
	Serial.flush();

	// Parse data...
	int index = rawData.indexOf("+IPD,");		// +IPD,3,329:GET /favicon.ico HTTP/1.1

	if (index < 1) {
		Serial.println("WEB SERVER: Error reading data (IPD NOT Found). Discard...");
		Serial.flush();
		return;
	}

	index += 5;
	String str_id = rawData.substring(index, index + 1);		// request client id

	index = index + 6;
	int get_index = rawData.indexOf("GET ", index);
	
	if (get_index < 1) {
		Serial.println("WEB SERVER: Error reading data (GET not found). Discard...");
		Serial.flush();
		return;
	}
	
	get_index += 4;
	index = rawData.indexOf(" ", get_index);
	String resource = rawData.substring(get_index, index);			// resource requested

	Serial.println("WEB SERVER: GET Found. Requested: " + resource);

	if (resource.equals("/")) {
		// requested "/", so it's a classic browser request
		resolve_homepage(str_id);
	}
	else {
		// requested a particular resource
		resolve_resource(str_id, resource.substring(1));
	}
	
	return;
}

// function: resolve_homepage
// Response in html format.
// Param -> client id.
// Return value -> none.
void resolve_homepage(String ch_id) {

	Serial.println("WEB SERVER. Building html response.");

	// reading from dht11
	readDHT11();

	// variables
	String header = "";
	String content = "";
	String deviceid = DEVICE_ID;
	String lifetime = getLifeTime();

	// make html response
	content += "<html>";
	content += "<head>";
	content += "<title>" + deviceid + "</title>";
	content += "</head>";
	content += "<body>";
	content += "<h3>Arduino: " + deviceid + "</h3>";
	content += "<h4>Temperature: " + String(_temperature) + "</h4>";
	content += "<h4>Humidity: " + String(_humidity) + "</h4>";
	content += "<h4>Life time: " + lifetime + "</h4>";
	content += "</body>";
	content += "</html>";
	content += "\r\n\r\n";

	header += "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nRefresh: 20\r\n";
	header += "Content-Length:";
	header += (int)(content.length());
	header += "\r\n\r\n";

	// sending data on esp8266
	String datarcv = "";
	long timeout = BRIKSDALL_TIMEOUT;
	String data2send = "AT+CIPSEND=" + ch_id + "," + String(header.length() + content.length());

	datarcv = sendByEsp8266(data2send, timeout);
	datarcv = sendByEsp8266(header, timeout);
	datarcv = sendByEsp8266(content, timeout);

	bool operationOK = (datarcv.length() > 0);
	
	// close the connection
	data2send = "AT+CIPCLOSE=" + ch_id;
	sendByEsp8266(data2send, timeout);

	return;
}
// function: resolve_resource
// Response in json format.
// Param -> client id.
// Param -> requested resource.
// Return value -> none.
void resolve_resource(String ch_id, String resource) {

	Serial.println("WEB SERVER. Building json response (resource: " + resource + ").");
	
	// reading from dht11
	readDHT11();

	// make html response
	String header = "";
	String content = "";
	String resource_value = getResourceValue(resource);

	content += "{";
	content += resource_value;
	content += "}";
	content += "\r\n\r\n";

	header += "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nRefresh: 20\r\n";
	header += "Content-Length:";
	header += (int)(content.length());
	header += "\r\n\r\n";

	// sending data on esp8266
	String datarcv = "";
	long timeout = BRIKSDALL_TIMEOUT;
	String data2send = "AT+CIPSEND=" + ch_id + "," + String(header.length() + content.length());

	datarcv = sendByEsp8266(data2send, timeout);
	datarcv = sendByEsp8266(header, timeout);
	datarcv = sendByEsp8266(content, timeout);

	bool operationOK = (datarcv.length() > 0);

	// close the connection
	data2send = "AT+CIPCLOSE=" + ch_id;
	sendByEsp8266(data2send, timeout);

	return;

}
// function: getResourceValue
// Get the value of a resource.
// Param -> resource name.
// Return value -> resource value.
String getResourceValue(String resource_name) {

	String content = "";

	if (resource_name == "T") {
		content = "\"TEMPERATURE\": \"" + String(_temperature) + "\"";
	}
	else if (resource_name == "H") {
		content = "\"HUMIDITY\": \"" + String(_humidity) + "\"";
	}
	else if (resource_name == "LT") {
		String lifetime = getLifeTime();
		content = "\"LIFETIME\": \"" + lifetime + "\"";
	}
	else if (resource_name == "ALL") {
		String lifetime = getLifeTime();
		content += "\"TEMPERATURE\": \"" + String(_temperature) + "\",";
		content += "\"HUMIDITY\": \"" + String(_humidity) + "\",";
		content += "\"LIFETIME\": \"" + lifetime + "\"";
	}

	return content;
}
// function: getLifeTime
// Get the life time ("mm:ss" format).
// Param -> none.
// Return value -> life time a string format.
String getLifeTime() {

	unsigned long msec = millis();
	long minutes = (msec / 1000) / 60;
	long sec = (msec / 1000) % 60;

	String returnvalue = String(minutes) + ":" + String(sec);

	return returnvalue;
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

