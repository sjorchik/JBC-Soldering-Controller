/* Copyright (c) 2017 timothyjager — MIT License. */
int multiMap2(int val, int* _in, int* _out, uint8_t size)
{
  if (val <= _in[0]) return _out[0];
  uint8_t pos = 1;
  while (val > _in[pos] && pos < size - 1) pos++;
  if (val == _in[pos]) return _out[pos];
  return (val - _in[pos - 1]) * (_out[pos] - _out[pos - 1]) / (_in[pos] - _in[pos - 1]) + _out[pos - 1];
}

void UpdatePowerMeasurement(void)
{
  float bus_voltage_v = ina219.getBusVoltage_V();
  float shunt_mv = ina219.getShuntVoltage_mV();
  float current_ma = shunt_mv / INA219_SHUNT_OHMS;
  status.adapter_voltage_mv = (int16_t)(bus_voltage_v * 1000.0f);
  status.current_sense_ma   = (int16_t)current_ma;
}

float ReadVN5ECurrent_mA(void)
{
  int raw = analogRead(VN5E_CS);
  float vsense = (raw / 1023.0f) * 3.3f;
  float isense_ma = (vsense / VN5E_RSENSE_OHMS) * 1000.0f;
  return isense_ma * VN5E_K_TYPICAL;
}

void PulsePin(int pin)
{
  digitalWrite(pin, HIGH);
  digitalWrite(pin, LOW);
}

OperatingMode GetOperatingMode(void)
{
  if (tc_open_fault)            return MODE_FAULT;    // аварія - найвищий пріоритет
  if (!params.heater_enabled)   return MODE_OFF;
  if (cradle_state_debounced)   return MODE_STANDBY;  // Використовуємо debounced стан
  return MODE_ON;
}

void updateLEDStatus(void)
{
  switch (GetOperatingMode())
  {
    case MODE_FAULT:   // аварійне мигання червоним ~2 Гц
      if ((millis() / 250) & 1) pixels.setPixelColor(0, pixels.Color(255, 0, 0));
      else                      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      break;
    case MODE_STANDBY:
      pixels.setPixelColor(0, pixels.Color(LED_COLOR_STANDBY));
      break;
    case MODE_ON:
      pixels.setPixelColor(0, pixels.Color(LED_COLOR_ON));
      break;
    case MODE_OFF:
    default:
      pixels.setPixelColor(0, pixels.Color(LED_COLOR_OFF));
      break;
  }
  pixels.show();
}

// ==== Функції Пищалки ====
void SetupBuzzer(void) {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void BuzzerBeep(uint16_t freq, uint16_t duration_ms) {
  if (!buzzer_enabled) return;
  if (freq == 0) {
    noTone(BUZZER_PIN);
    return;
  }
  tone(BUZZER_PIN, freq, duration_ms);
}

void BuzzerBeepReachTemp(void) { BuzzerBeep(tone_freq_reach_temp, 200); }
void BuzzerBeepCradleOn(void) { BuzzerBeep(tone_freq_cradle_on, 50); }
void BuzzerBeepCradleOff(void) { BuzzerBeep(tone_freq_cradle_off, 50); }
void BuzzerBeepEncRotate(void) { BuzzerBeep(tone_freq_enc_rotate, 30); }
void BuzzerBeepEncPress(void) { BuzzerBeep(tone_freq_enc_press, 50); }
void BuzzerBeepEncHold(void) { BuzzerBeep(tone_freq_enc_hold, 100); }

// ==== Звуки помилок ====
void BuzzerBeepFault(void) { 
  // Використовуємо тривалість без таймауту (керуємо вручну через state machine)
  if (!buzzer_enabled || tone_freq_fault == 0) return;
  tone(BUZZER_PIN, tone_freq_fault);
}

void ProcessFaultSounds(void) {
  // Станова машина для послідовності писків помилок
  // Стани: 0=IDLE, 1=BEEP_ON, 2=BEEP_OFF (між писками), 3=CYCLE_PAUSE
  static uint8_t fault_seq_state = 0;
  static uint8_t fault_beep_counter = 0;
  static uint32_t fault_last_change = 0;
  static byte last_fault_code = 0;
  
  if (fault_code > 0 && fault_code <= 4) {
    // Якщо помилка змінилася - скидаємо послідовність
    if (last_fault_code != fault_code) {
      fault_seq_state = 0;
      fault_last_change = millis();
      last_fault_code = fault_code;
    }
    
    uint32_t now = millis();
    
    switch (fault_seq_state) {
      case 0: // IDLE - починаємо цикл писків
        if (buzzer_enabled && tone_freq_fault > 0) {
          BuzzerBeepFault();
          fault_beep_counter = 1;
          fault_seq_state = 1;
          fault_last_change = now;
        } else {
          // Якщо звук вимкнений - просто пропускаємо
          fault_seq_state = 3;
          fault_last_change = now;
        }
        break;
        
      case 1: // BEEP_ON - триває писк (200мс)
        if (now - fault_last_change >= 200) {
          noTone(BUZZER_PIN);
          if (fault_beep_counter >= fault_code) {
            // Всі писки зроблені - пауза перед повтором
            fault_seq_state = 3;
          } else {
            // Ще будуть писки - пауза між ними
            fault_seq_state = 2;
          }
          fault_last_change = now;
        }
        break;
        
      case 2: // BEEP_OFF - пауза між писками (200мс)
        if (now - fault_last_change >= 200) {
          fault_beep_counter++;
          if (buzzer_enabled && tone_freq_fault > 0) {
            BuzzerBeepFault();
          }
          fault_seq_state = 1;
          fault_last_change = now;
        }
        break;
        
      case 3: // CYCLE_PAUSE - пауза між циклами (1500мс)
        if (now - fault_last_change >= 1500) {
          fault_seq_state = 0; // Починаємо новий цикл
        }
        break;
    }
  } else {
    // Немає помилки - скидаємо стан та вимикаємо звук
    if (fault_seq_state != 0 || last_fault_code != 0) {
      noTone(BUZZER_PIN);
      fault_seq_state = 0;
      fault_beep_counter = 0;
      last_fault_code = 0;
    }
  }
}
