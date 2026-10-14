#include <Arduino.h>

const uint8_t PIN_LED_RED   = PA6;
const uint8_t PIN_LED_GREEN = PA7;
const uint8_t PIN_LED_BLUE  = PB0;
const uint8_t PIN_BUTTON    = PB12;
const uint8_t PIN_HEARTBEAT = PC13;

const uint32_t HEARTBEAT_INTERVAL_MS = 900;

uint32_t prev_heartbeat_time = 0;
uint32_t prev_rgb_test_time  = 0;
uint32_t prev_report_time    = 0;

uint8_t rgb_test_step = 0;
bool heartbeat_state = false;
int last_button_state = HIGH;

void setup() {
    Serial.begin(115200);
    delay(50);

    pinMode(PIN_HEARTBEAT, OUTPUT);
    digitalWrite(PIN_HEARTBEAT, HIGH);

    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);

    analogWrite(PIN_LED_RED, 0);
    analogWrite(PIN_LED_GREEN, 0);
    analogWrite(PIN_LED_BLUE, 0);

    pinMode(PIN_BUTTON, INPUT_PULLUP);

    Serial.println(F("============================================="));
    Serial.println(F(" Practical 2 - Task 1: Wokwi Schematic Test  "));
    Serial.println(F(" Variant: 7 (Heartbeat: 900ms, RGB step: 30ms)"));
    Serial.println(F("============================================="));
    Serial.println(F("[OK] Hardware initialized successfully."));
    Serial.println(F("[OK] Starting schematic verification loop..."));
}

void loop() {
    uint32_t current_time = millis();

    // 1. Задача Heartbeat (PC13, період 900 мс)
    if (current_time - prev_heartbeat_time >= HEARTBEAT_INTERVAL_MS) {
        prev_heartbeat_time = current_time;
        heartbeat_state = !heartbeat_state;
        digitalWrite(PIN_HEARTBEAT, heartbeat_state ? LOW : HIGH);
    }

    // 2. Тестове перемикання кольорів RGB кожні 1000 мс для перевірки каналів
    if (current_time - prev_rgb_test_time >= 1000) {
        prev_rgb_test_time = current_time;
        rgb_test_step = (rgb_test_step + 1) % 4;

        switch (rgb_test_step) {
            case 0:
                analogWrite(PIN_LED_RED, 255);
                analogWrite(PIN_LED_GREEN, 0);
                analogWrite(PIN_LED_BLUE, 0);
                break;
            case 1:
                analogWrite(PIN_LED_RED, 0);
                analogWrite(PIN_LED_GREEN, 255);
                analogWrite(PIN_LED_BLUE, 0);
                break;
            case 2:
                analogWrite(PIN_LED_RED, 0);
                analogWrite(PIN_LED_GREEN, 0);
                analogWrite(PIN_LED_BLUE, 255);
                break;
            case 3:
                analogWrite(PIN_LED_RED, 0);
                analogWrite(PIN_LED_GREEN, 0);
                analogWrite(PIN_LED_BLUE, 0);
                break;
        }
    }

    // 3. Перевірка кнопки (PB12)
    int btn_read = digitalRead(PIN_BUTTON);
    if (btn_read != last_button_state) {
        last_button_state = btn_read;
        if (btn_read == LOW) {
            Serial.print(F("["));
            Serial.print(current_time);
            Serial.println(F(" ms] Button PB12 PRESSED (Circuit OK!)"));
        } else {
            Serial.print(F("["));
            Serial.print(current_time);
            Serial.println(F(" ms] Button PB12 RELEASED"));
        }
    }

    // 4. Періодичний звіт раз на 2000 мс
    if (current_time - prev_report_time >= 2000) {
        prev_report_time = current_time;
        Serial.print(F("[Uptime: "));
        Serial.print(current_time / 1000);
        Serial.print(F("s] HB(PC13): "));
        Serial.print(heartbeat_state ? "ON" : "OFF");
        Serial.print(F(" | RGB Channel Test: "));
        const char* step_names[] = {"RED", "GREEN", "BLUE", "OFF"};
        Serial.println(step_names[rgb_test_step]);
    }
}
