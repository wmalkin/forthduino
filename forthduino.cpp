//
//
//

#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#include "forth.h"

#include "SdFat.h"

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3
/*
  Change the value of SD_CS_PIN if you are using SPI and
  your hardware does not use the default value, SS.  
  Common values are:
  Arduino Ethernet shield: pin 4
  Sparkfun SD shield: pin 8
  Adafruit SD shields and modules: pin 10
*/

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI)
#endif  // HAS_SDIO_CLASS

#if SD_FAT_TYPE == 0
SdFat sd;
#define TFILE File
#elif SD_FAT_TYPE == 1
SdFat32 sd;
#define TFILE File32
#elif SD_FAT_TYPE == 2
SdExFat sd;
#define TFILE ExFile
#elif SD_FAT_TYPE == 3
SdFs sd;
#define TFILE FsFile
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE


//
// Ethernet interface and UDP listener
//
// byte mac[] = {
//   0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
// };
// IPAddress ip(192, 168, 1, 178);
// unsigned int localPort = 8888;
char packetBuffer[1500];
int pbofs = 0;
EthernetUDP Udp;
bool udpConnected = false;


void load_inet()
{
  unsigned int localPort = forth_stack()->popint();

  int ipa[4];
  for (int i = 3; i >= 0; i--)
    ipa[i] = forth_stack()->popint();
  IPAddress ip(ipa[0], ipa[1], ipa[2], ipa[3]);

  byte mac[6];
  for (int i = 5; i >= 0; i--)
    mac[i] = (byte)forth_stack()->popint();

  Serial.print("macaddr: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(mac[i]);
    Serial.print(" ");
  }
  Serial.println();
  Serial.print("ip address: ");
  for (int i = 0; i < 4; i++) {
    Serial.print(ipa[i]);
    if (i < 3)
        Serial.print(".");
  }
  Serial.println();
  Serial.print("port: ");
  Serial.println(localPort);

  Ethernet.begin(mac, ip);
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found");
      return;
  }
  if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
      return;
  }

  delay(1000);
  Udp.begin(localPort);
  Serial.println("udp started");
  udpConnected = true;
}



//
// Forth runtime
//
FDict* looptasks;


void opRndm()
{
    int max = forth_stack()->popint();
    forth_stack()->push( (int) random (max));
}

void opRRndm()
{
    int max = forth_stack()->popint();
    int min = forth_stack()->popint();
    forth_stack()->push( (int) random (min, max));
}

void op_delay()
{
  int ms = forth_stack()->popint();
  if (ms > 0)
    delay(ms);
}

void op_delay_us()
{
  int ms = forth_stack()->popint();
  if (ms > 0)
    delayMicroseconds(ms);
}

void op_now()
{
  forth_stack()->push((double)millis());
}

void op_pinmode()
{
  int mode = forth_stack()->popint();
  int pin = forth_stack()->popint();
  switch (mode) {
    case 1:
      pinMode(pin, INPUT);
      break;
    case 2:
      pinMode(pin, OUTPUT);
      break;
  }
}

void op_digitalread()
{
  int pin = forth_stack()->popint();
  forth_stack()->push(digitalRead(pin));
}

void op_digitalwrite()
{
  int value = forth_stack()->popint();
  int pin = forth_stack()->popint();
  digitalWrite(pin, value);
}

void op_analogread()
{
  int pin = forth_stack()->popint();
  forth_stack()->push(analogRead(pin));
}

void op_analogwrite()
{
  int value = forth_stack()->popint();
  int pin = forth_stack()->popint();
  analogWrite(pin, value);
}

void op_analogreference()
{
  int value = forth_stack()->popint();
  analogReference(value);
}

void prtvalue(Value* v)
{
  switch(v->vtype) {
    case FREE:
      Serial.print("<free>");
      break;
    case INT:
      Serial.print(v->inum);
      break;
    case FLOAT:
      Serial.print(v->fnum);
      break;
    case STR:
      Serial.print(v->str);
      break;
    case FUNC:
      Serial.print("<func>");
      break;
    case SEQ:
      Serial.print("<seq>");
      break;
    case ARRAY:
      Serial.print("<int[");
      Serial.print(v->len);
      Serial.print("]>");
      break;
    case SYM:
      Serial.print("<");
      Serial.print(v->sym->word);
      Serial.print(">");
      break;
  }
}

void dot()
{
  Value* v = forth_stack()->pop();
  prtvalue(v);
  Serial.print(" ");
  vfree(v);
}

void prtdict()
{
  Sym* sym = forth_dict()->head;
  while (sym) {
    Serial.print(sym->word);
    Serial.print(": ");
    prtvalue(sym->value);
    Serial.println();
    sym = sym->next;
  }
}

void cr()
{
  Serial.println("");
}

void prtstk()
{
  Value* itm = forth_stack()->head;
  while (itm)
  {
    prtvalue(itm);
    Serial.print(" ");
    itm = itm->next;
  }
  Serial.println();
}

void op_loopdef()
{
    char* w = forth_stack()->popstring();
    Value* v = forth_stack()->pop();
    looptasks->forget(w);
    looptasks->def(w, v);
    strdelete(w);
}

void op_loopforget()
{
    char* w = forth_stack()->popstring();
    looptasks->forget(w);
    strdelete(w);
}


void op_udp_begin()
{
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
}

void op_udp_prt()
{
  Value* v = forth_stack()->pop();
  switch(v->vtype) {
    case FREE:
      Udp.write("<free>");
      break;
    case INT:
      Udp.print(v->inum);
      break;
    case FLOAT:
      Udp.print(v->fnum);
      break;
    case STR:
      Udp.write(v->str);
      break;
    case FUNC:
      Udp.write("<func>");
      break;
    case SEQ:
      Udp.write("<seq>");
      break;
    case ARRAY:
      Udp.write("<int[");
      Udp.print(v->len);
      Udp.write("]>");
      break;
    case SYM:
      Udp.write("<");
      Udp.print(v->sym->word);
      Udp.write(">");
      break;
  }
}

void op_udp_end()
{
  Udp.endPacket();
}


void udp_ack(char *ack) {
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.write(ack);
  Udp.endPacket();
}

EthernetClient client;
unsigned long beginMicros, endMicros;
unsigned long byteCount = 0;



void udp_bootstrap() 
{
  // start client
  char server[] = "192.168.1.101";
  
  Serial.print("connecting to ");
  Serial.print(server);
  Serial.println("...");

  if (client.connect(server, 80)) {
      Serial.print("connected to ");
      Serial.println(client.remoteIP());
      client.println("GET /home/boot.forth HTTP/1.1");
      client.println("Host: 192.168.1.101");
      client.println("Authorization: Basic YWRtaW46S2l0MzFLYXQ=");
      client.println("Pragma: no-cache");
      client.println("Cache-Control: no-cache");
      client.println();
      pbofs = 0;
  } else {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }
}


void step_serial(Value* lastword)
{
    prtvalue(lastword);
    Serial.print(": ");
    prtstk();
}


void op_runfile()
{
  TFILE file;
  
  char* fname = forth_stack()->popstring();
  char inp[1024];
  if (file.open(fname, FILE_READ)) {
    forth_unu(true);
    while (file.available()) {
      int n = file.fgets(inp, 1024);
      if (n > 0) {
        for (int i = 0; i < 1024; i++)
          if (inp[i] == '\n' || inp[i] == '\r' || inp[i] == '\t')
            inp[i] = ' ';
        if (inp[0] != '/' && inp[1] != '/')
          forth_run(inp);
      }
    }
    file.close();
    forth_unu(false);
  }
  strdelete(fname);
}



bool cmd_echo = true;


void op_echo()
{
  int v = forth_stack()->popint();
  cmd_echo = (v != 0);
}


char serinput[1024];
int serlen = 0;


bool CheckSerial()
{
  while (Serial.available() > 0) {
    int b = Serial.read();
    if (b == 10 || b == 13) {
      serinput[serlen] = 0;
      if (cmd_echo) {
        Serial.print("serial>");
        Serial.println(serinput);
      }
      forth_run(serinput);
      serlen = 0;
    } else {
      serinput[serlen++] = b;
    }
    return true;
  }
  return false;
}


bool udp_writing_file = false;
TFILE udp_open_file;
char prev_ack[20] = "";

void CheckUDP()
{
  if (udpConnected) {
    int packetSize = Udp.parsePacket();
    if (packetSize) {
      memset(packetBuffer, 0, 1500);
      Udp.read(packetBuffer, 1500);

      // split ack and content
      int i = 0;
      while (packetBuffer[i] != ' ')
        i++;
      char *content = packetBuffer + i + 1;
      packetBuffer[i] = 0;

      // if this is a duplicate ack, don't process the content, but
      // still send the ack packet
      if (strcmp(packetBuffer, prev_ack) != 0) {
        if (strncmp(content, "----- ", 6) == 0) {
          // start or stop file load
          if (udp_writing_file) {
            // close the current file
            Serial.println("close udp file update");
            udp_open_file.flush();
            udp_open_file.close();
            udp_writing_file = false;
          } else {
            // open a file
            Serial.print("opening file ");
            Serial.println(&content[6]);
            if (udp_open_file.open(&content[6], FILE_WRITE)) {
              udp_writing_file = true;
              Serial.println("opened: true");
            }
          }
        } else if (udp_writing_file) {
          // add a line of content to the current file
          Serial.print("Write content: ");
          Serial.println(content);
          udp_open_file.write(content, strlen(content));
          udp_open_file.write("\n", 1);
        } else {
          if (cmd_echo) {
            Serial.print("udp>");
            Serial.println(content);
          }
          forth_run(content);
        }
      }

      // send ack
      udp_ack(packetBuffer);
      strcpy(packetBuffer, prev_ack);
    }
  }
}


// [ [  '**** quad:str ] #70 #73 loop ] 0 3 loop
void op_quad_char()
{
  char c = (char) forth_stack()->popint();
  int pos = forth_stack()->popint();
  int addr = forth_stack()->popint();
  int widx = forth_stack()->popint();

  Adafruit_AlphaNum4 a4 = Adafruit_AlphaNum4();
  switch(widx) {
    case 0:
      a4.begin(addr, &Wire);
      break;
    case 1:
      a4.begin(addr, &Wire1);
      break;
    case 2:
      a4.begin(addr, &Wire2);
      break;
  }
  

  a4.writeDigitAscii(pos, c);
  a4.writeDisplay();
}


void op_quad_str()
{
  char *s = forth_stack()->popstring();
  int addr = forth_stack()->popint();
  int widx = forth_stack()->popint();

  Adafruit_AlphaNum4 a4 = Adafruit_AlphaNum4();
  switch(widx) {
    case 0:
      a4.begin(addr, &Wire);
      break;
    case 1:
      a4.begin(addr, &Wire1);
      break;
    case 2:
      a4.begin(addr, &Wire2);
      break;
  }
  
  for (int i = 0; i < 4 && i < (int)strlen(s); i++)
    a4.writeDigitAscii(i, s[i]);
  a4.writeDisplay();
}


void op_quad_blank()
{
  forth_stack()->push("    ");
  op_quad_str();
}


void forthduino_setup()
{
  sd.begin(SD_CONFIG);
  forth_stepfunction(&step_serial);
  looptasks = new FDict();

  // define custom forth words to interact with the Arduino environment
  forth_dict()->def("udp:init", &load_inet);

  forth_dict()->def("cmd:echo", &op_echo);
  
  forth_dict()->def("rndm", &opRndm);
  forth_dict()->def("rrndm", &opRRndm);
  forth_dict()->def(".", &dot);
  forth_dict()->def("cr", &cr);
  forth_dict()->def("prtdict", &prtdict);
  forth_dict()->def("prtstk", &prtstk);
  forth_dict()->def("delay", &op_delay);
  forth_dict()->def("delayus", &op_delay_us);
  forth_dict()->def("now", &op_now);
  forth_dict()->def("pinmode", &op_pinmode);
  forth_dict()->def("digitalread", &op_digitalread);
  forth_dict()->def("digitalwrite", &op_digitalwrite);
  forth_dict()->def("analogread", &op_analogread);
  forth_dict()->def("analogreference", &op_analogreference);
  forth_dict()->def("analogwrite", &op_analogwrite);
  forth_dict()->def("udp-begin", &op_udp_begin);
  forth_dict()->def(".udp", &op_udp_prt);
  forth_dict()->def("udp-end", &op_udp_end);
  forth_dict()->def("loop-def", &op_loopdef);
  forth_dict()->def("loop-forget", &op_loopforget);
  forth_dict()->def("run-file", &op_runfile);

  Serial.begin(9600);
  Serial.println("serial started");

  forth_dict()->def("quad:char", &op_quad_char);
  forth_dict()->def("quad:str", &op_quad_str);
  forth_dict()->def("quad:blank", &op_quad_blank);

}


void loop_check(Value* task)
{
    Value* seq = task->seq->head;
    Value* rate = seq->next;
    Value* threshold = rate->next;
    double now = (double)millis();
    if (now >= threshold->fnum) {
      forth_run(seq);
      threshold->fnum = now + rate->asfloat();
    }
}


void forthduino_loop()
{
  while (CheckSerial()) {};
  CheckUDP();
  
  // do some default action on each loop
  Sym* task = looptasks->head;
  while (task) {
      loop_check(task->value);
      task = task->next;
  }
}
