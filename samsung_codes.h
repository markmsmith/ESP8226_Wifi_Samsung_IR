#include <pgmspace.h>

// Store the mapping of command names to command codes
// There's probably a better way to do this . . .

// number of characters for longest command name with space for terminating \0
#define LONGEST_NAME 12

struct CodeNamePair {
  // name of the command, eg "power"
  prog_char name[LONGEST_NAME];

  // the command code to use from LIRC eg 0xE0E040BF
  uint16_t code;
};

const CodeNamePair CODES[] PROGMEM = {
  { "power",       0x40BF },
  { "source",      0x807F },
  { "1",           0x20DF },
  { "2",           0xA05F },
  { "3",           0x609F },
  { "4",           0x10EF },
  { "5",           0x906F },
  { "6",           0x50AF },
  { "7",           0x30CF },
  { "8",           0xB04F },
  { "9",           0x708F },
  { "previous",    0xC837 }, //previous channel
  { "0",           0x8877 },
  { "text",        0x34CB },  //TTX/MIX
  { "mute",        0xF00F },
  { "channelup",   0x48B7 },
  { "channeldown", 0x08F7 },
  { "volumeup",    0xE01F },
  { "volumedown",  0xD02F },
  { "list",        0xD629 },  //CH LIST
  { "media",       0x31CE },  //MEDIA.P
  { "menu",        0x58A7 },  
  { "epg",         0xF20D },  //GUIDE
  { "tools",       0xD22D },
  { "up",          0x06F9 },
  { "right",       0x46B9 },
  { "left",        0xA659 },
  { "down",        0x8679 },
  { "ok",          0x16E9 },  //ENTER
  { "back",        0x1AE5 },  //RETURN
  { "exit",        0xB44B },
  { "a",           0x36C9 },  //RED
  { "b",           0x28D7 },  //GREEN
  { "c",           0xA857 },  //YELLOW
  { "d",           0x6897 },  //BLUE
  { "start",       0xFC03 },  //E-MANUAL
  { "subtitle",    0xA45B },  //AD/SUBT.
  { "stop",        0x629D },
  { "rewind",      0xA25D },
  { "play",        0xE21D },
  { "pause",       0x52AD },
  { "forward",     0x12ED },
  { "p",           0x7C83 }  //P.SIZE
};

const int NO_CODES = (int) (sizeof(CODES) / sizeof(CodeNamePair));

// prefix for all samsung devices (I think this is device 7, subdevice 7, bit order inverted within their bytes, then both bytes shifted up to the top of the dword, since the command is the bottom (LSB) byte)
const unsigned long SAMSUNG_PREFIX = 0xE0E00000;

unsigned long findCodeByName(const char *name){
  for(int i=0; i < NO_CODES; i++){
    
    if( strcmp_P(name, CODES[i].name) == 0 ){
      // found a match, so need to prefixed by the samsung device + subdevice
      uint16_t commandCode = pgm_read_word(&CODES[i].code);
      return SAMSUNG_PREFIX | commandCode;
    }
  }

  return 0L;
}

