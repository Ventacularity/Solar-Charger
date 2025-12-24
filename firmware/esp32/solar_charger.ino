#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ----------------------------------------------------------
// PIN ASSIGNMENTS 
// ----------------------------------------------------------
#define PIN_PGOOD     15   // BQ24075 PGOOD (input detected)
#define PIN_CHG       16   // BQ24075 CHG (active LOW when charging)
#define PIN_CE        14   // BQ24075 CE (LOW = charging enabled)
#define PIN_EN1       12   // BQ24075 EN1
#define PIN_EN2       13   // BQ24075 EN2

// ADC pins
#define PIN_ADC_BAT    4   // Battery voltage sense via R13=100k / R14=220k
#define PIN_ADC_VBUS  17   // 5V output sense via R18=100k / R17=220k

// OLED I2C pins
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22

// ----------------------------------------------------------
// DIVIDERS 
// ----------------------------------------------------------

// Battery divider R13 = 100k (top), R14 = 220k (bottom)
const float R_BAT_TOP    = 100000.0f;
const float R_BAT_BOTTOM = 220000.0f;

// 5V sense divider R18 = 100k (top), R17 = 220k (bottom)
const float R_VBUS_TOP    = 100000.0f;
const float R_VBUS_BOTTOM = 220000.0f;

// ADC configuration
const float ADC_REF = 3.3f;
const int   ADC_RES = 4095;

// Li-ion battery voltage range
const float V_BAT_MIN = 3.0f;
const float V_BAT_MAX = 4.2f;

// 5V rail detection thresholds
const float VBUS_PRESENT_MIN = 4.2f;  // 5V rail alive
const float VBUS_GOOD_THRESH = 4.8f;  // no significant load


// ----------------------------------------------------------
// HELPER FUNCTIONS
// ----------------------------------------------------------
float readDividerVoltage(int pin, float Rtop, float Rbottom) {
    int raw = analogRead(pin);
    float v_adc = (raw * ADC_REF) / ADC_RES;
    float ratio = (Rtop + Rbottom) / Rbottom;
    return v_adc * ratio;
}

int batteryPercent(float vbat) {
    if (vbat <= V_BAT_MIN) return 0;
    if (vbat >= V_BAT_MAX) return 100;
    return (int)(((vbat - V_BAT_MIN) / (V_BAT_MAX - V_BAT_MIN)) * 100.0f);
}


// ----------------------------------------------------------
// SETUP
// ----------------------------------------------------------
void setup() {
    Serial.begin(115200);

    pinMode(PIN_PGOOD, INPUT);
    pinMode(PIN_CHG,   INPUT);

    pinMode(PIN_CE,  OUTPUT);
    pinMode(PIN_EN1, OUTPUT);
    pinMode(PIN_EN2, OUTPUT);

    // Default charger behavior: charging enabled, USB-charge mode
    digitalWrite(PIN_CE,  LOW);
    digitalWrite(PIN_EN1, LOW);
    digitalWrite(PIN_EN2, LOW);

    // Configure ADC input attenuation
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_ADC_BAT,  ADC_11db);
    analogSetPinAttenuation(PIN_ADC_VBUS, ADC_11db);

    // Initialize OLED
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for (;;) {}
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("HOPE Solar Charger");
    display.println("Booting...");
    display.display();
    delay(1200);
}


// ----------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------
void loop() {

    // -------- Read voltages ----------
    float vbat = readDividerVoltage(PIN_ADC_BAT, R_BAT_TOP, R_BAT_BOTTOM);
    float vbus = readDividerVoltage(PIN_ADC_VBUS, R_VBUS_TOP, R_VBUS_BOTTOM);

    int pct = batteryPercent(vbat);

    // BQ24075 digital signals
    bool solar = digitalRead(PIN_PGOOD);    // input present
    bool chg   = (digitalRead(PIN_CHG) == LOW);  // charging active
    bool full  = (pct >= 98);

    // Auto CE disable on full battery
    digitalWrite(PIN_CE, full ? HIGH : LOW);

    // 5V rail / device load inference
    bool vbusPresent = (vbus >= VBUS_PRESENT_MIN);
    bool vbusStrong  = (vbus >= VBUS_GOOD_THRESH);

    bool deviceCharging = false;
    const char *vbusStatus;

    if (!vbusPresent) {
        vbusStatus = "Off";
        deviceCharging = false;
    } else if (vbusStrong) {
        vbusStatus = "OK";
        deviceCharging = false;  // nearly no load
    } else {
        vbusStatus = "Low";      // sagging → load
        deviceCharging = true;
    }

    // --------- Serial Debug ---------
    Serial.print("Vbat=");
    Serial.print(vbat);
    Serial.print("  (");
    Serial.print(pct);
    Serial.print("%)   Vbus=");
    Serial.print(vbus);
    Serial.print("   Device=");
    Serial.println(deviceCharging ? "CHARGING" : "none");


    // ----------------------------------------------------------
    // OLED OUTPUT 
    // ----------------------------------------------------------
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);

    display.print("Bat: ");
    display.print(vbat, 2);
    display.print("V  ");
    display.print(pct);
    display.println("%");

    display.print("Input: ");
    display.println(solar ? "Solar/USB" : "None");

    display.print("Charge: ");
    if (full) display.println("FULL");
    else if (chg) display.println("Charging");
    else display.println("Idle");

    display.print("ChgEn: ");
    display.println(full ? "OFF" : "ON");

    display.print("5V: ");
    display.println(vbusStatus);

    display.print("Device: ");
    display.println(deviceCharging ? "Charging" : "Not Detected");

    display.display();

    delay(1000);
}