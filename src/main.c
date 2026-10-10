#include "stm32f103xb.h"

void hardware_init(void){
    /*
        RCC — регістр керування тактуванням
        APB2ENR — регістр дозволу тактування на шині APB2
        |= — додаємо біт, не затираємо інші
        RCC_APB2ENR_IOPAEN — конкретний біт що вмикає GPIOA
    */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRL &= ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5);
    GPIOA->CRL |= GPIO_CRL_MODE5_1;
}

int main(void){
    hardware_init();

    while(1){
        __asm__("wfi");
    }

    return 0;
}