//
//
//

#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#include "forth.h"



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



void step_serial(Value* lastword)
{
    prtvalue(lastword);
    Serial.print(": ");
    prtstk();
}


char serinput[1024];
int serlen = 0;


bool CheckSerial()
{
  while (Serial.available() > 0) {
    int b = Serial.read();
    if (b == 10 || b == 13) {
      serinput[serlen] = 0;
      if (forth_getecho()) {
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
  forth_stepfunction(&step_serial);
  looptasks = new FDict();

  // define custom forth words to interact with the Arduino environment
  forth_dict()->def("rndm", &opRndm);
  forth_dict()->def("rrndm", &opRRndm);
  forth_dict()->def(".", &dot);
  forth_dict()->def("cr", &cr);
  forth_dict()->def("prtdict", &prtdict);
  forth_dict()->def("prtstk", &prtstk);
  forth_dict()->def("delay", &op_delay);
  forth_dict()->def("delayus", &op_delay_us);
  forth_dict()->def("now", &op_now);
  forth_dict()->def("pin:mode", &op_pinmode);
  forth_dict()->def("pin:dread", &op_digitalread);
  forth_dict()->def("pin:dwrite", &op_digitalwrite);
  forth_dict()->def("pin:aread", &op_analogread);
  forth_dict()->def("pin:aref", &op_analogreference);
  forth_dict()->def("pin:awrite", &op_analogwrite);
  forth_dict()->def("loop:def", &op_loopdef);
  forth_dict()->def("loop:forget", &op_loopforget);

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
  
  // do some default action on each loop
  Sym* task = looptasks->head;
  while (task) {
      loop_check(task->value);
      task = task->next;
  }
}
