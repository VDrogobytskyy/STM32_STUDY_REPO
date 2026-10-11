#include "stm32f103xb.h"
#include <stdbool.h>

volatile bool event_mode_change = false;
volatile bool button_mode_change = false;
volatile uint32_t system_millis = 0;


void SysTick_Handler(void) {
    system_millis++;
}

void EXTI1_IRQHandler(void){
    if(EXTI->PR & EXTI_PR_PR1){
        EXTI->PR = EXTI_PR_PR1;

        static uint32_t last_button_press = 0;
        uint32_t current_time = system_millis;

        if(current_time - last_button_press > 200){
            last_button_press = current_time;
            button_mode_change = !button_mode_change;
        }
        
    }
}

uint16_t potentiometr_value(void){
    ADC1->CR2 |= ADC_CR2_ADON;

    while (!(ADC1->SR & ADC_SR_EOC));

    uint16_t value = ADC1->DR & 0x0FFF;

    uint16_t return_value = (value * 100) / 4095;

    return return_value;
}
void hardware_init(void){
    /*
        RCC — регістр керування тактуванням
        APB2ENR — регістр дозволу тактування на шині APB2
        |= — додаємо біт, не затираємо інші
        RCC_APB2ENR_IOPAEN — конкретний біт що вмикає GPIOA
    */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN 
                    | RCC_APB2ENR_AFIOEN
                    | RCC_APB2ENR_ADC1EN;

    // підключаємо Лед-лампочку і ставим біти в положення OUTPUT і push-pull режим
    GPIOA->CRL &= ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5);
    GPIOA->CRL |= GPIO_CRL_MODE5_1;

    //підключаєм кнопку
    GPIOA->CRL &= ~(GPIO_CRL_CNF1 | GPIO_CRL_MODE1);
    GPIOA->CRL |= GPIO_CRL_CNF1_1;
    GPIOA->ODR |= GPIO_ODR_ODR1;
    
    // потенціометр на input
    GPIOA->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0);

    //спочатку вказуємо що це порт А 
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI1;
    EXTI->IMR |= EXTI_IMR_MR1; // дозвіл
    EXTI->FTSR |= EXTI_FTSR_TR1;
    NVIC_EnableIRQ(EXTI1_IRQn); //ядра NVIC

    //adc - аналог в діджитал
    ADC1->CR2 |= ADC_CR2_ADON;
    ADC1->SQR3 = 0;

    SysTick_Config(8000);
}

int main(void){
    hardware_init();

    //GPIOA->BSRR = GPIO_BSRR_BS5;
    while(1){
        if(button_mode_change){
            
            //button_mode_change = !button_mode_change;

            GPIOA->BSRR = GPIO_BSRR_BR5;

        }else{
            uint16_t potent = potentiometr_value();
            if(potent >= 90){
                GPIOA->BSRR = GPIO_BSRR_BS5;
            }else{
                GPIOA->BSRR = GPIO_BSRR_BR5;
            }
            //GPIOA->BSRR = GPIO_BSRR_BR5;
        }
        __asm__("wfi");
    }

    return 0;
}