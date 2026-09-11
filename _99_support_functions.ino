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
  if (digitalRead(CRADLE_SENSOR) == false) return MODE_STANDBY;
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
