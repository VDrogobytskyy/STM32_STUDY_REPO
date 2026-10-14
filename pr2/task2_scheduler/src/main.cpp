#include <Arduino.h>

const uint8_t PIN_LED_RED   = PA6;
const uint8_t PIN_LED_GREEN = PA7;
const uint8_t PIN_LED_BLUE  = PB0;
const uint8_t PIN_BUTTON    = PB12;
const uint8_t PIN_HEARTBEAT = PC13;

const uint32_t DEBOUNCE_TIME_MS      = 30;
const uint32_t DOUBLE_CLICK_TIME_MS  = 450;
const uint32_t LONG_PRESS_TIME_MS    = 1500;

struct ScheduledTask {
    uint32_t interval_ms;
    uint32_t last_run_ms;
    uint32_t execution_count;
};

ScheduledTask task_heartbeat = { 900, 0, 0 };
ScheduledTask task_rgb       = {  30, 0, 0 };
ScheduledTask task_telemetry = { 1000, 0, 0 };
ScheduledTask task_button    = {  10, 0, 0 };

bool heartbeat_state = false;
uint8_t current_r = 255;
uint8_t current_g = 0;
uint8_t current_b = 255;

uint32_t loop_iteration_count = 0;
bool stats_10s_printed = false;

enum ButtonEvent {
    EVENT_NONE = 0,
    EVENT_SHORT_PRESS,
    EVENT_DOUBLE_PRESS,
    EVENT_LONG_PRESS
};

bool button_is_down = false;
uint32_t press_start_time = 0;
bool long_press_handled = false;
bool double_press_handled = false;
uint8_t pending_clicks = 0;
uint32_t last_release_time = 0;
uint32_t last_press_duration = 0;

ButtonEvent read_button_events(uint32_t now) {
    ButtonEvent event = EVENT_NONE;
    int pin_state = digitalRead(PIN_BUTTON);

    if (pin_state == LOW && !button_is_down) {
        button_is_down = true;
        press_start_time = now;
        long_press_handled = false;
        double_press_handled = false;

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

    analogWrite(PIN_LED_RED, current_r);
    analogWrite(PIN_LED_GREEN, current_g);
    analogWrite(PIN_LED_BLUE, current_b);

    Serial.println(F("=================================================="));
    Serial.println(F(" Practical 2 - Task 2: Non-blocking Task Scheduler"));
    Serial.println(F(" Variant: 7 (Heartbeat: 900ms, RGB step: 30ms)    "));
    Serial.println(F("=================================================="));
    Serial.println(F("[INFO] System started. 4 cooperative tasks running in loop()."));
    Serial.println(F("[INFO] RGB LED is active and smoothly changing color."));
}

void loop() {
    uint32_t current_time = millis();
    loop_iteration_count++;

    // 1. Задача Heartbeat (900 мс)
    if (current_time - task_heartbeat.last_run_ms >= task_heartbeat.interval_ms) {
        task_heartbeat.last_run_ms = current_time;
        task_heartbeat.execution_count++;

        heartbeat_state = !heartbeat_state;
        digitalWrite(PIN_HEARTBEAT, heartbeat_state ? LOW : HIGH);
    }

    // 2. Задача оновлення RGB (30 мс) - плавна зміна трьох каналів
    if (current_time - task_rgb.last_run_ms >= task_rgb.interval_ms) {
        task_rgb.last_run_ms = current_time;
        task_rgb.execution_count++;

        static uint16_t step_counter = 0;
        step_counter = (step_counter + 1) % 768;

        if (step_counter < 256) {
            current_r = 255 - step_counter;
            current_g = step_counter;
            current_b = 0;
        } else if (step_counter < 512) {
            current_r = 0;
            current_g = 255 - (step_counter - 256);
            current_b = step_counter - 256;
        } else {
            current_r = step_counter - 512;
            current_g = 0;
            current_b = 255 - (step_counter - 512);
        }

        analogWrite(PIN_LED_RED, current_r);
        analogWrite(PIN_LED_GREEN, current_g);
        analogWrite(PIN_LED_BLUE, current_b);
    }

    // 3. Задача обробки кнопки (подієво з розпізнаванням Short, Double, Long)
    if (current_time - task_button.last_run_ms >= task_button.interval_ms) {
        task_button.last_run_ms = current_time;
        task_button.execution_count++;

        ButtonEvent e = read_button_events(current_time);
        if (e != EVENT_NONE) {
            Serial.print(F("["));
            Serial.print(current_time);
            Serial.print(F(" ms] Button Event: "));
            if (e == EVENT_SHORT_PRESS) {
                Serial.print(F("SHORT PRESS | Duration: "));
                Serial.print(last_press_duration);
                Serial.println(F(" ms"));
            } else if (e == EVENT_DOUBLE_PRESS) {
                Serial.print(F("DOUBLE CLICK | Interval: "));
                Serial.print(last_press_duration);
                Serial.println(F(" ms"));
            } else if (e == EVENT_LONG_PRESS) {
                Serial.print(F("LONG PRESS (>1.5s) | Duration: "));
                Serial.print(last_press_duration);
                Serial.println(F(" ms"));
            }
        }
    }

    // 4. Задача телеметрії (1000 мс)
    if (current_time - task_telemetry.last_run_ms >= task_telemetry.interval_ms) {
        task_telemetry.last_run_ms = current_time;
        task_telemetry.execution_count++;

        Serial.print(F("[Telemetry | Uptime: "));
        Serial.print(current_time / 1000);
        Serial.print(F("s] HB(PC13): "));
        Serial.print(heartbeat_state ? "HIGH" : "LOW");
        Serial.print(F(" | RGB: ("));
        Serial.print(current_r);
        Serial.print(F(", "));
        Serial.print(current_g);
        Serial.print(F(", "));
        Serial.print(current_b);
        Serial.print(F(") | Duty R: "));
        Serial.print((current_r * 100) / 255);
        Serial.print(F("% G: "));
        Serial.print((current_g * 100) / 255);
        Serial.print(F("% B: "));
        Serial.print((current_b * 100) / 255);
        Serial.println(F("%"));
    }

    // 5. Одноразовий вивід статистики за 10 секунд симуляції
    if (current_time >= 10000 && !stats_10s_printed) {
        stats_10s_printed = true;

        Serial.println(F("\n=================================================="));
        Serial.println(F("       10-SECOND SIMULATION BENCHMARK STATS       "));
        Serial.println(F("=================================================="));
        Serial.print(F(" 1. Heartbeat Task (900 ms):   "));
        Serial.print(task_heartbeat.execution_count);
        Serial.println(F(" runs  (Theoretical: 11)"));

        Serial.print(F(" 2. RGB Update Task (30 ms):   "));
        Serial.print(task_rgb.execution_count);
        Serial.println(F(" runs (Theoretical: ~333)"));

        Serial.print(F(" 3. Telemetry Task (1000 ms):  "));
        Serial.print(task_telemetry.execution_count);
        Serial.println(F(" runs  (Theoretical: 10)"));

        Serial.print(F(" 4. Button Scan Task (10 ms):  "));
        Serial.print(task_button.execution_count);
        Serial.println(F(" runs (Theoretical: ~1000)"));

        Serial.print(F(" Total loop() passes:          "));
        Serial.print(loop_iteration_count);
        Serial.println(F(" cycles"));
        Serial.println(F(" Verification: Tasks run completely independently."));
        Serial.println(F("==================================================\n"));
    }
}
