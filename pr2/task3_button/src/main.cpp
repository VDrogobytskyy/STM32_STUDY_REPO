#include <Arduino.h>

const uint8_t PIN_LED_RED   = PA6;
const uint8_t PIN_LED_GREEN = PA7;
const uint8_t PIN_LED_BLUE  = PB0;
const uint8_t PIN_BUTTON    = PB12;
const uint8_t PIN_HEARTBEAT = PC13;

const uint32_t HEARTBEAT_INTERVAL_MS = 900;
const uint32_t DEBOUNCE_TIME_MS      = 30;
const uint32_t DOUBLE_CLICK_TIME_MS  = 450;
const uint32_t LONG_PRESS_TIME_MS    = 1500;

volatile bool button_isr_triggered = false;

enum ButtonEvent {
    EVENT_NONE = 0,
    EVENT_SHORT_PRESS,
    EVENT_DOUBLE_PRESS,
    EVENT_LONG_PRESS
};

enum DemoMode {
    MODE_STOP = 0,
    MODE_AUTO,
    MODE_MANUAL,
    MODE_ALARM
};

DemoMode current_mode = MODE_STOP;
DemoMode prev_mode    = MODE_STOP;

bool button_is_down = false;
uint32_t press_start_time = 0;
bool long_press_handled = false;
bool double_press_handled = false;
uint8_t pending_clicks = 0;
uint32_t last_release_time = 0;
uint32_t last_press_duration = 0;

uint32_t prev_heartbeat_time = 0;
bool heartbeat_state = false;

void button_exti_isr() {
    button_isr_triggered = true;
}

ButtonEvent process_button(uint32_t now) {
    ButtonEvent detected_event = EVENT_NONE;
    int pin_state = digitalRead(PIN_BUTTON); // LOW = натиснуто

    // 1. Подія натискання кнопки вниз (Down)
    if (pin_state == LOW && !button_is_down) {
        button_is_down = true;
        press_start_time = now;
        long_press_handled = false;
        double_press_handled = false;
        button_isr_triggered = false;

        // Якщо це друге натискання у вікні подвійного кліку
        if (pending_clicks == 1 && (now - last_release_time <= DOUBLE_CLICK_TIME_MS)) {
            detected_event = EVENT_DOUBLE_PRESS;
            pending_clicks = 0;
            double_press_handled = true;
        }
    } 
    // 2. Кнопка все ще утримується (Held)
    else if (pin_state == LOW && button_is_down) {
        if (!long_press_handled && (now - press_start_time >= LONG_PRESS_TIME_MS)) {
            long_press_handled = true;
            pending_clicks = 0;
            last_press_duration = now - press_start_time;
            detected_event = EVENT_LONG_PRESS;
        }
    } 
    // 3. Відпускання кнопки (Release)
    else if (pin_state == HIGH && button_is_down) {
        uint32_t press_duration = now - press_start_time;
        button_is_down = false;
        last_press_duration = press_duration;

        if (press_duration >= DEBOUNCE_TIME_MS && !long_press_handled && !double_press_handled) {
            pending_clicks = 1;
            last_release_time = now;
        }
    }

    // 4. Сплив таймаут очікування другого кліку -> одинарний короткий клік
    if (pending_clicks == 1 && !button_is_down && (now - last_release_time > DOUBLE_CLICK_TIME_MS)) {
        detected_event = EVENT_SHORT_PRESS;
        pending_clicks = 0;
    }

    return detected_event;
}

const char* get_mode_name(DemoMode m) {
    switch (m) {
        case MODE_STOP:   return "STOP";
        case MODE_AUTO:   return "AUTO";
        case MODE_MANUAL: return "MANUAL";
        case MODE_ALARM:  return "ALARM";
        default:          return "UNKNOWN";
    }
}

void setup() {
    Serial.begin(115200);
    delay(50);

    pinMode(PIN_HEARTBEAT, OUTPUT);
    digitalWrite(PIN_HEARTBEAT, HIGH);

    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), button_exti_isr, FALLING);

    Serial.println(F("=================================================="));
    Serial.println(F(" Practical 2 - Task 3: Button Interrupt & Debounce"));
    Serial.println(F(" Variant: 7 (Button PB12, EXTI, Debounce: 50ms)   "));
    Serial.println(F("=================================================="));
    Serial.println(F("[INFO] PB12 EXTI Interrupt registered successfully."));
    Serial.println(F("[INFO] Supported gestures: Short Click, Double Click, Long Press (>1.5s)."));
    Serial.print(F("[State: "));
    Serial.print(get_mode_name(current_mode));
    Serial.println(F("]"));
}

void loop() {
    uint32_t current_time = millis();

    // 1. Фонова задача Heartbeat (900 мс)
    if (current_time - prev_heartbeat_time >= HEARTBEAT_INTERVAL_MS) {
        prev_heartbeat_time = current_time;
        heartbeat_state = !heartbeat_state;
        digitalWrite(PIN_HEARTBEAT, heartbeat_state ? LOW : HIGH);
    }

    // 2. Обробка кнопки
    ButtonEvent event = process_button(current_time);

    if (event != EVENT_NONE) {
        Serial.print(F("["));
        Serial.print(current_time);
        Serial.print(F(" ms] Recognized Event: "));

        DemoMode old_mode = current_mode;

        switch (event) {
            case EVENT_SHORT_PRESS:
                Serial.print(F("SHORT PRESS (<1.5s) | Duration: "));
                Serial.print(last_press_duration);
                Serial.print(F(" ms"));
                if (current_mode == MODE_STOP) {
                    current_mode = MODE_AUTO;
                } else if (current_mode == MODE_AUTO) {
                    current_mode = MODE_MANUAL;
                } else if (current_mode == MODE_MANUAL) {
                    Serial.print(F(" -> Action: Next Manual Color"));
                }
                break;

            case EVENT_DOUBLE_PRESS:
                Serial.print(F("DOUBLE CLICK | Gap/Duration: "));
                Serial.print(last_press_duration);
                Serial.print(F(" ms"));
                if (current_mode == MODE_MANUAL) {
                    current_mode = MODE_STOP;
                } else {
                    current_mode = MODE_STOP;
                    Serial.print(F(" -> Action: Reset to STOP"));
                }
                break;

            case EVENT_LONG_PRESS:
                Serial.print(F("LONG PRESS (>1.5s) | Duration: "));
                Serial.print(last_press_duration);
                Serial.print(F(" ms"));
                if (current_mode != MODE_ALARM) {
                    prev_mode = current_mode;
                    current_mode = MODE_ALARM;
                } else {
                    current_mode = prev_mode;
                }
                break;

            default:
                break;
        }

        if (old_mode != current_mode) {
            Serial.print(F(" | State Transition: "));
            Serial.print(get_mode_name(old_mode));
            Serial.print(F(" -> "));
            Serial.println(get_mode_name(current_mode));
        } else {
            Serial.println();
        }
    }
}
