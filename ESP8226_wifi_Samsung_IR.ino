#include <ir_Daikin.h>
#include <ir_Kelvinator.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

#include <ESP8266WiFi.h>

#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <WiFiUdp.h>
#include <functional>
#include "switch.h"
#include "UpnpBroadcastResponder.h"
#include "CallbackFunction.h"
 
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
 * IR_PIN (D1) on NodeMCU to 1.8k resistor (approximated with 3x 560 + 100 = 1.78k) to base of P2N2222A transitor's base (an NPN BJT)
 * Transistor collector connected to 3mm IR LED cathode
 * IR LED annode connected through 100k resistor to 3.3v of NodeMCU
 * Transistor emitter connected to NodeMCU ground
 * Note: P2N222A pins are, from left to right, collector, base, emitter, when looking at flat side (http://ardx.org/datasheet/IC-2222A.pdf)
 * Resistor calculations based on http://www.petervis.com/Raspberry_PI/Driving_LEDs_with_CMOS_and_TTL_Outputs/Driving_an_LED_Using_Transistors.html
 * to give 1.5v around 20mA being driven through IR LED and 2mA being sourced from D1.
 * 
 * Arduino IDE Setup
 * Preferences -> Additional Board Manger Urls: http://arduino.esp8266.com/stable/package_esp8266com_index.json
 * Tools -> Board Manager -> "esp8266 by ESP8266 Community" -> Install
 * 
 * Sketch -> Include Library -> Manage Libraries...
 * IRremoteESP8266
 * 
 * Install driver for virtual COM port here:
 * https://github.com/nodemcu/nodemcu-devkit/wiki/Getting-Started-on-OSX
 * 
 * Board: NodeMCU 1.0 (ESP-12E Module)
 * CPU Frequency: 80MHz
 * Flash Size: 4M (3M SPIFFS)
 * Upload speed: 115200
 * 
*/

// special value to represent that no command was supplied
const String NO_COMMAND = "NO_COMMAND" ;

boolean wifiConnected = false;

UpnpBroadcastResponder upnpBroadcastResponder;
Switch *outlet = NULL;

// the current state of the normal LED (on or off)
int ledStatus;

ESP8266WebServer server(SERVER_PORT);
IRsend irSend(IR_PIN);

void updateLed(int newState){
  ledStatus = newState;
  digitalWrite(LED_PIN, newState);
}

bool turnOnLed() {
  Serial.println("Turning on LED");
  updateLed(LED_ON);
  return true;
}

bool turnOffLed() {
  Serial.println("Turning off LED");
  updateLed(!LED_ON);
  return false;
}

void handleUpdate(String argumentName, String argumentValue){
  if ( argumentName.equals("LED") )  {
    
    // Set LED to the requested state
    if ( argumentValue.equals("ON") ) {
      turnOnLed();
    }
    else if ( argumentValue.equals("OFF") )  {
      turnOffLed();
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

void handleNotFound(){
  server.send(404);
}

void setup() {
  Serial.begin(115200);
  delay(10);
 
  pinMode(LED_PIN, OUTPUT);
  updateLed(!LED_ON);
 
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

  upnpBroadcastResponder.beginUdpMulticast();
    
  // Define your switches here. Max of UpnpBroadcastResponder.cpp MAX_SWITCHES (14)
  // Format: Alexa invocation name, local port no, on callback, off callback
  outlet = new Switch("Node plug", 88, turnOnLed, turnOffLed);

  Serial.println("Adding switches upnp broadcast responder");
  upnpBroadcastResponder.addDevice(*outlet);

  server.on("/", handleRoot);
  server.on("/tv", handleTvPath);
  server.on("/led", handleLedPath);
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
    upnpBroadcastResponder.serverLoop();
    outlet->serverLoop();
  }
}
 
