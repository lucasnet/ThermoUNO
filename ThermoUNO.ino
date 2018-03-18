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
#define DEVICE_HOST "auno_01"				// Device HOST -> change it for every arduino board!!!
#define DEVICE_ID "1"						// Device ID -> change it for every arduino board!!!
#define BRIKSDALL_TIMEOUT 10000				// Briksdall Connection Timeout (milliseconds)
#define POOL_INTERVAL 1						// Briksdall send data interval (in minutes)
#define PHYSICAL_ENTITIES 2					// Number of Physical entities (to misure...)
#define DEBUG true							// flag for debugs

// GLOBAL VARIABLES definition
SoftwareSerial esp8266(RX_ESP, TX_ESP);		// Serial software for comunicating with esp8266
SimpleDHT11 dht11;							// global interface for comunicating with dht11 sensor
byte _temperature = 0;						// temperature value to send
byte _humidity = 0;							// humidity value to send
unsigned long _pool_interval = 0;			// Briksdall send data interval (in milliseconds)
unsigned long _time_counter = 0;


// main functions
void setup()
{
	Serial.begin(9600);								// Start Serial (for debug)
	esp8266.begin(112000);							// Start Esp8266 software serial (for communication)

	startClient();									// setup Arduino Client
}

void loop()
{
	//unsigned long millisec = millis();
	_time_counter++;
	Serial.println(_time_counter);

	// sending data to briksdall...
	//if ((millisec % _pool_interval) < 2000) {
	if (_time_counter > _pool_interval) {
		_time_counter = 0;
		log_message("ARDUINO: it's time to send DHT11 data ...", true);

		// reading values from dht11
		readDHT11();		
		// formatting values for briksdall
		char entities_names[2] = "TH";	
		int entities_values[2] = { _temperature, _humidity };
		// sending to briksdall...
		bool sent = send2Briksdall(entities_names, entities_values);
	}
}






// PRIVATE SECTION ....


// function: startClient
// Start connection with esp8266...
/// Param -> none.
/// Return value -> none.
void startClient() {

	_pool_interval = POOL_INTERVAL;			// setup briksdall pool interval...
	_pool_interval = _pool_interval * 100 * 60;

	sendByEsp8266("ATE0", BRIKSDALL_TIMEOUT);			// echo off
	sendByEsp8266("AT+CIPMUX=0", BRIKSDALL_TIMEOUT);	// Multiple connections setting (0=NO)

	log_message("ARDUINO: start", true);

}

// function: readDHT11
// Read data from DHT11. Fill global variables...
/// Param -> none.
/// Return value -> none.
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
			log_message("DHT11: Read failed.", true);
			temperatures[i] = 0;
			humidities[i] = 0;
		}

		temperatures[i] = temperature;
		humidities[i] = humidity;

		delay(1000);	// DHT11 sampling rate is 1Hz.
	}



	// average of values 
	int sumT = 0;
	int sumH = 0;

	for (int i = 0; i < 5; i++) {
		sumT += temperatures[i];
		sumH += humidities[i];
	}

	_temperature = sumT / 5;
	_humidity = sumH / 5;


	// logging...
	char str_log[] = "";
	sprintf(str_log, "DHT11: Temperature value %u.", _temperature);
	log_message(str_log, true);
	sprintf(str_log, "DHT11: Humidity value %u.", _humidity);
	log_message(str_log, true);

	return;
}

// function: send2Briksdall
// sends data to Briksdall (with retries if something goes wrong).
/// Param -> names : entities name, T for temperature, H for humidity 
/// Param -> values : values of temperature and humidity
/// Return value -> true if trasmission completes successfully, false otherwise.
bool send2Briksdall(char names[2], int values[]) {
	char timestamp[10];
	sprintf(timestamp, "%u", millis());

	char logmessage[40] = "";
	bool transmissionOK = false;
	int i = 1;
	do {
		delay(3000);
		sprintf(logmessage, "ARDUINO: Sending Data, attempt %u ...", i);
		log_message(logmessage, true);
		transmissionOK = post2briksdall(timestamp, names, values);
		i++;
	} while ((!transmissionOK) && (i < 11));

	return transmissionOK;
}


// function: post2briksdall
// Main function for send data to Briksdall via an http post request.
/// Param -> timestamp : "timestamp" value
/// Param -> names : entities names
/// Param -> value : values
/// Return value -> always true
bool post2briksdall(char timestamp[], char names[2], int values[]) {

	byte operationOK;
	char data2send[255] = "";
	char *datarcv;
	char logmessage[100] = "";

	// Connection...
	log_message("ESP8266: Connecting...", true);
	sprintf(data2send, "AT+CIPSTART=\"TCP\",\"%s\",%s", BRIKSDALL_URL, BRIKSDALL_PORT);
	char *data = sendByEsp8266(data2send, BRIKSDALL_TIMEOUT);
	byte opResult = received_ok(data, 1);
	if (opResult == 0) {
		log_message("ESP8266: TCP connection KO", true);
		closing_connection(BRIKSDALL_TIMEOUT);
		return false;
	}
	log_message("ESP8266: Connected.", true);
	// Connection completed...

	// building postrequest 
	char *postdata = create_postData(timestamp, names, values);		// postdata is data on post request ...
	char str_row[65] = "";											// single row in the post request
	long request_length = 0;										// post request length
	char postRequest[180];											// post request
	// row 1
	sprintf(str_row, "POST %s/%s HTTP/1.1", BRIKSDALL_HUB, DEVICE_ID);
	request_length = request_length + strlen(str_row) + 2;
	sprintf(postRequest, "%s\r\n", str_row);
	// row 2
	sprintf(str_row, "DeviceName: %s", DEVICE_HOST);
	request_length = request_length + strlen(str_row) + 2;
	sprintf(postRequest, "%s%s\r\n", postRequest, str_row);
	// row 3
	sprintf(str_row, "Content-Type: application/x-www-form-urlencoded;charset=UTF-8");
	request_length = request_length + strlen(str_row) + 2;
	sprintf(postRequest, "%s%s\r\n", postRequest, str_row);
	// row 4
	sprintf(str_row, "Content-Length: %u", strlen(postdata));
	request_length = request_length + strlen(str_row) + 2;
	sprintf(postRequest, "%s%s\r\n", postRequest, str_row);
	// row 5 (only \r\n)
	request_length = request_length + 2;
	sprintf(postRequest, "%s\r\n", postRequest);
	// row 6 (postdata)
	request_length = request_length + strlen(postdata) + 2;
	sprintf(postRequest, "%s%s\r\n", postRequest, postdata);
	// row 7 (only \r\n)
	request_length = request_length + 2;
	sprintf(postRequest, "%s\r\n", postRequest);
	// postrequest completed...


	// Sending data...
	sprintf(logmessage, "ESP8266: Sending data (%u bytes)...", request_length);
	log_message(logmessage, true);
	sprintf(data2send, "AT+CIPSEND=%u", request_length);
	data = sendByEsp8266(data2send, BRIKSDALL_TIMEOUT);
	data = sendByEsp8266(postRequest, BRIKSDALL_TIMEOUT, 4800);
	opResult = (strlen(data) > 0);
	if (opResult == 0) {
		log_message("ESP8266: Send error on postrequest -> closing connection.", true);
		closing_connection(BRIKSDALL_TIMEOUT);
		return false;
	}
	// Send data completed...

	// close the connection
	closing_connection(BRIKSDALL_TIMEOUT);
	log_message("ESP8266: transmission ok.", true);

	return true;
}


// function: create_postData.
// create the post data for post request payload. Post data is an array of (name, value) couples.
// example: timestamp=<timestamp>&T=<xx>&H=<yy>
/// Params: 
/// timestamp: string of time stamp.
/// names: arrays of names
/// values: arrays of values.
char *create_postData(char timestamp[], char names[], int values[]){
	char rawdata[32] = "";
	sprintf(rawdata, "timestamp=%s", timestamp);

	// in a microcontroller environment it's better to use fixed values than "sizeof" function
	// sizeof(names) function returns the size of pointer to "names" (it's a pointer, so 2 bytes)
	for (int i = 0; i < PHYSICAL_ENTITIES; i++) {
		char str_value[16];
		sprintf(str_value, "&%c=%02d", names[i], values[i]);
		strcat(rawdata, str_value);
	}

	return rawdata;
}

// function: received_ok
// Find an "ok" response inside a message. The "ok" is identified by a "OK", "CONNECT", ">" strings.
/// Params
/// rawdata: the message (raw)
/// type: index of "what I'm searching for...."
/// Return value -> 1 if an "ok" is found inside the param "rawdata"
byte received_ok(char rawdata[], int type) {
	byte retval = 0;
	byte exit_loop = 0;

	byte counter = 0;
	while ((counter < 255) && (exit_loop == 0)){
		
		if (rawdata[counter] == 0){
			exit_loop = 1;
		}

		switch (type){
			case 1:	// connect
				if (rawdata[counter] == 67){					// C
					if (( rawdata[counter + 1] == 79)			// O
						&& ( rawdata[counter + 2] == 78)		// N
						&& ( rawdata[counter + 3] == 78)		// N
						&& ( rawdata[counter + 4] == 69)){		// E
							retval = 1;
						}
				}
				break;

			case 2:	// ok
				if (rawdata[counter] == 79){					// O
					if ( rawdata[counter + 1] == 75)			// K
						retval = 1;
				}
				break;

			case 3:	// \nOK\n>
				if (rawdata[counter] == 122){					// z
					if (( rawdata[counter + 1] == 79)			// O
						&& ( rawdata[counter + 2] == 75)		// K
						&& ( rawdata[counter + 3] == 122)		// z
						&& ( rawdata[counter + 4] == 62)){		// >
							retval = 1;
						}
				}
				break;

		}

		if (retval == 1){
			exit_loop = 1;
		}

		counter++;
	}

	return retval;
}


// function: closing_connection
// send an AT close command to esp8266 to closing connection with the server.
/// params
/// timeout: connection timeout
/// Return value -> none
void closing_connection(int timeout) {
	char data2send[] = "AT+CIPCLOSE";
	sendByEsp8266(data2send, timeout);

	return;
}

// function: sendByEsp8266
// Send data on the air by Esp8266 and wait for a response...
/// params
/// rawdata : data (raw) to send
/// timeout : timeout for the response
/// Return value -> data received as a response
char *sendByEsp8266(char rawdata[], long timeout) {
	return sendByEsp8266(rawdata, timeout, 0);
}

// function: sendByEsp8266
// Send data on the air by Esp8266 and wait for a response...
/// params
/// rawdata : data (raw) to send
/// timeout : timeout for the response
/// delaytime : delay time for the reponse.
/// Return value -> data received as a response
char *sendByEsp8266(char rawdata[], long timeout, long delaytime) {
	char datarcv[255] = "";

	// Sending to esp8266..
	log_message("DEBUG. Send -> ", false);
	log_message(rawdata, true);
	esp8266.println(rawdata);

	if (delaytime == 0){
		delay(500);
	}else{
		delay(delaytime);
	}

	// Handling the response...
	log_message("DEBUG. Receive -> ", false);
	long time = millis();
	bool end_of_line = false;
	int count=0;
	while (((time + timeout) > millis()) && !end_of_line) 
	{ 
		char c = esp8266.read();

		switch ((int) c)
		{
			case -1:
				end_of_line = true;
				log_message("", true);
				break;

			case 13:
				datarcv[count] = " ";
				count++;log_message("", true);
				time = millis();		// resetting time...
				break;

			case 32 ... 127:
				log_char(c, false);
				datarcv[count] = c;
				count++;
				time = millis();		// resetting time...
				break;
		}
		
		delay(10);
	}

	if (!end_of_line)
	{
		log_message("... Timeout happens!!", true);
	}

	esp8266.flush();
	Serial.flush();
	
	return datarcv;
}



// function: log_char
// Logs a character on a Serial port with (optional) carriage return/line feed.
// Logs only the DEBUG constant is set to true.
/// params:
/// c: character to log;
/// crlf: flag for carriage return/line feed.
void log_char(char c, bool crlf){
	
	bool debug = DEBUG;

	if (debug){
		if (crlf){
			Serial.println(c);
		}else{
			Serial.print(c);
		}
	}
}

// function: log_message
// Logs message on a Serial port if DEBUG constant is set to true.
/// params:
/// message : message to log;
/// crlf: flag for carriage return/line feed.
void log_message(char message[], bool crlf){

	bool debug = DEBUG;

	if (debug){
		if (crlf){
			Serial.println(message);
		}else{
			Serial.print(message);
		}
	}
	
}