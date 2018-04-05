Alexa TV
========
Sketch to run a basic web server than lets you select IR commands to transmit to control a Samsung TV.  
The set of supported commands are enumerated in samsung_codes.h  
Commands should be supplied as the query parameter command as either a GET or POST  
  http://<nodeMCU IP>:80/tv?command=<command>  
eg  
  http://192.168.1.27:80/tv?command=power  
  
You can also toggle the onboard LED (which is on when D0 is low and vice versa) with  
  http://<nodeMCU IP>:80/led?command=<command>  
where command is one of ON or OFF  

Basic wiring:  
IR_PIN (D1) on NodeMCU to 100k resistor to IR LED anode.  
IR LED cathode to ground on NodeMCU.  

Boosted Power Wiring:  
IR_PIN (D1) on NodeMCU to 1.8k resistor (approximated with 3x 560 + 100 = 1.78k) to base of P2N2222A transitor's base (an NPN BJT)  
Transistor collector connected to 3mm IR LED cathode  
IR LED annode connected through 100k resistor to 3.3v of NodeMCU  
Transistor emitter connected to NodeMCU ground  
Note: P2N222A pins are, from left to right, collector, base, emitter, when looking at flat side (http://ardx.org/datasheet/IC-2222A.pdf)  
Resistor calculations based on http://www.petervis.com/Raspberry_PI/Driving_LEDs_with_CMOS_and_TTL_OutputsDriving_an_LED_Using_Transistors.html  
to give 1.5v around 20mA being driven through IR LED and 2mA being sourced from D1.

Arduino IDE Setup
-----------------
Preferences -> Additional Board Manger Urls: http://arduino.esp8266.com/stable/package_esp8266com_index.json  
Tools -> Board Manager -> "esp8266 by ESP8266 Community" -> Install  

Sketch -> Include Library -> Manage Libraries...
IRremoteESP8266

Install driver for virtual COM port here:
https://github.com/nodemcu/nodemcu-devkit/wiki/Getting-Started-on-OSX

Board: NodeMCU 1.0 (ESP-12E Module)
CPU Frequency: 80MHz
Flash Size: 4M (3M SPIFFS)
Upload speed: 115200

To Do
-----
[] Switch on/off command to toggle tv power rather than LED
[] Tidy up UDPResponder and Switch to do proper OOP
[] Experiment with faking other types of devices (saw some sort of sound discovery) to give natural commands for volume up/down, play/pause etc. 


Credits
-------
Device IR codes from LIRC project:  
http://lirc-remotes.sourceforge.net/  

IR sending library for ESP8266:  
https://github.com/markszabo/IRremoteESP8266  

Alexa UDP discovery based on:  
https://github.com/Syberteck/esp8266-Smart-Plug

