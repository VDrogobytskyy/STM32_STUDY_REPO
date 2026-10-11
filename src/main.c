#include "stm32f103xb.h"
#include <stdbool.h>

volatile bool button_mode_change = false;
volatile uint32_t system_millis = 0;
volatile bool wake_up = false;
volatile bool led_light = false;

typedef enum {chill = 0, warning = 1, stress = 2} state;
typedef enum {automate = 0, manual = 1} mode;

volatile state cur_state = chill;
volatile mode cur_mode = automate;


void SysTick_Handler(void) {
    system_millis++;

    static uint32_t ticks = 0;
    static bool led_state = false;
    ticks++;

    if(ticks % 250 == 0){
        wake_up = true;
    }

    if(cur_mode == automate){
        if(cur_state == chill){
            GPIOA->BSRR = GPIO_BSRR_BR5;

            led_light = false;

        }else if(cur_state == warning){
            if((ticks % 1000) == 0){
                GPIOA->BSRR = GPIO_BSRR_BS5;
                led_light = !led_light;
                
                if(led_light == true){
                    GPIOA->BSRR = GPIO_BSRR_BS5;
                }else{
                    GPIOA->BSRR = GPIO_BSRR_BR5;
                }
            }
        }else{
            if((ticks % 250) == 0){
                GPIOA->BSRR = GPIO_BSRR_BS5;
                led_light = !led_light;
                
                if(led_light == true){
                    GPIOA->BSRR = GPIO_BSRR_BS5;
                }else{
                    GPIOA->BSRR = GPIO_BSRR_BR5;
                }
            }
        }
    }else{
        GPIOA->BSRR = GPIO_BSRR_BS5;
        led_light = true;
    }
}

void EXTI1_IRQHandler(void){
    if(EXTI->PR & EXTI_PR_PR1){
        EXTI->PR = EXTI_PR_PR1;

        static uint32_t last_button_press = 0;
        uint32_t current_time = system_millis;

        if(current_time - last_button_press > 200){
            last_button_press = current_time;

            cur_mode = cur_mode ? automate : manual;
            if(cur_mode == automate){
                send_string("Mode changed to automate\r\n");
            }else{
                send_string("Mode changed to manual\r\n");
            }
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

void send_char(char c) {
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = (c & 0xFF);
}

void send_string(const char *str){
    while(*str){
        send_char(*str++);
    }
}

void send_num(uint32_t num){
    char buffer[15];
    int counter = 0;

    if(num == 0){
        send_char('0');
        return;
    }

    while(num > 0){
        buffer[counter++] = (num % 10) + '0';
        num = num / 10;
    }

    while(counter > 0){
        send_char(buffer[--counter]);
    }
    
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
                    | RCC_APB2ENR_ADC1EN
                    | RCC_APB2ENR_USART1EN;

    GPIOA->CRH &= ~(GPIO_CRH_CNF9 | GPIO_CRH_MODE9);
    GPIOA->CRH |= (GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9_1);

    USART1->BRR = 8000000 / 115200;

    USART1->CR1 |= USART_CR1_TE | USART_CR1_UE;


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

    send_string("\nProgram Started\r\n");

    state start_state = chill;
    uint16_t last_potent = 0;
    
    while(1){

        if(wake_up){
            wake_up = false;
            
            uint16_t potent = potentiometr_value();
            if(potent >= 90){
                cur_state = stress;
            }else if(potent >= 45){
                cur_state = warning;
            }else{
                cur_state = chill;
            }
            

            if(cur_state != start_state | last_potent != potent){
                start_state = cur_state;
                send_string("Value: ");
                send_num(potent);
                send_string("'%', current state: ");
                
                last_potent = potent;

                switch (cur_state){
                    case chill:
                        send_string("chill\r\n");
                        break;
                    case warning:
                        send_string("warning\r\n");
                        break;
                    case stress:
                        send_string("stress\r\n");
                        break;
                    default:
                        break;
                }

            }
        }


        __asm__("wfi");
    }

    return 0;
}