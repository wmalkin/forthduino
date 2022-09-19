

#include "forth.h"
#include "led.h"
#include "forthduino.h"
#include "teensy41.h"
#include "alpha.h"


void setup() {
  // initialize the forth runtime
  forth_init();
  led_setup();
  forthduino_setup();
  #ifdef _TEENSY41_
  teensy41_setup();
  #endif
  alpha_init();

  // run the bootfile.forth from the SD card, if it exists
  forth_run("'boot.forth file:run");
}


void loop() {
  forthduino_loop();
  #ifdef _TEENSY41_
  teensy41_loop();
  #endif
}
