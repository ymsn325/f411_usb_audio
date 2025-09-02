#include "clock.h"
#include "gpio.h"
#include "tim.h"
#include "usb.h"
#include <stm32f411xe.h>

int main(void) {
  clock_init();
  gpio_init();
  tim1_init();
  usb_init();

  while (1)
    ;
}
