/* Copyright (c) 2017 timothyjager — MIT License.
   TIM2 через HardwareTimer: CH1 = ШІМ нагрівача, CH2+Update = вікно АЦП. */
#include <SPI.h>
#include <HardwareTimer.h>
#include "ads1118.h"

HardwareTimer *heaterTimer = new HardwareTimer(TIM2);

void OnAdcWindowStart(void);
void OnPeriodEnd(void);

void setupHeaterPwmAndAdcTimer(void)
{
  heaterTimer->setOverflow(PWM_PERIOD_US, MICROSEC_FORMAT);
  heaterTimer->setMode(1, TIMER_OUTPUT_COMPARE_PWM1, PA0);
  heaterTimer->setCaptureCompare(1, 0, MICROSEC_COMPARE_FORMAT); // старт з вимкненим нагрівачем
  heaterTimer->setMode(2, TIMER_OUTPUT_COMPARE);
  heaterTimer->setCaptureCompare(2, MAX_HEATER_DUTY_TICKS, MICROSEC_COMPARE_FORMAT);
  heaterTimer->attachInterrupt(2, OnAdcWindowStart);
  heaterTimer->attachInterrupt(OnPeriodEnd);
  heaterTimer->setInterruptPriority(1, 0);
  heaterTimer->resume();
  heaterTimer->resumeChannel(2);
}

static void setHeaterDutyTicks(uint16_t us)
{
  if (us > MAX_HEATER_DUTY_LIMITED) us = MAX_HEATER_DUTY_LIMITED;
  heaterTimer->setCaptureCompare(1, us, MICROSEC_COMPARE_FORMAT);
}

void OnAdcWindowStart(void)
{
  digitalWrite(ADS_CS_PIN, LOW);
  status.adc_ic_temp_counts = SPI.transfer16(ADS1118_SINGLE_SHOT_ADC);
  digitalWrite(ADS_CS_PIN, HIGH);
  digitalWrite(ADS_CS_PIN, LOW);   // лишаємо LOW для читання в OnPeriodEnd
}

void OnPeriodEnd(void)
{
  int16_t adc_raw = SPI.transfer16(ADS1118_SINGLE_SHOT_INTERNAL_TEMPERATURE);
  digitalWrite(ADS_CS_PIN, HIGH);

  //==== ЗАХИСТ: обрив термопари (перевіряємо RAW ДО фільтра!) ====
  static uint8_t tc_open_cnt = 0;
  if (adc_raw > TC_OPEN_COUNTS_THRESHOLD) { if (tc_open_cnt < 255) tc_open_cnt++; }
  else tc_open_cnt = 0;
  if (tc_open_cnt >= TC_OPEN_DEBOUNCE) { tc_open_fault = true; fault_code = 1; }

  if (tc_open_fault)
  {
    status.adc_counts        = adc_raw;  // сирий рівень - у діагностику
    status.tip_temperature_c = 0;
    status.pid_output        = 0;
    setHeaterDutyTicks(0);               // нагрівач вимкнений щоперіоду
    led_update_pending = true;
    return;                              // ПІД не рахується ВЗАГАЛІ
  }

  //==== Фільтр викидів: порівнюємо з останнім ПРИЙНЯТИМ (status.adc_counts),
  //     а не з попереднім сирим - інакше одиночний глюк "з'їдав" і наступне читання
  if (abs((int32_t)adc_raw - (int32_t)status.adc_counts) < 1000)
  {
    status.adc_counts = adc_raw;
  }

  //==== Температура з компенсацією холодного спаю ====
  // T_жала = T_холодного_спаю + нахил*(counts - біас підтягу)
  double cj_temp_c  = (double)status.adc_ic_temp_counts / 128.0;
  double tip_temp_c = TC_SLOPE_C_PER_COUNT * ((double)status.adc_counts - TC_BIAS_COUNTS) + cj_temp_c;

  status.tip_temperature_c = (params.simulate_input == 1)
                             ? params.simulated_input
                             : tip_temp_c;

  myPID.Compute();
  setHeaterDutyTicks((uint16_t)status.pid_output);
  led_update_pending = true;   // LED оновлює loop, а не ISR
}

/* ---- Квадратурний декодер енкодера ---- */
static uint8_t enc_state = 0;
static const int8_t enc_transition_table[16] = {
  0, -1,  1,  0,
  1,  0,  0, -1,
 -1,  0,  0,  1,
  0,  1, -1,  0
};
static void EncoderISR(void)
{
  enc_state = ((enc_state << 2) | (digitalRead(ENC_A) << 1) | digitalRead(ENC_B)) & 0x0F;
  encoder_position += enc_transition_table[enc_state];
}
void setupEncoder(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
  AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG) | AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), EncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), EncoderISR, CHANGE);
}
int32_t EncoderRead(void)
{
  noInterrupts();
  int32_t pos = encoder_position;
  interrupts();
  return pos;
}
void EncoderWrite(int32_t new_pos)
{
  noInterrupts();
  encoder_position = new_pos;
  interrupts();
}
