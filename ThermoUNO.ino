#include <SoftwareSerial.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 4          // Digital pin connected to the DHT sensor 
#define DHTTYPE DHT11     // DHT 11

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

// Pin configuration: RX pin 2, TX pin 3
SoftwareSerial esp8266(2, 3);

const String SSID = "Telecom-33318697";
const String PASS = "qwertyuiopASDFGHJKL1234567890";
const String HOST = "webhook.site";
// const String HOST = "192.168.1.35";
// const int PORT = 8000;
const int PORT = 80;
const String RESOURCE = "/9d79f7fd-4d28-42e4-b836-c2c285a4ec96";
// const String RESOURCE = "/api/iothub/raw";

String IpAddr = "**";
String MacAddr = "**";
String SensorType = "**";
float temperature = 0;
float humidity = 0;



void setup() {
  Serial.begin(9600);    // Pc communication
  esp8266.begin(9600);   // ESP8266 communication (try 115200...)

  Serial.println("Check connection...");
  // Check if already connected (AT+CIPSTATUS or AT+CIFSR)
  if (!checkWiFi()) { 
    connettiWiFi();
  }
  Serial.println("ESP8266 ready.");
  
  // setup dht
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  // Serial.println(F("Temperature Sensor"));
  // Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  // Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  // Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  // Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  // Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  // Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  // Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  // Serial.println(F("Humidity Sensor"));
  // Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  // Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  // Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  // Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  // Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  // Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  // Serial.println(F("------------------------------------"));
  // Set delay between sensor readings based on sensor details.
  delayMS = sensor.min_delay / 1000;

}



void loop() {

  readFromDHT11();
  
  
  delay(5000);

  sendData(temperature, humidity, IpAddr, MacAddr); 
  delay(25000);
}

// Funzione di supporto per inviare comandi e leggere la risposta nel Monitor Seriale
void SendCommand(String command, const int timeout) {
  esp8266.println(command); 
  long int time = millis();
  
  while ((time + timeout) > millis()) {
    while (esp8266.available()) {
      char c = esp8266.read(); 
      Serial.print(c);
    }
  }
}

void connettiWiFi() {  
  Serial.println("Begin Wi-Fi configuration...");

  // 1. Response test
  SendCommand("AT", 2000);
  
  // 2. Set'ESP in "Station" (client) mode
  SendCommand("AT+CWMODE=1", 2000);
  
  // 3. Connection
  String cmdConnection = "AT+CWJAP=\"" + SSID + "\",\"" + PASS + "\"";
  Serial.println("Connect to: " + SSID);
  SendCommand(cmdConnection, 7000); // Connection can require some seconds...

  delay(5000);

  // 4. Check IP Address
  SendCommand("AT+CIFSR", 2000);
}

bool checkWiFi() {
  esp8266.println("AT+CIFSR"); // Ask IP address
  
  long int time = millis();
  String response = "";
  
  // Read response for 2 seconds
  while ((time + 2000) > millis()) {
    while (esp8266.available()) {
      char c = esp8266.read();
      response += c;
    }
  }

  // If response contains "192." (or something like an IP address) we are connected.
  // If it contains "0.0.0.0" we aren't connected
  IpAddr = getIpAddr(response);
  Serial.println("The IP address is " + IpAddr);
  MacAddr = getMacAddr(response);
  Serial.println("The Mac address is " + MacAddr);
  if (response.indexOf("0.0.0.0") == -1 && response.indexOf("192.") != -1) {
    Serial.println("State: Connected! IP address: " + response);
    return true;
  } else {
    Serial.println("State: Disconnected.");
    return false;
  }
}



void sendData(float temp, float hum, String ipaddr, String macaddr) {
  // 1. Open TCP connection
  Serial.println(F("Open TCP Connection..."));
  esp8266.println("AT+CIPSTART=\"TCP\",\"" + HOST + "\"," + String(PORT));
  
  if (!waitForResponse("OK", 3000)) {
    Serial.println(F("Error: Can't connect to host."));
    return; 
  }

  // 2. Calcolo della lunghezza esatta del JSON (Streaming)
  // Il tuo JSON: {"device":"MAC","data":"{\"ipaddr\":\"IP\", \"temp\":\"TEMP\", \"hum\":\"0\"}"}
  // Contiamo i caratteri fissi che vanno "sul filo" dopo che C++ ha elaborato gli escape
  String tempStr = String(temp);
  String humStr = String(hum);
  int jsonLength = 11; // {"device":"
  jsonLength += macaddr.length();
  jsonLength += 24;    // ","data":"{\"ipaddr\":\" 
  jsonLength += ipaddr.length();
  jsonLength += 15;    // \", \"temp\":\"  (incluso spazio dopo virgola)
  jsonLength += tempStr.length();
  jsonLength += 14;    // \", \"hum\":\"
  jsonLength += humStr.length();
  jsonLength += 5; // \"}"}
  
  // 3. Prepariamo gli header HTTP
  String header = "POST " + RESOURCE + " HTTP/1.1\r\n";
  header += "Host: " + HOST + ":" + String(PORT) + "\r\n";
  header += "Content-Type: application/json\r\n";
  header += "Content-Length: " + String(jsonLength) + "\r\n";
  header += "Connection: close\r\n\r\n";

  // 4. Inviamo il comando CIPSEND con la somma totale
  int totaleInvio = header.length() + jsonLength;
  esp8266.print(F("AT+CIPSEND="));
  esp8266.println(totaleInvio);

  if (waitForResponse(">", 2000)) {
    // 5. Inviamo l'Header
    esp8266.print(header);
    
    // 6. Inviamo il JSON pezzo per pezzo (Streaming)
    // Usiamo F() per non occupare RAM
    esp8266.print(F("{\"device\":\""));
    esp8266.print(macaddr);
    esp8266.print(F("\",\"data\":\"{\\\"ipaddr\\\":\\\""));
    esp8266.print(ipaddr);
    esp8266.print(F("\\\", \\\"temp\\\":\\\""));
    esp8266.print(tempStr);
    esp8266.print(F("\\\", \\\"hum\\\":\\\""));
    esp8266.print(humStr);
    esp8266.print(F("\\\"}\"}"));

    // 7. Aspettiamo la conferma dall'ESP
    if (waitForResponse("SEND OK", 3000)) {
      Serial.println(F("Successo: JSON inviato e ricevuto dall'ESP!"));
    } else {
      Serial.println(F("Errore: L'invio dei dati è fallito o è andato in timeout."));
    }
  } else {
    Serial.println(F("Errore: L'ESP non è pronto per ricevere i dati (manca >)."));
  }
}

// Wait for ESP8266 response
bool waitForResponse(String target, int timeout) {
  long int startTime = millis();
  String buffer = "";
  while (millis() - startTime < timeout) {
    while (esp8266.available()) {
      char c = esp8266.read();
      buffer += c;
      if (buffer.indexOf(target) != -1) return true;
    }
  }
  return false;
}

// get ip address
String getIpAddr(String s) {
  return getAddr(s, "STAIP");
}

// get mac address
String getMacAddr(String s) {
  return getAddr(s, "STAMAC");
}

// get generic address
String getAddr(String s, String prefix) {
  String addr = "**";

  prefix += ",\""; // add ,"
  int start_index = s.indexOf(prefix);

  if (start_index != -1) {

    start_index += prefix.length(); 

    int end_index = s.indexOf("\"", start_index + 1);

    if (end_index != -1){
      // get addr
      addr = s.substring(start_index, end_index);
    }
  }

  return addr;

}


void readFromDHT11() {
  Serial.println(F("Reading from DHT11..."));
  // Delay between measurements.
  delay(delayMS);
  // Get temperature event and print its value.
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    temperature = event.temperature;
    Serial.print(F("Temperature: "));
    Serial.print(temperature);
    Serial.println(F("°C"));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    humidity = event.relative_humidity;
    Serial.print(F("Humidity: "));
    Serial.print(humidity);
    Serial.println(F("%"));
  }
}