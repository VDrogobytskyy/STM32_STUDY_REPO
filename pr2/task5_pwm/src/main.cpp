#include <Arduino.h>

const uint8_t PIN_LED_RED   = PA6;
const uint8_t PIN_LED_GREEN = PA7;
const uint8_t PIN_LED_BLUE  = PB0;
const uint8_t PIN_BUTTON    = PB12;
const uint8_t PIN_HEARTBEAT = PC13;

const uint32_t HEARTBEAT_INTERVAL_MS = 900;
const uint32_t RGB_STEP_INTERVAL_MS  = 30;
const uint32_t FADE_DURATION_MS      = 1900;

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

struct ColorRGB {
    uint8_t r, g, b;
};

const ColorRGB AUTO_KEY_COLORS[] = {
    {   0,   0, 255 },
    { 255,   0, 255 },
    { 255, 255, 255 }
};
const uint8_t NUM_AUTO_COLORS = sizeof(AUTO_KEY_COLORS) / sizeof(AUTO_KEY_COLORS[0]);

struct NamedColor {
    ColorRGB color;
    const char* name;
};

const NamedColor MANUAL_COLORS[] = {
    { { 255,   0,   0 }, "RED"    },
    { {   0, 255,   0 }, "GREEN"  },
    { {   0,   0, 255 }, "BLUE"   },
    { { 255, 255,   0 }, "YELLOW" },
    { {   0,   0,   0 }, "OFF"    }
};
const uint8_t NUM_MANUAL_COLORS = sizeof(MANUAL_COLORS) / sizeof(MANUAL_COLORS[0]);
uint8_t manual_color_idx = 0;

SystemState current_state  = STATE_STOP;
SystemState previous_state = STATE_STOP;

uint8_t current_r = 0;
uint8_t current_g = 0;
uint8_t current_b = 0;

ColorRGB fade_start_color = { 0, 0, 0 };
ColorRGB fade_target_color = { 0, 0, 255 };
uint8_t auto_target_idx = 0;
uint32_t fade_start_time = 0;

uint32_t alarm_start_time = 0;
const uint8_t ALARM_MIN_R = 25;
const uint8_t ALARM_MAX_R = 255;
const uint32_t ALARM_PULSE_PERIOD_MS = 1000;

uint32_t prev_heartbeat_time = 0;
uint32_t prev_rgb_step_time  = 0;
uint32_t prev_telemetry_time = 0;
bool heartbeat_state = false;

volatile bool button_isr_triggered = false;
bool button_is_down = false;
uint32_t press_start_time = 0;
bool long_press_handled = false;
bool double_press_handled = false;
uint8_t pending_clicks = 0;
uint32_t last_release_time = 0;
uint32_t last_press_duration = 0;

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

void write_pwm(uint8_t r, uint8_t g, uint8_t b) {
    current_r = r;
    current_g = g;
    current_b = b;

    analogWrite(PIN_LED_RED,   current_r);
    analogWrite(PIN_LED_GREEN, current_g);
    analogWrite(PIN_LED_BLUE,  current_b);
}

void on_state_enter(SystemState new_state) {
    uint32_t now = millis();

    switch (new_state) {
        case STATE_STOP:
            write_pwm(0, 0, 0);
            break;

        case STATE_AUTO:
            fade_start_color = { current_r, current_g, current_b };
            auto_target_idx = 0;
            fade_target_color = AUTO_KEY_COLORS[auto_target_idx];
            fade_start_time = now;
            break;

        case STATE_MANUAL:
            write_pwm(MANUAL_COLORS[manual_color_idx].color.r,
                      MANUAL_COLORS[manual_color_idx].color.g,
                      MANUAL_COLORS[manual_color_idx].color.b);
            break;

        case STATE_ALARM:
            alarm_start_time = now;
            write_pwm(current_r, 0, 0);
            break;
    }
}

bool transition_to_state(SystemState target, const char* reason) {
    if (target == current_state) return false;

    bool allowed = false;
    switch (current_state) {
        case STATE_STOP:
            if (target == STATE_AUTO || target == STATE_ALARM) allowed = true;
            break;
        case STATE_AUTO:
            if (target == STATE_MANUAL || target == STATE_ALARM) allowed = true;
            break;
        case STATE_MANUAL:
            if (target == STATE_STOP || target == STATE_ALARM) allowed = true;
            break;
        case STATE_ALARM:
            if (target == previous_state) allowed = true;
            break;
    }

    if (allowed) {
        Serial.print(F("["));
        Serial.print(millis());
        Serial.print(F(" ms] Transition: "));
        Serial.print(state_to_string(current_state));
        Serial.print(F(" -> "));
        Serial.print(state_to_string(target));
        Serial.print(F(" | Reason: "));
        Serial.println(reason);

        if (target == STATE_ALARM) {
            previous_state = current_state;
        }

        current_state = target;
        on_state_enter(current_state);
        return true;
    }
    return false;
}

void update_rgb_task(uint32_t now) {
    if (current_state == STATE_STOP) {
        write_pwm(0, 0, 0);
        return;
    }

    if (current_state == STATE_AUTO) {
        uint32_t elapsed = now - fade_start_time;

        if (elapsed >= FADE_DURATION_MS) {
            fade_start_color = fade_target_color;
            auto_target_idx = (auto_target_idx + 1) % NUM_AUTO_COLORS;
            fade_target_color = AUTO_KEY_COLORS[auto_target_idx];
            fade_start_time = now;
            elapsed = 0;
        }

        int32_t r = fade_start_color.r + (((int32_t)fade_target_color.r - fade_start_color.r) * (int32_t)elapsed) / (int32_t)FADE_DURATION_MS;
        int32_t g = fade_start_color.g + (((int32_t)fade_target_color.g - fade_start_color.g) * (int32_t)elapsed) / (int32_t)FADE_DURATION_MS;
        int32_t b = fade_start_color.b + (((int32_t)fade_target_color.b - fade_start_color.b) * (int32_t)elapsed) / (int32_t)FADE_DURATION_MS;

        if (r < 0) r = 0; else if (r > 255) r = 255;
        if (g < 0) g = 0; else if (g > 255) g = 255;
        if (b < 0) b = 0; else if (b > 255) b = 255;

        write_pwm((uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
    else if (current_state == STATE_ALARM) {
        uint32_t phase = (now - alarm_start_time) % ALARM_PULSE_PERIOD_MS;
        uint32_t half_period = ALARM_PULSE_PERIOD_MS / 2;
        int32_t r = 0;

        if (phase < half_period) {
            r = ALARM_MIN_R + ((int32_t)(ALARM_MAX_R - ALARM_MIN_R) * phase) / half_period;
        } else {
            r = ALARM_MAX_R - ((int32_t)(ALARM_MAX_R - ALARM_MIN_R) * (phase - half_period)) / half_period;
        }

        if (r < ALARM_MIN_R) r = ALARM_MIN_R;
        else if (r > ALARM_MAX_R) r = ALARM_MAX_R;

        write_pwm((uint8_t)r, 0, 0);
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

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), button_exti_isr, FALLING);

    on_state_enter(STATE_STOP);

    Serial.println(F("=================================================="));
    Serial.println(F(" Practical 2 - Task 5: Three-Channel PWM & Color  "));
    Serial.println(F(" Variant: 7 (Blue -> Purple -> White, Alarm: 25-255)"));
    Serial.println(F("=================================================="));
    Serial.println(F("[0 ms] System started in STOP mode. Click button to enter AUTO."));
}

void loop() {
    uint32_t current_time = millis();

    // 1. Heartbeat задача (900 мс)
    if (current_time - prev_heartbeat_time >= HEARTBEAT_INTERVAL_MS) {
        prev_heartbeat_time = current_time;
        heartbeat_state = !heartbeat_state;
        digitalWrite(PIN_HEARTBEAT, heartbeat_state ? LOW : HIGH);
    }

    // 2. Оновлення RGB PWM (30 мс)
    if (current_time - prev_rgb_step_time >= RGB_STEP_INTERVAL_MS) {
        prev_rgb_step_time = current_time;
        update_rgb_task(current_time);
    }

    // 3. Обробка кнопки
    ButtonEvent event = read_button_events(current_time);
    if (event != EVENT_NONE) {
        switch (event) {
            case EVENT_SHORT_PRESS:
                if (current_state == STATE_STOP) {
                    transition_to_state(STATE_AUTO, "Short Click");
                } else if (current_state == STATE_AUTO) {
                    transition_to_state(STATE_MANUAL, "Short Click");
                } else if (current_state == STATE_MANUAL) {
                    manual_color_idx = (manual_color_idx + 1) % NUM_MANUAL_COLORS;
                    on_state_enter(STATE_MANUAL);
                    Serial.print(F("["));
                    Serial.print(current_time);
                    Serial.print(F(" ms] MANUAL Color: "));
                    Serial.println(MANUAL_COLORS[manual_color_idx].name);
                }
                break;

            case EVENT_DOUBLE_PRESS:
                Serial.print(F("["));
                Serial.print(current_time);
                Serial.print(F(" ms] Event: DOUBLE CLICK"));
                if (current_state == STATE_MANUAL) {
                    Serial.println();
                    transition_to_state(STATE_STOP, "Double Click");
                } else {
                    Serial.print(F(" | State: "));
                    Serial.print(state_to_string(current_state));
                    Serial.println(F(" -> Note: Double click triggers MANUAL -> STOP per FSM table. Click Short 2x to reach MANUAL first."));
                }
                break;

            case EVENT_LONG_PRESS:
                if (current_state != STATE_ALARM) {
                    transition_to_state(STATE_ALARM, "Long Press");
                } else {
                    transition_to_state(previous_state, "Long Press");
                }
                break;

            default:
                break;
        }
    }

    // 4. Телеметрія (раз на 1000 мс)
    if (current_time - prev_telemetry_time >= 1000) {
        prev_telemetry_time = current_time;
        Serial.print(F("[Telemetry] Mode: "));
        Serial.print(state_to_string(current_state));
        Serial.print(F(" | R: "));
        Serial.print(current_r);
        Serial.print(F(" ("));
        Serial.print((current_r * 100) / 255);
        Serial.print(F("%), G: "));
        Serial.print(current_g);
        Serial.print(F(" ("));
        Serial.print((current_g * 100) / 255);
        Serial.print(F("%), B: "));
        Serial.print(current_b);
        Serial.print(F(" ("));
        Serial.print((current_b * 100) / 255);
        Serial.println(F("%)"));
    }
}
