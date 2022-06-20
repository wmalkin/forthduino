//
// Forth wrapper for RGB LED library
//

#ifndef forth_octo_h
#define forth_octo_h

void octo_setup();

int getTotalLEDs();
void octo_put(int idx, int c);

#endif
