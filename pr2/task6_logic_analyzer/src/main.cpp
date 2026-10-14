#include <Arduino.h>

const uint8_t PIN_LED_RED   = PA6;
const uint8_t PIN_LED_GREEN = PA7;
const uint8_t PIN_LED_BLUE  = PB0;
const uint8_t PIN_BUTTON    = PB12;
const uint8_t PIN_HEARTBEAT = PC13;

const uint32_t HEARTBEAT_INTERVAL_MS = 900;

uint32_t prev_heartbeat_time = 0;
bool heartbeat_state = false;

uint8_t current_phase = 0;
uint32_t phase_start_time = 0;

void setup() {
    Serial.begin(115200);
    delay(50);

    pinMode(PIN_HEARTBEAT, OUTPUT);
    digitalWrite(PIN_HEARTBEAT, HIGH);

    pinMode(PIN_BUTTON, INPUT_PULLUP);

    Serial.println(F("=================================================="));
    Serial.println(F(" Practical 2 - Task 6: Logic Analyzer Benchmarking"));
    Serial.println(F(" Variant: 7 (D0:PA6, D1:PA7, D2:PB0, D3:PC13)     "));
    Serial.println(F("=================================================="));
    Serial.println(F("[0s] Starting Phase 1: PWM = 64 (Duty: 25.1%) on PA6"));

    analogWrite(PIN_LED_RED, 64);
    analogWrite(PIN_LED_GREEN, 0);
    analogWrite(PIN_LED_BLUE, 0);
}

void loop() {
    uint32_t now = millis();

    // 1. Heartbeat генератор (900 мс)
    if (now - prev_heartbeat_time >= HEARTBEAT_INTERVAL_MS) {
        prev_heartbeat_time = now;
        heartbeat_state = !heartbeat_state;
        digitalWrite(PIN_HEARTBEAT, heartbeat_state ? LOW : HIGH);
    }

    // 2. Автоматичний перемикач калібрувальних фаз для Logic Analyzer
    if (current_phase == 0 && now >= 3000) {
        current_phase = 1;
        analogWrite(PIN_LED_RED, 128);
        Serial.println(F("[3s] Starting Phase 2: PWM = 128 (Duty: 50.2%) on PA6"));
        Serial.println(F("     Notice: Frequency is constant, HIGH pulse width doubles!"));
    }
    else if (current_phase == 1 && now >= 6000) {
        current_phase = 2;
        analogWrite(PIN_LED_RED, 191);
        Serial.println(F("[6s] Starting Phase 3: PWM = 191 (Duty: 74.9%) on PA6"));
        Serial.println(F("     Notice: Frequency is constant, HIGH pulse width is ~75% of period!"));
    }
    else if (current_phase == 2 && now >= 9000) {
        current_phase = 3;
        phase_start_time = now;
        Serial.println(F("[9s] Starting Phase 4: Mode AUTO (Smooth color blend 1900ms)"));
    }
    else if (current_phase == 3 && now >= 20000) {
        current_phase = 4;
        phase_start_time = now;
        Serial.println(F("[20s] Starting Phase 5: Mode ALARM (Red pulse 25-255)"));
    }

    if (current_phase == 3) {
        static uint32_t last_rgb_time = 0;
        if (now - last_rgb_time >= 30) {
            last_rgb_time = now;

            uint32_t t = (now - phase_start_time) % 5700;
            if (t < 1900) {
                uint8_t r = (t * 255) / 1900;
                analogWrite(PIN_LED_RED, r);
                analogWrite(PIN_LED_GREEN, 0);
                analogWrite(PIN_LED_BLUE, 255);
            } else if (t < 3800) {
                uint8_t g = ((t - 1900) * 255) / 1900;
                analogWrite(PIN_LED_RED, 255);
                analogWrite(PIN_LED_GREEN, g);
                analogWrite(PIN_LED_BLUE, 255);
            } else {
                uint8_t val = 255 - (((t - 3800) * 255) / 1900);
                analogWrite(PIN_LED_RED, val);
                analogWrite(PIN_LED_GREEN, val);
                analogWrite(PIN_LED_BLUE, 255);
            }
        }
    }
    else if (current_phase == 4) {
        static uint32_t last_alarm_time = 0;
        if (now - last_alarm_time >= 30) {
            last_alarm_time = now;
            uint32_t phase = (now - phase_start_time) % 1000;
            uint8_t r = (phase < 500) ? (25 + (230 * phase) / 500) : (255 - (230 * (phase - 500)) / 500);
            analogWrite(PIN_LED_RED, r);
            analogWrite(PIN_LED_GREEN, 0);
            analogWrite(PIN_LED_BLUE, 0);
        }
    }
}
