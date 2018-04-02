#include <ir_Daikin.h>
#include <ir_Kelvinator.h>
#include <IRremoteESP8266.h>
//#include <IRremoteInt.h>
#include <IRsend.h>

#include <ESP8266WiFi.h>

#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
 
#include "wifi_config.h"
#include "samsung_codes.h"
 
#define LED_PIN D0
#define LED_ON LOW

#define IR_PIN D1

#define SERVER_PORT 80

/*
 * Sketch to run a basic web server than lets you select IR commands to transmit to control a Samsung TV.
 * The set of supported commands are enumerated in samsung_codes.h
 * Commands should be supplied as the query parameter command as either a GET or POST
 *   http://<nodeMCU IP>:80/tv?command=<command>
 * eg
 *   http://192.168.1.27:80/tv?command=power
 *   
 * You can also toggle the onboard LED (which is on when D0 is low and vice versa) with
 *   http://<nodeMCU IP>:80/led?command=<command>
 * where command is one of ON or OFF
 * 
 * Basic wiring:
 * IR_PIN (D1) on NodeMCU to 100k resistor to IR LED anode.
 * IR LED cathode to ground on NodeMCU.
 * 
 * Boosted Power Wiring:
 * IR_PIN (D1) on NodeMCU to 1.8k resistor (approximated with 3x 560 + 100 = 1.78k) to base of P2N2222A transitor's base
 * Transistor collector connected to 3mm IR LED cathode
 * IR LED annode connected through 100k resistor to 3.3v of NodeMCU
 * Transistor emitter connected to NodeMCU ground
 * Note: P2N222A pins are, from left to right, collector, base, emitter, when looking at flat side (http://ardx.org/datasheet/IC-2222A.pdf)
 * Resistor calculations based on http://www.petervis.com/Raspberry_PI/Driving_LEDs_with_CMOS_and_TTL_Outputs/Driving_an_LED_Using_Transistors.html
 * to give 1.5v around 20mA being driven through IR LED and 2mA being sourced from D1.
 * 
 * Board: NodeMCU 1.0 (ESP-12E Module)
 * CPU Frequency: 80MHz
 * Flash Size: 4M (3M SPIFFS)
 * Upload speed: 115200
 * 
*/

// special value to represent that no command was supplied
const String NO_COMMAND = "NO_COMMAND" ;

WiFiUDP UDP;
boolean udpConnected = false;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming UDP packet
IPAddress ipMulti(239, 255, 255, 250);
unsigned int portMulti = 1900;      // local port to listen on
String serialNumber;
String persistent_uuid;
String device_name = "box";

boolean wifiConnected = false;

// the current state of the normal LED (on or off)
int ledStatus;

ESP8266WebServer server(SERVER_PORT);
IRsend irSend(IR_PIN);

void updateLed(int newState){
  ledStatus = newState;
  digitalWrite(LED_PIN, newState);
}

void handleUpdate(String argumentName, String argumentValue){
  if ( argumentName.equals("LED") )  {
    
    // Set LED to the requested state
    if ( argumentValue.equals("ON") ) {
      updateLed(LED_ON);
    }
    else if ( argumentValue.equals("OFF") )  {
      updateLed(!LED_ON);
    }
  }
  else {
    String message = " got argument " + argumentName + ": " + argumentValue;
    Serial.println(message);
    //TODO check command match
    if(argumentName.equals("command")){
      unsigned long code = findCodeByName(argumentValue.c_str());
      if(code != 0L){
        Serial.println("Found matching command");
        Serial.println(code, HEX);
        irSend.sendSAMSUNG(code, SAMSUNG_BITS);
      }
      else{
        Serial.println("No match found");
      }
    }
  }
}

String getCommand(){
  // check if it's a valid update (just use first query parameter)
  if(server.args() > 0 && server.argName(0).equals("command")){
    String command = server.arg(0);
    Serial.print("Got command "); Serial.println(command);
    return command;
  }
  return NO_COMMAND;
}

void handleLedPath() {
  String command = getCommand();
  // make sure we actually got a command
  if( !NO_COMMAND.equals(command) ){
    // Set LED to the requested state
    if ( command.equals("ON") ) {
      updateLed(LED_ON);
      server.send (200, "text/plain", "Success");
    }
    else if ( command.equals("OFF") )  {
      updateLed(!LED_ON);
      server.send (200, "text/plain", "Success");
    }
    else{
      server.send (400, "text/plain", "Invalid command");
    }
  }
}

boolean handleTvCommand(String &command){
   // try to lookup the command
    unsigned long code = findCodeByName(command.c_str());
    if (code != 0L) {
      // Transmit the IR code for the command
      Serial.println("Found matching command");
      Serial.println(code, HEX);
      irSend.sendSAMSUNG(code, SAMSUNG_BITS);
      return true;
    }
    else {
      Serial.println("No match found");
    }

    return false;
}

void handleTvPath() {
  String command = getCommand();
  // make sure we actually got a command
  if ( !NO_COMMAND.equals(command) ) {
    boolean result = handleTvCommand(command);
    if (result) {
      server.send (200, "text/plain", "Success");
      return;
    }
  }

  // if we got here something didn't work out
  server.send (400, "text/plain", "Invalid command");
}

void handleControlEvent(){
  Serial.println("########## Responding to  /upnp/control/basicevent1 ... ##########");      

  String request = server.arg(0);      
  Serial.print("request:");
  Serial.println(request);
  String powerCmd = "power";

  if(request.indexOf("<BinaryState>1</BinaryState>") > 0) {
    Serial.println("Got Turn on request");
    // best we can do is toggle
    handleTvCommand(powerCmd);
  }

  if(request.indexOf("<BinaryState>0</BinaryState>") > 0) {
    Serial.println("Got Turn off request");
    // best we can do is toggle
    handleTvCommand(powerCmd);
  }
  
  server.send(200, "text/plain", "");
}

// Serve out the web page version
void handleRoot() {

  // good thing we've got some RAM to burn /s
  String pageContent = "<!DOCTYPE HTML>"
  "<html>";
  
  // prevent favicon request
  pageContent += "<head>"
  "<link rel='icon' href='data:,'>"
  "<script>\n"
  "function sendCommand(path, command, callback){ \n"
  "  var xhr = new XMLHttpRequest();\n"
  "  xhr.open('POST', path + '?command='+ command);\n"
  "  xhr.onreadystatechange = function () { \n"
  "    if(xhr.readyState === XMLHttpRequest.DONE){ \n"
  "      document.getElementById('commandResult').value = xhr.responseText;\n"
  "      if(callback){ callback(); }\n"
  "    }\n"
  "  };\n"
  "  xhr.send();\n"
  "  console.log('sending command');\n"
  "}\n"
  "function updateLed(command){\n"
  "  sendCommand('/led', command, function(){ document.getElementById('ledStatus').innerHTML = command; });\n"
  "}\n"
  "</script>\n"
  "<style>\n"
  "body, button { font-size: 18pt }\n"
  "table { text-align: center }\n"
  "button { height: 41px }\n"
  "table button { padding-bottom: 10px; }\n"
  "</style>\n"
  "</head>\n";
  
  pageContent += "LED is now: <span id='ledStatus'>";
  pageContent += ((ledStatus == LED_ON) ? "ON" : "OFF");
  pageContent += "</span><br><br>"
  "<button onclick=\"updateLed('ON')\">Turn LED On</button>"
  "<button onclick=\"updateLed('OFF')\">Turn LED Off</button><br />"
  "<h3>TV</H3>"
  "<button onclick=\"sendCommand('/tv', 'power')\">Power</button><button onclick=\"sendCommand('/tv', 'source')\">Source</button><br />"
  "<button onclick=\"sendCommand('/tv', 'volumeup')\">Vol +</button> <button onclick=\"sendCommand('/tv', 'volumedown')\">Vol -</button><br />"
  "<button onclick=\"sendCommand('/tv', 'channelup')\">Ch +</button> <button onclick=\"sendCommand('/tv', 'channeldown')\">Ch -</button><br />"
  "<table>"
  "<tr><td></td><td><button onclick=\"sendCommand('/tv', 'up')\">&uarr;</button></td><td></td></tr>"
  "<tr><td><button onclick=\"sendCommand('/tv', 'left')\">&larr;</button></td><td><button onclick=\"sendCommand('/tv', 'ok')\">&#9635;</button></td><td><button onclick=\"sendCommand('/tv', 'right')\">&rarr;</button></td></tr>"
  "<tr><td></td><td><button onclick=\"sendCommand('/tv', 'down')\">&darr;</button></td><td></td></tr>"
  "<tr><td><button onclick=\"sendCommand('/tv', 'back')\">Back</button></td><td></td><td><button onclick=\"sendCommand('/tv', 'exit')\">Exit</button></td></tr>"
  "</table>\n"
  "<button onclick=\"sendCommand('/tv', 'stop')\">&#9724;</button><button onclick=\"sendCommand('/tv', 'rewind')\">&#9194;</button><button onclick=\"sendCommand('/tv', 'play')\">&#9658;</button><button onclick=\"sendCommand('/tv', 'pause')\">&#10074;&#10074;</button><button onclick=\"sendCommand('/tv', 'forward')\">&#9193;</button><br/>"
  "Last command result: <input type='text' id='commandResult' disabled/>"
  "</html>";

  server.send(200, "text/html", pageContent);
}

void handleServiceDescriptionRequest(){
  Serial.println(" ########## Responding to eventservice.xml ... ########\n");
  String eventservice_xml = "<?scpd xmlns=\"urn:Belkin:service-1-0\"?>"
    "<actionList>"
      "<action>"
        "<name>SetBinaryState</name>"
        "<argumentList>"
          "<argument>"
            "<retval/>"
            "<name>BinaryState</name>"
            "<relatedStateVariable>BinaryState</relatedStateVariable>"
            "<direction>in</direction>"
          "</argument>"
        "</argumentList>"
         "<serviceStateTable>"
          "<stateVariable sendEvents=\"yes\">"
            "<name>BinaryState</name>"
            "<dataType>Boolean</dataType>"
            "<defaultValue>0</defaultValue>"
          "</stateVariable>"
          "<stateVariable sendEvents=\"yes\">"
            "<name>level</name>"
            "<dataType>string</dataType>"
            "<defaultValue>0</defaultValue>"
          "</stateVariable>"
        "</serviceStateTable>"
      "</action>"
    "</scpd>\r\n"
    "\r\n";
        
  server.send(200, "text/plain", eventservice_xml.c_str());
  Serial.println("Sending :");
  Serial.println(eventservice_xml);
}

void handleSetupRequest(){
  Serial.println(" ########## Responding to setup.xml ... ########\n");
  
  IPAddress localIP = WiFi.localIP();
  char s[16];
  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
  
  String setup_xml = "<?xml version=\"1.0\"?>"
        "<root>"
         "<device>"
            "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
            "<friendlyName>"+ device_name +"</friendlyName>"
            "<manufacturer>Belkin International Inc.</manufacturer>"
            "<modelName>Emulated Socket</modelName>"
            "<modelNumber>3.1415</modelNumber>"
            "<UDN>uuid:"+ persistent_uuid +"</UDN>"
            "<serialNumber>221517K0101769</serialNumber>"
            "<binaryState>0</binaryState>"
            "<serviceList>"
              "<service>"
                  "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                  "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                  "<controlURL>/upnp/control/basicevent1</controlURL>"
                  "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                  "<SCPDURL>/eventservice.xml</SCPDURL>"
              "</service>"
          "</serviceList>" 
          "</device>"
        "</root>\r\n"
        "\r\n";
        
    server.send(200, "text/xml", setup_xml.c_str());
    
    Serial.print("Sending :");
    Serial.println(setup_xml);
}

void handleNotFound(){
  server.send(404);
}

boolean connectUDP(){
  boolean state = false;
  
  Serial.println("");
  Serial.println("Connecting to UDP");
  
  if(UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti)) {
    Serial.println("Connection successful");
    state = true;
  }
  else{
    Serial.println("Connection failed");
  }
  
  return state;
}

void prepareIds() {
  uint32_t chipId = ESP.getChipId();
  char uuid[64];
  sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
        (uint16_t) ((chipId >> 16) & 0xff),
        (uint16_t) ((chipId >>  8) & 0xff),
        (uint16_t)   chipId        & 0xff);

  serialNumber = String(uuid);
  persistent_uuid = "Socket-1_0-" + serialNumber;
}

void respondToSearch() {
  Serial.println("");
  Serial.print("Sending response to ");
  Serial.println(UDP.remoteIP());
  Serial.print("Port : ");
  Serial.println(UDP.remotePort());

  IPAddress localIP = WiFi.localIP();
  char s[16];
  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

  String response = 
   "HTTP/1.1 200 OK\r\n"
   "CACHE-CONTROL: max-age=86400\r\n"
   "DATE: Fri, 2 Feb 2016 04:56:29 GMT\r\n"
   "EXT:\r\n"
   "LOCATION: http://" + String(s) + ":80/setup.xml\r\n"
   "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
   "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
   "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
   "ST: urn:Belkin:device:**\r\n"
   "USN: uuid:" + persistent_uuid + "::urn:Belkin:device:**\r\n"
   "X-User-Agent: redsonic\r\n\r\n";

  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
  UDP.write(response.c_str());
  UDP.endPacket();                    

  Serial.println("Response sent !");
}

void setup() {
  Serial.begin(115200);
  delay(10);
 
  pinMode(LED_PIN, OUTPUT);
  updateLed(!LED_ON);

  prepareIds();
 
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  wifiConnected = true;

  udpConnected = connectUDP();

  server.on("/", handleRoot);
  server.on("/tv", handleTvPath);
  server.on("/led", handleLedPath);
  server.on("/upnp/control/basicevent1", HTTP_POST, handleControlEvent);
  server.on("/eventservice.xml", HTTP_GET, handleServiceDescriptionRequest);
  server.on("/setup.xml", HTTP_GET, handleSetupRequest);
  server.onNotFound( handleNotFound );
 
  // Start the server
  server.begin();
  Serial.println("Server started");
 
  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  irSend.begin();
  Serial.println("irSend started");

  updateLed(LED_ON);
}
 
void loop() {
  server.handleClient();

  // if there's data available, read a packet
  // check if the WiFi and UDP connections were successful
  if(wifiConnected){
    if(udpConnected){    
      // if there’s data available, read a packet
      int packetSize = UDP.parsePacket();
      
      if(packetSize) {
        Serial.println("");
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remote = UDP.remoteIP();
        
        for (int i =0; i < 4; i++) {
          Serial.print(remote[i], DEC);
          if (i < 3) {
            Serial.print(".");
          }
        }
        
        Serial.print(", port ");
        Serial.println(UDP.remotePort());
        
        int len = UDP.read(packetBuffer, 255);
        
        if (len > 0) {
            packetBuffer[len] = 0;
        }

        String request = packetBuffer;
         
        if(request.indexOf("M-SEARCH") > 0) {
          if(request.indexOf("urn:Belkin:device:**") > 0) {
            Serial.println("Responding to search request ...");
            respondToSearch();
          }
        }
      }
        
      delay(10);
    }
  }
  else {
    // Turn on/off to indicate cannot connect ..      
  }
}
 
