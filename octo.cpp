
//
// Forth wrapper for OctoWS2811 board and RGB LED control
//

#include "forth.h"
#include <OctoWS2811.h>

const int maxLedsPerStrip = 1100;
int ledsPerStrip;
int totalLeds;

DMAMEM int displayMemory[maxLedsPerStrip*6];
int drawingMemory[maxLedsPerStrip*6];
const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 *leds;

int *ledmap;

void op_octo_init()
{
  ledsPerStrip = forth_stack()->popint();
  totalLeds = forth_stack()->popint();
  leds = new OctoWS2811(ledsPerStrip, displayMemory, drawingMemory, config);
  leds->begin();
  ledmap = new int[totalLeds];
  for (int i = 0; i < totalLeds; i++)
    ledmap[i] = i;
}


int getTotalLEDs()
{
  return totalLeds;
}


void op_showa()
{
    Value* v = forth_stack()->pop();
    if (v->vtype == ARRAY && leds) {
        for (int i = 0; i < v->len; i++)
          leds->setPixel(ledmap[i], v->ia[i]);
        leds->show();
    }
    vfree(v);
}

void op_reada()
{
    Value *v = forth_stack()->pop();
    if (v->vtype == ARRAY) {
        if (leds)
          for (int i = 0; i < v->len; i++)
              v->ia[i] = leds->getPixel(ledmap[i]);
        forth_stack()->push(v);
    }
}

void octo_put(int idx, int c)
{
  if (leds && idx >= 0 && idx < totalLeds)
    leds->setPixel(ledmap[idx], c);
}

void op_pixel()
{
  int idx = forth_stack()->popint();
  int c = forth_stack()->popint();
  octo_put(idx, c);
}

void op_fill()
{
  int pmax = forth_stack()->popint();
  int pmin = forth_stack()->popint();
  int c = forth_stack()->popint();
  if (leds)
    for (int i = pmin; i < pmax; i++)
      octo_put(i, c);
}

void op_set_map()
{
  Value* m = forth_stack()->pop();
  if (m->vtype == ARRAY)
    for (int i = 0; i < m->len && i < totalLeds; i++)
      ledmap[i] = m->ia[i];
  vfree(m);
}

void op_show()
{
    if (leds)
      leds->show();
}

void op_dma_wait()
{
  if (leds) {
    while (leds->busy())
      delayMicroseconds(100);
  }
}


void octo_setup()
{
  forth_dict()->def("octo:init", &op_octo_init);
  forth_dict()->def("octo:showa", &op_showa);
  forth_dict()->def("octo:reada", &op_reada);
  forth_dict()->def("octo:pixel", &op_pixel);
  forth_dict()->def("octo:fill", &op_fill);
  forth_dict()->def("octo:show", &op_show);
  forth_dict()->def("octo:dma-wait", &op_dma_wait);
  forth_dict()->def("octo:set-map", &op_set_map);
}
