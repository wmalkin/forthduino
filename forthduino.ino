

#include "forth.h"
#include "octo.h"
#include "forthduino.h"
#include "alpha.h"


void setup() {
  // initialize the forth runtime
  forth_init();
  octo_setup();
  forthduino_setup();
  alpha_init();

  // run the bootfile.forth from the SD card, if it exists
  forth_run("'boot.forth run-file");
}


void loop() {
  forthduino_loop();
}
