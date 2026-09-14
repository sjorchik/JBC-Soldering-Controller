/* Copyright (c) 2017 timothyjager — MIT License. */
//----------------Pin Mapping (STM32F103C8T6 / Blue Pill)-------------------------
const int HEATER_PWM_PIN  = PA0;  // TIM2_CH1 - апаратний ШІМ (НЕ міняти без ремапу)
const int TFT_BACKLIGHT   = PA1;
const int ADS_CS_PIN      = PA4;
const int SPI_SCLK        = PA5;
const int SPI_MISO        = PA6;
const int SPI_MOSI        = PA7;
const int I2C_SCL         = PB6;
const int I2C_SDA         = PB7;
const int WS2812_DATA     = PB0;
const int CRADLE_SENSOR   = PB12;
const int VN5E_CS         = PB1;
#define VN5E_RSENSE_OHMS   1500.0f
#define VN5E_K_TYPICAL     6740.0f
const int ENC_A           = PB3;
const int ENC_B           = PB4;
const int ENC_BUTTON      = PB5;
const int TFT_CS          = PA8;
const int TFT_DC          = PA9;
const int TFT_RST         = PA10;
//----------------ШІМ нагрівача / вікно вибірки АЦП--------
#define PWM_PERIOD_US            20000UL
#define PWM_PERIOD_MS            (PWM_PERIOD_US / 1000UL)
// 1250 мкс: конверсія 860SPS ≈ 1.16 мс + запас;
#define ADC_SAMPLE_WINDOW_US     1250UL
#define MAX_HEATER_DUTY_TICKS    (PWM_PERIOD_US - ADC_SAMPLE_WINDOW_US)
#define MAX_HEATER_DUTY_PERCENT   70  // Обмеження потужності нагріву (%)
#define MAX_HEATER_DUTY_LIMITED   ((MAX_HEATER_DUTY_TICKS * MAX_HEATER_DUTY_PERCENT) / 100)
//----------------Захист: обрив термопари------------------
#define TC_OPEN_COUNTS_THRESHOLD 2500   // насичення при обриві (підтяг R6 тягне до VCC)
#define TC_OPEN_DEBOUNCE         5
//----------------Тепловий захист, шар 2-------------------
#define MAX_SETPOINT_C        450.0f
#define HARD_MAX_TEMP_C       480.0f
#define OVERSHOOT_LIMIT_C     60.0f
#define OVERSHOOT_TIME_MS     2000UL
#define STALL_OUTPUT_FRAC     0.90f
#define STALL_TIME_MS         30000UL   // для 24В БЖ (нагрів швидкий)
#define STALL_MIN_RISE_C      20.0f
//----------------Калібрування температури-----------------
// Біас = сирі counts на ХОЛОДНОМУ жалі (підтяг R6 через R7); підганяється
// під своє залізо: холодне жало -> TIP має дорівнювати IC1118
#define TC_SLOPE_C_PER_COUNT  0.2925f
#define TC_BIAS_COUNTS        79.0f
//----------------Профілі температур-----------------------
#define NUM_PROFILES            3
#define PROFILE_LONG_PRESS_MS   700UL
#define PROF_EEPROM_ADDR        32
#define PROF_MAGIC              0x5052
struct PROF {
  uint16_t magic;
  int16_t  temp[NUM_PROFILES];
  uint8_t  active;
};
PROF prof;
//----------------Function Prototypes------------------
bool SerialReceive(void);
void SendStatusPacket(void);
int  multiMap2(int val, int* _in, int* _out, uint8_t size);
void UpdatePowerMeasurement(void);
float ReadVN5ECurrent_mA(void);
void PulsePin(int pin);
enum OperatingMode { MODE_OFF = 0, MODE_STANDBY = 1, MODE_ON = 2, MODE_FAULT = 3 };
OperatingMode GetOperatingMode(void);
void updateLEDStatus(void);
void ProcessSerialComm(void);
void updateDisplay(bool update_now);
void setupHeaterPwmAndAdcTimer(void);
void setupEncoder(void);
int32_t EncoderRead(void);
void EncoderWrite(int32_t new_pos);
void SaveSettingsIfNeeded(void);
void SaveProfilesIfNeeded(void);
//----------------Structure Definitions----------------
#define NVOL_EEPROM_ADDR 0
#define NVOL_MAGIC 0x4A42
struct NVOL {
  uint16_t magic;
  float    setpoint;
  float    kP; float kI; float kD;
};
NVOL nvol;
#include "project_types.h"
union controller_packet_struct {
  struct {
    volatile status_struct status;
    volatile system_parameters_struct params;
  } payload;
  byte asBytes[sizeof(payload)];
};
union host_packet_struct {
  struct {
    byte start_of_packet;
    system_parameters_struct params;
  } payload;
  byte asBytes[sizeof(payload)];
};
//----------------Standard Global Variables----------------
host_packet_struct host_packet;
controller_packet_struct controller_packet;
volatile status_struct status;
volatile system_parameters_struct params;
#define NUM_CAL_POINTS 4
uint16_t adc_reading [NUM_CAL_POINTS] = {283, 584, 919, 1098};
uint16_t deg_c [NUM_CAL_POINTS] = {105, 200, 300, 345};
volatile int32_t encoder_position = 0;
volatile bool tc_open_fault = false;   // latch до скидання кнопкою/ребуту
volatile byte fault_code   = 0;        // 1=обрив TC, 2=перегрів, 3=runaway, 4=не росте
volatile bool led_update_pending = false; // ISR просить loop оновити LED
//----------------INA219-------------
#define INA219_SHUNT_OHMS   0.0056f
#define MAX_DISPLAY_POWER_W   150
Adafruit_INA219 ina219;
//----------------NeoPixel---------------
#define LED_BRIGHTNESS   150
#define LED_COLOR_OFF       100, 100, 100
#define LED_COLOR_STANDBY     0,   0, 255
#define LED_COLOR_ON          0, 255,   0   // зелений - гріє; червоний ЛИШЕ для FAULT
//----------------Globals Objects----------------------
SPIClass SPI_2(PB15, PB14, PB13);
Adafruit_ST7735 display(&SPI_2, TFT_CS, TFT_DC, TFT_RST);
HardwareSerial Serial2(PA3, PA2);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, WS2812_DATA, NEO_GRB + NEO_KHZ800);
PID myPID(const_cast<double*>(&status.tip_temperature_c),
          const_cast<double*>(&status.pid_output),
          const_cast<double*>(&status.pid_setpoint),
          params.kP, params.kI, params.kD, P_ON_E, DIRECT);
