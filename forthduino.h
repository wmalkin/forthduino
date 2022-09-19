//
// Forth functions for Arduino-specific functionality
//

#ifndef forthduino_h
#define forthduino_h

#ifdef __IMXRT1062__
#define _TEENSY41_
#else
#define _ESP32WROVER_
#endif

void forthduino_setup();
void forthduino_loop();

#endif
