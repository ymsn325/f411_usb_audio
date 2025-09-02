#include "tim.h"
#include <stm32f411xe.h>

void tim1_init(void) {
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

  TIM1->PSC = 9600;
  TIM1->ARR = 5000;
  TIM1->DIER |= TIM_DIER_UIE;
  NVIC_SetPriority(TIM1_UP_TIM10_IRQn, 1);
  NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
  TIM1->CR1 |= TIM_CR1_CEN;
}

void TIM1_UP_TIM10_IRQHandler(void) {
  if (TIM1->SR & TIM_SR_UIF_Msk) {
    TIM1->SR &= ~TIM_SR_UIF;
    GPIOD->ODR ^= 1 << GPIO_ODR_OD15_Pos;
  }
}
