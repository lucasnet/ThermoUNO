#include <SoftwareSerial.h>

// Pin configuration: RX pin 2, TX pin 3
SoftwareSerial esp8266(2, 3);

const String SSID = "Telecom-33318697";
const String PASS = "qwertyuiopASDFGHJKL1234567890";
//const String HOST = "webhook.site";
const String HOST = "192.168.1.35";
const int PORT = 8000;
//const int PORT = 80;
//const String RESOURCE = "/753bfa51-cb20-4f8e-a444-dbe1c3cb57f9";
const String RESOURCE = "/api/iothub/raw";

String IpAddr = "**";
String MacAddr = "**";



void setup() {
  Serial.begin(9600);    // Pc communication
  esp8266.begin(9600);   // ESP8266 communication (try 115200...)

  Serial.println("Check connection...");
  // Check if already connected (AT+CIPSTATUS or AT+CIFSR)
  if (!checkWiFi()) { 
    connettiWiFi();
  }
  Serial.println("ESP8266 ready.");
  
}

void loop() {
  delay(5000);
  sendJsonCloud(25.5, IpAddr, MacAddr); 
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

void sendJsonCloud(float valore, String ipaddr, String macaddr) {
  // 1. JSON payload
  //String json = "{\"device\":\"ESP01\",\"data\":\"{\\\"date\\\":\\\"17/11/2025\\\", \\\"value\\\":\\\"" + String(valore) + "\\\"}\"}";
  String json = buildJson(valore, ipaddr, macaddr);
  
  // 2. HTTP POST request
  String richiesta = "POST " + RESOURCE + " HTTP/1.1\r\n";
  richiesta += "Host: " + HOST + ":" + String(PORT) + "\r\n";
  richiesta += "Content-Type: application/json\r\n";
  richiesta += "Content-Length: " + String(json.length()) + "\r\n";
  richiesta += "Connection: close\r\n\r\n";
  richiesta += json;

  // 3. Apriamo la connessione TCP
  esp8266.println("AT+CIPSTART=\"TCP\",\"" + HOST + "\"," + String(PORT));
  if(waitForResponse("OK", 6000)) {
    Serial.println("Connected.");
    
    // 4. Inviamo il comando CIPSEND con la lunghezza totale della richiesta
    esp8266.println("AT+CIPSEND=" + String(richiesta.length()));
    
    if(waitForResponse(">", 2000)) {
      // 5. Inviamo finalmente la richiesta HTTP
      esp8266.print(richiesta);
      Serial.println("JSON data sent!");
    }
  } else {
    Serial.println("TCP connection error.");
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

// build json
String buildJson(float valore, String ipaddr, String macaddr) {
  String json = "{\"device\":\"" + macaddr + "\",\"data\":\"{\\\"mac\\\":\\\"" + macaddr + "\\\", \\\"ipaddr\\\":\\\"" + ipaddr + "\\\", \\\"date\\\":\\\"17/11/2025\\\", \\\"value\\\":\\\"" + String(valore) + "\\\"}\"}";

  return json;
}