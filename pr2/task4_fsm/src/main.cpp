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

enum SystemState {
    STATE_STOP = 0,
    STATE_AUTO,
    STATE_MANUAL,
    STATE_ALARM
};

enum ButtonEvent {
    EVENT_NONE = 0,
    EVENT_SHORT_PRESS,
    EVENT_DOUBLE_PRESS,
    EVENT_LONG_PRESS
};

SystemState current_state  = STATE_STOP;
SystemState previous_state = STATE_STOP;

struct ColorRGB {
    uint8_t r, g, b;
    const char* name;
};

const ColorRGB MANUAL_COLORS[] = {
    { 255,   0,   0, "RED"    },
    {   0, 255,   0, "GREEN"  },
    {   0,   0, 255, "BLUE"   },
    { 255, 255,   0, "YELLOW" },
    {   0,   0,   0, "OFF"    }
};
const uint8_t NUM_MANUAL_COLORS = sizeof(MANUAL_COLORS) / sizeof(MANUAL_COLORS[0]);
uint8_t manual_color_idx = 0;

volatile bool button_isr_triggered = false;
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

const char* state_to_string(SystemState s) {
    switch (s) {
        case STATE_STOP:   return "STOP";
        case STATE_AUTO:   return "AUTO";
        case STATE_MANUAL: return "MANUAL";
        case STATE_ALARM:  return "ALARM";
        default:           return "UNKNOWN";
    }
}

void on_state_enter(SystemState new_state) {
    switch (new_state) {
        case STATE_STOP:
            analogWrite(PIN_LED_RED, 0);
            analogWrite(PIN_LED_GREEN, 0);
            analogWrite(PIN_LED_BLUE, 0);
            break;

        case STATE_AUTO:
            analogWrite(PIN_LED_RED, 0);
            analogWrite(PIN_LED_GREEN, 0);
            analogWrite(PIN_LED_BLUE, 255);
            break;

        case STATE_MANUAL:
            analogWrite(PIN_LED_RED, MANUAL_COLORS[manual_color_idx].r);
            analogWrite(PIN_LED_GREEN, MANUAL_COLORS[manual_color_idx].g);
            analogWrite(PIN_LED_BLUE, MANUAL_COLORS[manual_color_idx].b);
            break;

        case STATE_ALARM:
            analogWrite(PIN_LED_GREEN, 0);
            analogWrite(PIN_LED_BLUE, 0);
            analogWrite(PIN_LED_RED, 255);
            break;
    }
}

bool transition_to_state(SystemState target, const char* trigger_reason) {
    if (target == current_state) return false;

    bool transition_allowed = false;

    switch (current_state) {
        case STATE_STOP:
            if (target == STATE_AUTO || target == STATE_ALARM) {
                transition_allowed = true;
            }
            break;

        case STATE_AUTO:
            if (target == STATE_MANUAL || target == STATE_ALARM) {
                transition_allowed = true;
            }
            break;

        case STATE_MANUAL:
            if (target == STATE_STOP || target == STATE_ALARM) {
                transition_allowed = true;
            }
            break;

        case STATE_ALARM:
            if (target == previous_state) {
                transition_allowed = true;
            }
            break;
    }

    if (transition_allowed) {
        uint32_t now = millis();
        Serial.print(F("["));
        Serial.print(now);
        Serial.print(F(" ms] FSM Transition: "));
        Serial.print(state_to_string(current_state));
        Serial.print(F(" -> "));
        Serial.print(state_to_string(target));
        Serial.print(F(" | Trigger: "));
        Serial.println(trigger_reason);

        if (target == STATE_ALARM) {
            previous_state = current_state;
        }

        current_state = target;
        on_state_enter(current_state);
        return true;
    } else {
        Serial.print(F("[REJECTED] Illegal transition attempt from "));
        Serial.print(state_to_string(current_state));
        Serial.print(F(" to "));
        Serial.print(state_to_string(target));
        Serial.print(F(" via trigger: "));
        Serial.println(trigger_reason);
        return false;
    }
}

ButtonEvent read_button_events(uint32_t now) {
    ButtonEvent event = EVENT_NONE;
    int pin_state = digitalRead(PIN_BUTTON);

    if (pin_state == LOW && !button_is_down) {
        button_is_down = true;
        press_start_time = now;
        long_press_handled = false;
        double_press_handled = false;
        button_isr_triggered = false;

        // Детекція другого кліку в межах вікна DOUBLE_CLICK_TIME_MS
        if (pending_clicks == 1 && (now - last_release_time <= DOUBLE_CLICK_TIME_MS)) {
            event = EVENT_DOUBLE_PRESS;
            pending_clicks = 0;
            double_press_handled = true;
        }
    } 
    else if (pin_state == LOW && button_is_down) {
        if (!long_press_handled && (now - press_start_time >= LONG_PRESS_TIME_MS)) {
            long_press_handled = true;
            pending_clicks = 0;
            last_press_duration = now - press_start_time;
            event = EVENT_LONG_PRESS;
        }
    } 
    else if (pin_state == HIGH && button_is_down) {
        uint32_t press_duration = now - press_start_time;
        button_is_down = false;
        last_press_duration = press_duration;

        if (press_duration >= DEBOUNCE_TIME_MS && !long_press_handled && !double_press_handled) {
            pending_clicks = 1;
            last_release_time = now;
        }
    }

    if (pending_clicks == 1 && !button_is_down && (now - last_release_time > DOUBLE_CLICK_TIME_MS)) {
        event = EVENT_SHORT_PRESS;
        pending_clicks = 0;
    }

    return event;
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

    on_state_enter(STATE_STOP);

    Serial.println(F("=================================================="));
    Serial.println(F(" Practical 2 - Task 4: Finite State Machine (FSM) "));
    Serial.println(F(" Variant: 7 (States: STOP, AUTO, MANUAL, ALARM)   "));
    Serial.println(F("=================================================="));
    Serial.println(F("[0 ms] Initial State: STOP (RGB Off)"));
}

void loop() {
    uint32_t current_time = millis();

    // 1. Heartbeat незалежно від режиму
    if (current_time - prev_heartbeat_time >= HEARTBEAT_INTERVAL_MS) {
        prev_heartbeat_time = current_time;
        heartbeat_state = !heartbeat_state;
        digitalWrite(PIN_HEARTBEAT, heartbeat_state ? LOW : HIGH);
    }

    // 2. Опитування подій кнопки
    ButtonEvent event = read_button_events(current_time);

    if (event != EVENT_NONE) {
        switch (event) {
            case EVENT_SHORT_PRESS:
                if (current_state == STATE_STOP) {
                    transition_to_state(STATE_AUTO, "Short Click (STOP -> AUTO)");
                } else if (current_state == STATE_AUTO) {
                    transition_to_state(STATE_MANUAL, "Short Click (AUTO -> MANUAL)");
                } else if (current_state == STATE_MANUAL) {
                    manual_color_idx = (manual_color_idx + 1) % NUM_MANUAL_COLORS;
                    on_state_enter(STATE_MANUAL);
                    Serial.print(F("["));
                    Serial.print(current_time);
                    Serial.print(F(" ms] MANUAL Mode: Color changed to "));
                    Serial.println(MANUAL_COLORS[manual_color_idx].name);
                }
                break;

            case EVENT_DOUBLE_PRESS:
                Serial.print(F("["));
                Serial.print(current_time);
                Serial.print(F(" ms] Event: DOUBLE CLICK"));
                if (current_state == STATE_MANUAL) {
                    Serial.println();
                    transition_to_state(STATE_STOP, "Double Click (MANUAL -> STOP)");
                } else {
                    Serial.print(F(" | State: "));
                    Serial.print(state_to_string(current_state));
                    Serial.println(F(" -> Note: Double click triggers MANUAL -> STOP per FSM table. Click Short 2x to reach MANUAL first."));
                }
                break;

            case EVENT_LONG_PRESS:
                if (current_state != STATE_ALARM) {
                    transition_to_state(STATE_ALARM, "Long Press (Enter ALARM)");
                } else {
                    transition_to_state(previous_state, "Long Press (Exit ALARM to previous)");
                }
                break;

            default:
                break;
        }
    }
}
