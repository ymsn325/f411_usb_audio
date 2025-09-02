#include "clock.h"
#include <stm32f411xe.h>

void clock_init(void) {
  RCC->CR |= RCC_CR_HSEON;
  while (!(RCC->CR & RCC_CR_HSERDY_Msk))
    ;

  RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;
  RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP |
                    RCC_PLLCFGR_PLLQ);
  RCC->PLLCFGR |= (4 << RCC_PLLCFGR_PLLM_Pos) | (96 << RCC_PLLCFGR_PLLN_Pos) |
                  (4 << RCC_PLLCFGR_PLLQ_Pos);
  RCC->CR |= RCC_CR_PLLON;
  while (!(RCC->CR & RCC_CR_PLLRDY_Msk))
    ;

  RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

  FLASH->ACR |= FLASH_ACR_LATENCY_3WS;

  RCC->CFGR &= RCC_CFGR_SW;
  RCC->CFGR |= RCC_CFGR_SW_PLL;
}
