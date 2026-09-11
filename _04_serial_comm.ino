/* Copyright (c) 2017 timothyjager — MIT License. */
#define SERIAL_COMM_PERIOD_MS 250
void ProcessSerialComm(void)
{
  static long next_millis = millis() + SERIAL_COMM_PERIOD_MS;
  static bool serial_active = false;
  if (millis() > next_millis)
  {
    if (Serial2.available())
    {
      if (SerialReceive()) serial_active = true;
    }
    if (Serial2) SendStatusPacket();

    noInterrupts();
    int16_t tip_temp_copy    = status.tip_temperature_c;
    int16_t ic_raw_copy      = status.adc_ic_temp_counts;
    int16_t voltage_mv_copy  = status.adapter_voltage_mv;
    int16_t current_ma_copy  = status.current_sense_ma;
    double  pid_output_copy  = status.pid_output;
    interrupts();
    int16_t setpoint_copy    = (int16_t)params.setpoint;
    int16_t ic_temp_c_copy   = ADS1118_INT_TEMP_C(ic_raw_copy);
    float   power_w          = ((int32_t)voltage_mv_copy * (int32_t)current_ma_copy) / 1000000.0f;
    float   vn5e_current_ma  = ReadVN5ECurrent_mA();
    int16_t pwm_pct_copy     = (int16_t)((pid_output_copy * 100.0) / MAX_HEATER_DUTY_TICKS);
    const char* mode_str;
    switch (GetOperatingMode())
    {
      case MODE_STANDBY: mode_str = "STANDBY"; break;
      case MODE_ON:      mode_str = "ON";      break;
      case MODE_FAULT:   mode_str = "FAULT";   break;
      default:           mode_str = "OFF";     break;
    }
    Serial2.print("SET=");         Serial2.print(setpoint_copy);        Serial2.print("C");
    Serial2.print("  TIP=");       Serial2.print(tip_temp_copy);        Serial2.print("C");
    Serial2.print("  IC1118=");    Serial2.print(ic_temp_c_copy);       Serial2.print("C");
    Serial2.print("  |  INA219: I="); Serial2.print(current_ma_copy);   Serial2.print("mA");
    Serial2.print(" V=");          Serial2.print(voltage_mv_copy);      Serial2.print("mV");
    Serial2.print(" P=");          Serial2.print(power_w, 1);           Serial2.print("W");
    Serial2.print("  |  VN5E_I="); Serial2.print((int16_t)vn5e_current_ma); Serial2.print("mA");
    Serial2.print("  PWM=");       Serial2.print(pwm_pct_copy);         Serial2.print("%");
    Serial2.print("  MODE=");      Serial2.print(mode_str);
    Serial2.print("  FLT=");       Serial2.print(fault_code);
    Serial2.println();
    next_millis += SERIAL_COMM_PERIOD_MS;
  }
}

void SendStatusPacket()
{
  status.gpio_port_b = (byte)(GPIOA->IDR & 0xFF);
  status.gpio_port_c = (byte)((GPIOA->IDR >> 8) & 0xFF);
  status.gpio_port_d = (byte)(GPIOB->IDR & 0xFF);
  status.gpio_port_e = (byte)((GPIOB->IDR >> 8) & 0xFF);
  noInterrupts();
  memcpy((void*)&controller_packet.payload.status, (const void*)&status, sizeof(status_struct));
  memcpy((void*)&controller_packet.payload.params, (const void*)&params, sizeof(system_parameters_struct));
  interrupts();
  for (int i = 0; i < sizeof(controller_packet_struct); i++)
  {
    const char lookup[] = "0123456789abcdef";
    Serial2.write(lookup[controller_packet.asBytes[i] >> 4]);
    Serial2.write(lookup[controller_packet.asBytes[i] & 0x0f]);
  }
  Serial2.write('\n');
}

bool SerialReceive()
{
  bool return_value = false;
  int index = 0;
  while (Serial2.available() && index < sizeof(host_packet_struct))
  {
    host_packet.asBytes[index] = Serial2.read();
    index++;
  }
  if (index == sizeof(host_packet_struct) && host_packet.payload.start_of_packet == 0xAB)
  {
    noInterrupts();
    memcpy((void*)&params, (const void*)&host_packet.payload.params, sizeof(system_parameters_struct));
    //Кламп уставки з хоста
    if (params.setpoint < 0) params.setpoint = 0;
    if (params.setpoint > MAX_SETPOINT_C) params.setpoint = MAX_SETPOINT_C;
    params.heater_enabled = (params.pid_mode == AUTOMATIC) ? 1 : 0;
    if (params.pid_mode == MANUAL) status.pid_output = params.output_override;
    else status.pid_setpoint = params.setpoint;
    myPID.SetTunings(params.kP, params.kI, params.kD);
    EncoderWrite(params.setpoint);
    interrupts();
    return_value = true;
  }
  while (Serial2.available()) Serial2.read();   // drain RX (flush() чекає TX, а не чистить RX)
  return return_value;
}
