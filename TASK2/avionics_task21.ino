// NAME: AARAV DEVARSHI OZA
// ID: 2026B5PS0910H

#include <Adafruit_LiquidCrystal.h>

// Pins
const int button_pin = 3;
const int led_pin    = 4;
const int buzz_pin   = 5;
const int echo_pin   = 6;
const int trig_pin   = 7;
const int photo_pin  = A0;

// Sensor data
int distance   = 999;
int light_value = 512;

// Light averaging (rolling 5 samples)
int light_readings[5] = {512, 512, 512, 512, 512};  // start neutral
int light_index = 0;

bool button_state  = false;
bool button_toggle = false;   // true = anchor dropped
bool led_state     = false;
bool buzzer_state  = false;

// LCD (change 0x20 if your Tinkercad LCD uses a different address)
Adafruit_LiquidCrystal lcd(0x20);

// States
enum State {
    OPEN_SEA,
    ANCHOR_DROPPED,
    WRECKED,
    STORM,
    CHARYBDIS
};

State current_state = OPEN_SEA;
State prev_state    = WRECKED;   // force first LCD update

// Thresholds (brief)
const int distance_threshold = 100;   // cm  – below = Charybdis
const int light_threshold    = 512;   // below half of 0-1023 = Storm

// Timing
const unsigned long DANGER_TIME_MS = 5000;
const unsigned long BLINK_MS       = 300;
const unsigned long BUZZ_MS        = 300;

unsigned long danger_start_time = 0;
bool run_warning_light = false;
bool run_warning_sound = false;

void setup() {
    pinMode(echo_pin,   INPUT);
    pinMode(trig_pin,   OUTPUT);
    pinMode(buzz_pin,   OUTPUT);
    pinMode(led_pin,    OUTPUT);
    pinMode(button_pin, INPUT);   // external pull-down

    Serial.begin(9600);
    lcd.begin(16, 2);
    lcd.clear();
    update_lcd();                 // show initial state
}

void loop() {
    get_ultrasonic_data();
    get_light_data();
    Serial.println(light_value);
    get_button_state();

    // ---------- State machine ----------
    if (current_state == OPEN_SEA) {
        bool in_storm   = (light_value <= light_threshold);
        bool in_charyb  = (distance  <= distance_threshold);

        if (in_charyb && !in_storm) {
            current_state = CHARYBDIS;
            danger_start_time = millis();
        } else if (in_storm && !in_charyb) {
            current_state = STORM;
            danger_start_time = millis();
        } else if (in_storm && in_charyb) {
            // Both true at the same instant → pick one (STORM here)
            // Once chosen, the later switch logic keeps the first timer
            current_state = STORM;
            danger_start_time = millis();
        }
    }

    // 5-second continuous danger → WRECKED
    if ((current_state == STORM || current_state == CHARYBDIS) &&
        (millis() - danger_start_time >= DANGER_TIME_MS)) {
        current_state = WRECKED;
    }

    // Anchor drop / raise (protected from all danger while dropped)
    if (current_state != WRECKED && button_toggle) {
        current_state = ANCHOR_DROPPED;
    }
    if (current_state == ANCHOR_DROPPED && !button_toggle) {
        current_state = OPEN_SEA;   // back to OPEN; sensors will re-trigger & reset timer if still dangerous
    }

    // While still inside the 5 s window, allow switching between STORM ↔ CHARYBDIS
    // or return to OPEN SEA. Timer is NOT reset (matches “first one keeps its timer”).
    if (millis() - danger_start_time < DANGER_TIME_MS) {
        if (current_state == CHARYBDIS &&
            distance > distance_threshold &&
            light_value <= light_threshold) {
            current_state = STORM;
        }
        if (current_state == STORM &&
            distance <= distance_threshold &&
            light_value > light_threshold) {
            current_state = CHARYBDIS;
        }
        if (distance > distance_threshold &&
            light_value > light_threshold &&
            current_state != ANCHOR_DROPPED &&
            current_state != WRECKED) {
            current_state = OPEN_SEA;
        }
    }

    // Outputs
    run_warning_light = (current_state == STORM);
    run_warning_sound = (current_state == CHARYBDIS);

    handle_led();
    handle_buzzer();

    if (prev_state != current_state) {
        update_lcd();
        prev_state = current_state;
    }

    delay(10);
}

// ---------- Helpers ----------
void update_lcd() {
    lcd.setCursor(0, 0);
    switch (current_state) {
        case OPEN_SEA:       lcd.print("    OPEN SEA    "); break;
        case ANCHOR_DROPPED: lcd.print(" ANCHOR DROPPED "); break;
        case WRECKED:        lcd.print("    WRECKED     "); break;
        case STORM:          lcd.print("     STORM      "); break;
        case CHARYBDIS:      lcd.print("   CHARYBDIS    "); break;
    }
    lcd.setCursor(0, 1);
    lcd.print("                ");   // clear second line
}

unsigned long prev_millis_ultrasonic = 0;
void get_ultrasonic_data() {
    unsigned long now = millis();
    if (now - prev_millis_ultrasonic < 100) return;
    prev_millis_ultrasonic = now;

    digitalWrite(trig_pin, LOW);
    delayMicroseconds(2);
    digitalWrite(trig_pin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig_pin, LOW);

    long duration = pulseIn(echo_pin, HIGH, 30000);  // 30 ms timeout
    if (duration == 0) {
        distance = 999;          // no echo = far away
    } else {
        distance = duration * 0.034 / 2;
    }
}

void get_light_data() {
    light_readings[light_index] = analogRead(photo_pin);
    light_index = (light_index + 1) % 5;

    long sum = 0;
    for (int i = 0; i < 5; i++) sum += light_readings[i];
    light_value = sum / 5;
}

void get_button_state() {
    bool prev = button_state;
    button_state = digitalRead(button_pin);
    // rising edge → toggle anchor
    if (!prev && button_state) {
        button_toggle = !button_toggle;
    }
}

unsigned long prev_millis_led = 0;
void handle_led() {
    if (run_warning_light) {
        unsigned long now = millis();
        if (now - prev_millis_led >= BLINK_MS) {
            prev_millis_led = now;
            led_state = !led_state;
            digitalWrite(led_pin, led_state ? HIGH : LOW);
        }
    } else {
        digitalWrite(led_pin, LOW);
        led_state = false;
    }
}

unsigned long prev_millis_buzzer = 0;
void handle_buzzer() {
    if (run_warning_sound) {
        unsigned long now = millis();
        if (now - prev_millis_buzzer >= BUZZ_MS) {
            prev_millis_buzzer = now;
            buzzer_state = !buzzer_state;
            if (buzzer_state) {
                tone(buzz_pin, 1000);
            } else {
                noTone(buzz_pin);
            }
        }
    } else {
        noTone(buzz_pin);
        buzzer_state = false;
    }
}