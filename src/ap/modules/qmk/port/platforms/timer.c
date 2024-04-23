#include "timer.h"




void timer_init(void)
{

}

void timer_clear(void)
{

}

uint16_t timer_read(void)
{
  return millis();
}

uint32_t timer_read32(void)
{
  return millis();
}

uint16_t timer_elapsed(uint16_t last)
{
    uint32_t t;

    t = millis();

    return TIMER_DIFF_16((t & 0xFFFF), last);  
}

uint32_t timer_elapsed32(uint32_t last)
{
  return millis()-last;
}