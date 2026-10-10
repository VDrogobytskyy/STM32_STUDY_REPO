#include "stm32f103xb.h"
#include <stdint.h>
#include <stdbool.h>

// -------------------------------------------------------------
// 1. Стани та режими системи
// -------------------------------------------------------------
typedef enum { MODE_AUTO = 0, MODE_MANUAL = 1 } SystemMode;
typedef enum { STATE_NORMAL = 0, STATE_WARNING = 1, STATE_CRITICAL = 2 } SystemState;

volatile SystemMode current_mode = MODE_AUTO;
volatile SystemState current_state = STATE_NORMAL;
volatile uint32_t system_millis = 0;

// Прапорці подій для обробки (Слайд 20: короткі ISR)
volatile bool event_mode_changed = false;
volatile bool event_adc_sample = false;
volatile uint16_t adc_percent = 0;

// -------------------------------------------------------------
// 2. Системний таймер SysTick (викликається апаратно кожну 1 мс)
// -------------------------------------------------------------
void SysTick_Handler(void) {
    system_millis++;
    static uint32_t ticks = 0;
    static bool led_state = false;
    ticks++;

    // Кожні 250 мс: генеруємо подію вимірювання АЦП
    if ((ticks % 250) == 0) {
        event_adc_sample = true;
    }

    // Керування світлодіодом через регістр BSRR (Слайди 17-18)
    if (current_mode == MODE_AUTO) {
        if (current_state == STATE_NORMAL) {
            // NORMAL: скидання PA5 (Reset) через верхні 16 біт BSRR
            GPIOA->BSRR = GPIO_BSRR_BR5;
            led_state = false;
        } 
        else if (current_state == STATE_WARNING) {
            if ((ticks % 500) == 0) { // 500 мс інтервал
                led_state = !led_state;
                GPIOA->BSRR = led_state ? GPIO_BSRR_BS5 : GPIO_BSRR_BR5;
            }
        } 
        else if (current_state == STATE_CRITICAL) {
            if ((ticks % 100) == 0) { // 100 мс швидке тривожне блимання
                led_state = !led_state;
                GPIOA->BSRR = led_state ? GPIO_BSRR_BS5 : GPIO_BSRR_BR5;
            }
        }
    } else {
        // Режим MANUAL: світлодіод постійно горить (Set)
        GPIOA->BSRR = GPIO_BSRR_BS5;
        led_state = true;
    }
}

// -------------------------------------------------------------
// 3. Апаратне переривання кнопки (EXTI1 на ніжці PA1)
// -------------------------------------------------------------
void EXTI1_IRQHandler(void) {
    if (EXTI->PR & EXTI_PR_PR1) {
        EXTI->PR = EXTI_PR_PR1; // Очищення прапора в EXTI (записом 1)

        static uint32_t last_press = 0;
        uint32_t now = system_millis;
        // Неблокуючий debounce (Слайд 19)
        if ((now - last_press) > 200) {
            last_press = now;
            current_mode = (current_mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
            event_mode_changed = true; // Лише прапорець, без важкого коду!
        }
    }
}

// -------------------------------------------------------------
// 4. Драйвер UART для виводу даних (PA9 TX)
// -------------------------------------------------------------
void uart_putc(char c) {
    while (!(USART1->SR & USART_SR_TXE)); // Очікування готовності регістра
    USART1->DR = (c & 0xFF);
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

void uart_put_num(uint32_t val) {
    char buf[10]; int i = 0;
    if (val == 0) { uart_putc('0'); return; }
    while (val > 0) { buf[i++] = (val % 10) + '0'; val /= 10; }
    while (i > 0) uart_putc(buf[--i]);
}

// -------------------------------------------------------------
// 5. Ініціалізація заліза за електричною схемою ТЗ
// -------------------------------------------------------------
void hardware_init(void) {
    // Вмикаємо тактування периферії через RCC (Слайди 13, 17)
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN   // Порт GPIOA
                  | RCC_APB2ENR_AFIOEN   // Альтернативні функції (для EXTI)
                  | RCC_APB2ENR_ADC1EN   // АЦП 1
                  | RCC_APB2ENR_USART1EN;// UART 1

    // PA0: Analog Input (потенціометр)
    GPIOA->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0);

    // PA1: Input with Pull-up (кнопка)
    GPIOA->CRL &= ~(GPIO_CRL_CNF1 | GPIO_CRL_MODE1);
    GPIOA->CRL |= GPIO_CRL_CNF1_1;
    GPIOA->ODR |= GPIO_ODR_ODR1; // Внутрішня підтяжка до 3.3V

    // PA5: Output push-pull 2MHz (світлодіод)
    GPIOA->CRL &= ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5);
    GPIOA->CRL |= GPIO_CRL_MODE5_1;

    // PA9: Alternate Function output (UART TX)
    GPIOA->CRH &= ~(GPIO_CRH_CNF9 | GPIO_CRH_MODE9);
    GPIOA->CRH |= (GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9);

    // Налаштування АЦП1
    ADC1->CR2 |= ADC_CR2_ADON;
    ADC1->SQR3 = 0; // Канал 0 (PA0)

    // Налаштування UART1 на 115200 бод (при частоті ядра 8 МГц)
    USART1->BRR = 8000000 / 115200;
    USART1->CR1 |= USART_CR1_TE | USART_CR1_UE;

    // Налаштування EXTI1 для кнопки на PA1
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI1; // Порт A -> лінія 1
    EXTI->IMR  |= EXTI_IMR_MR1;             // Дозвіл переривання
    EXTI->FTSR |= EXTI_FTSR_TR1;            // Спрацьовувати на спад (Falling Edge)
    NVIC_EnableIRQ(EXTI1_IRQn);            // Увімкнути в контролері ядра NVIC

    // Запуск системного таймера на 1 мс (8000 тактів при 8 МГц)
    SysTick_Config(8000);
}

// Зчитування результату АЦП
uint16_t read_adc(void) {
    ADC1->CR2 |= ADC_CR2_ADON; // Запуск перетворення
    while (!(ADC1->SR & ADC_SR_EOC)); // Очікування завершення
    uint32_t raw = ADC1->DR & 0x0FFF;
    return (uint16_t)((raw * 100) / 4095); // 12-бітний АЦП (0..4095)
}

// -------------------------------------------------------------
// 6. Головний цикл: Подієвий автомат + Апаратний сон (WFI)
// -------------------------------------------------------------
int main(void) {
    hardware_init();
    uart_puts("\r\n========================================\r\n");
    uart_puts("STM32 Native CMSIS System Started\r\n");
    uart_puts("Initial Mode: AUTO\r\n");
    uart_puts("========================================\r\n");

    while (1) {
        // Подія 1: Натиснуто кнопку
        if (event_mode_changed) {
            event_mode_changed = false;
            uart_puts("\r\n>>> [EVENT] Mode changed to: ");
            uart_puts(current_mode == MODE_AUTO ? "AUTO" : "MANUAL");
            uart_puts(" <<<\r\n\r\n");
        }

        // Подія 2: Період вимірювання АЦП від таймера
        if (event_adc_sample) {
            event_adc_sample = false;
            adc_percent = read_adc();

            SystemState old = current_state;
            if (adc_percent <= 33)      current_state = STATE_NORMAL;
            else if (adc_percent <= 66) current_state = STATE_WARNING;
            else                        current_state = STATE_CRITICAL;

            // Звіт у термінал при зміні стану
            if (current_state != old) {
                uart_puts("ADC: ");
                uart_put_num(adc_percent);
                uart_puts("% | State: ");
                if (current_state == STATE_NORMAL)        uart_puts("NORMAL\r\n");
                else if (current_state == STATE_WARNING)  uart_puts("WARNING\r\n");
                else                                      uart_puts("CRITICAL\r\n");
            }
        }

        // Апаратна інструкція сну: процесор засинає і не крутить порожній цикл!
        // Прокидається лише по перериванню SysTick або EXTI1
        __asm__ volatile ("wfi");
    }

    return 0;
}