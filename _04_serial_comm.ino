/* Copyright (c) 2017 timothyjager
   JBC-Soldering-Controller
   MIT License. See LICENSE file for details.

   Patched version:
   - Додано текстові команди через Serial2.
   - Можна міняти PID-коефіцієнти на ходу через Serial Monitor.
   - Старий бінарний протокол у цьому файлі вимкнено.

   Команди:
   HELP
   PID?
   KP=<значення>
   KI=<значення>
   KD=<значення>
   SET=<температура>
   HEATER=ON
   HEATER=OFF
   HEATER=1
   HEATER=0
   STATUS
   SAVE
*/

#define SERIAL_COMM_PERIOD_MS 250
#define SERIAL_CMD_MAX_LEN    64

static String serial_rx_line = "";


String GetCommandValue(String cmd, int start_pos)
{
  String value = cmd.substring(start_pos);
  value.trim();
  return value;
}


void ApplyPidTunings(void)
{
  noInterrupts();
  myPID.SetTunings(params.kP, params.kI, params.kD);
  interrupts();
}


void ProcessTextCommand(String cmd)
{
  cmd.trim();

  if (cmd.length() == 0)
  {
    return;
  }

  cmd.toUpperCase();

  // ================= HELP =================
  if (cmd == "HELP" || cmd == "?")
  {
    Serial2.println(F("Available commands:"));
    Serial2.println(F("  HELP"));
    Serial2.println(F("  PID?"));
    Serial2.println(F("  KP=<float>"));
    Serial2.println(F("  KI=<float>"));
    Serial2.println(F("  KD=<float>"));
    Serial2.println(F("  SET=<C>"));
    Serial2.println(F("  HEATER=ON|OFF|1|0"));
    Serial2.println(F("  STATUS"));
    Serial2.println(F("  SAVE"));
    return;
  }

  // ================= PID? =================
  if (cmd == "PID?")
  {
    Serial2.print(F("OK KP="));
    Serial2.print(params.kP, 3);
    Serial2.print(F(" KI="));
    Serial2.print(params.kI, 3);
    Serial2.print(F(" KD="));
    Serial2.println(params.kD, 3);
    return;
  }

  // ================= KP= =================
  if (cmd.startsWith("KP="))
  {
    String value = GetCommandValue(cmd, 3);

    if (value.length() == 0)
    {
      Serial2.println(F("ERR Usage: KP=<value>"));
      return;
    }

    float v = value.toFloat();

    if (v < 0.0f)
    {
      v = 0.0f;
    }

    noInterrupts();
    params.kP = v;
    interrupts();

    ApplyPidTunings();

    Serial2.print(F("OK KP="));
    Serial2.println(params.kP, 3);
    return;
  }

  // ================= KI= =================
  if (cmd.startsWith("KI="))
  {
    String value = GetCommandValue(cmd, 3);

    if (value.length() == 0)
    {
      Serial2.println(F("ERR Usage: KI=<value>"));
      return;
    }

    float v = value.toFloat();

    if (v < 0.0f)
    {
      v = 0.0f;
    }

    noInterrupts();
    params.kI = v;
    interrupts();

    ApplyPidTunings();

    Serial2.print(F("OK KI="));
    Serial2.println(params.kI, 3);
    return;
  }

  // ================= KD= =================
  if (cmd.startsWith("KD="))
  {
    String value = GetCommandValue(cmd, 3);

    if (value.length() == 0)
    {
      Serial2.println(F("ERR Usage: KD=<value>"));
      return;
    }

    float v = value.toFloat();

    if (v < 0.0f)
    {
      v = 0.0f;
    }

    noInterrupts();
    params.kD = v;
    interrupts();

    ApplyPidTunings();

    Serial2.print(F("OK KD="));
    Serial2.println(params.kD, 3);
    return;
  }

  // ================= SET= =================
  if (cmd.startsWith("SET="))
  {
    String value = GetCommandValue(cmd, 4);

    if (value.length() == 0)
    {
      Serial2.println(F("ERR Usage: SET=<C>"));
      return;
    }

    float v = value.toFloat();

    if (v < 0.0f)
    {
      v = 0.0f;
    }

    if (v > MAX_SETPOINT_C)
    {
      v = MAX_SETPOINT_C;
    }

    noInterrupts();
    params.setpoint = v;

    if (myPID.GetMode() == AUTOMATIC)
    {
      status.pid_setpoint = params.setpoint;
    }

    interrupts();

    EncoderWrite((int32_t)params.setpoint);

    Serial2.print(F("OK SET="));
    Serial2.println(params.setpoint, 1);
    return;
  }

  // ================= HEATER= =================
  if (cmd.startsWith("HEATER="))
  {
    String value = GetCommandValue(cmd, 7);
    value.toUpperCase();

    if (value.length() == 0)
    {
      Serial2.println(F("ERR Usage: HEATER=ON|OFF|1|0"));
      return;
    }

    noInterrupts();

    if (value == "ON" || value == "1")
    {
      params.heater_enabled = 1;
    }
    else if (value == "OFF" || value == "0")
    {
      params.heater_enabled = 0;
    }

    interrupts();

    Serial2.print(F("OK HEATER="));

    if (params.heater_enabled)
    {
      Serial2.println(F("ON"));
    }
    else
    {
      Serial2.println(F("OFF"));
    }

    return;
  }

  // ================= STATUS =================
  if (cmd == "STATUS")
  {
    noInterrupts();
    int16_t tip_temp_copy   = status.tip_temperature_c;
    int16_t voltage_mv_copy = status.adapter_voltage_mv;
    int16_t current_ma_copy = status.current_sense_ma;
    double  pid_output_copy = status.pid_output;
    interrupts();

    int16_t setpoint_copy = (int16_t)params.setpoint;
    int16_t pwm_pct_copy  = (int16_t)((pid_output_copy * 100.0) / MAX_HEATER_DUTY_TICKS);

    float power_w = ((int32_t)voltage_mv_copy * (int32_t)current_ma_copy) / 1000000.0f;

    Serial2.print(F("STATUS SET="));
    Serial2.print(setpoint_copy);
    Serial2.print(F("C TIP="));
    Serial2.print(tip_temp_copy);
    Serial2.print(F("C V="));
    Serial2.print(voltage_mv_copy);
    Serial2.print(F("mV I="));
    Serial2.print(current_ma_copy);
    Serial2.print(F("mA P="));
    Serial2.print(power_w, 1);
    Serial2.print(F("W PWM="));
    Serial2.print(pwm_pct_copy);
    Serial2.print(F("% MODE="));

    switch (GetOperatingMode())
    {
      case MODE_STANDBY:
        Serial2.print(F("STANDBY"));
        break;

      case MODE_ON:
        Serial2.print(F("ON"));
        break;

      case MODE_FAULT:
        Serial2.print(F("FAULT"));
        break;

      default:
        Serial2.print(F("OFF"));
        break;
    }

    Serial2.print(F(" FLT="));
    Serial2.println(fault_code);

    return;
  }

  // ================= SAVE =================
  if (cmd == "SAVE")
  {
    noInterrupts();
    nvol.magic = NVOL_MAGIC;
    nvol.setpoint = params.setpoint;
    nvol.kP = params.kP;
    nvol.kI = params.kI;
    nvol.kD = params.kD;
    interrupts();

    EEPROM.put(NVOL_EEPROM_ADDR, nvol);

    Serial2.println(F("OK Settings saved to flash/EEPROM emulation"));
    return;
  }

  // ================= UNKNOWN =================
  Serial2.print(F("ERR Unknown command: "));
  Serial2.println(cmd);
  Serial2.println(F("Send HELP"));
}


void HandleSerialText(void)
{
  while (Serial2.available())
  {
    char c = (char)Serial2.read();

    if (c == '\n')
    {
      ProcessTextCommand(serial_rx_line);
      serial_rx_line = "";
    }
    else if (c == '\r')
    {
      // Ігноруємо каретку.
    }
    else if (c >= 32 && c <= 126)
    {
      if (serial_rx_line.length() < SERIAL_CMD_MAX_LEN)
      {
        serial_rx_line += c;
      }
    }
  }
}


void ProcessSerialComm(void)
{
  // Спершу обробляємо текстові команди, щоб не чекати 250 мс.
  HandleSerialText();

  static long next_millis = millis() + SERIAL_COMM_PERIOD_MS;

  if (millis() > next_millis)
  {
    // Якщо потрібен старий бінарний протокол — розкоментуйте.
    // if (Serial2) SendStatusPacket();

    noInterrupts();
    int16_t tip_temp_copy   = status.tip_temperature_c;
    int16_t ic_raw_copy     = status.adc_ic_temp_counts;
    int16_t voltage_mv_copy = status.adapter_voltage_mv;
    int16_t current_ma_copy = status.current_sense_ma;
    double  pid_output_copy = status.pid_output;
    float   kP_copy         = params.kP;
    float   kI_copy         = params.kI;
    float   kD_copy         = params.kD;
    interrupts();

    int16_t setpoint_copy  = (int16_t)params.setpoint;
    int16_t ic_temp_c_copy = ADS1118_INT_TEMP_C(ic_raw_copy);

    float power_w = ((int32_t)voltage_mv_copy * (int32_t)current_ma_copy) / 1000000.0f;
    float vn5e_current_ma = ReadVN5ECurrent_mA();

    int16_t pwm_pct_copy = (int16_t)((pid_output_copy * 100.0) / MAX_HEATER_DUTY_TICKS);

    const char* mode_str;

    switch (GetOperatingMode())
    {
      case MODE_STANDBY:
        mode_str = "STANDBY";
        break;

      case MODE_ON:
        mode_str = "ON";
        break;

      case MODE_FAULT:
        mode_str = "FAULT";
        break;

      default:
        mode_str = "OFF";
        break;
    }

    Serial2.print(F("SET="));
    Serial2.print(setpoint_copy);
    Serial2.print(F("C"));

    Serial2.print(F("  TIP="));
    Serial2.print(tip_temp_copy);
    Serial2.print(F("C"));

    Serial2.print(F("  IC1118="));
    Serial2.print(ic_temp_c_copy);
    Serial2.print(F("C"));

    Serial2.print(F("  |  INA219: I="));
    Serial2.print(current_ma_copy);
    Serial2.print(F("mA"));

    Serial2.print(F(" V="));
    Serial2.print(voltage_mv_copy);
    Serial2.print(F("mV"));

    Serial2.print(F(" P="));
    Serial2.print(power_w, 1);
    Serial2.print(F("W"));

    Serial2.print(F("  |  VN5E_I="));
    Serial2.print((int16_t)vn5e_current_ma);
    Serial2.print(F("mA"));

    Serial2.print(F("  PWM="));
    Serial2.print(pwm_pct_copy);
    Serial2.print(F("%"));

    Serial2.print(F("  MODE="));
    Serial2.print(mode_str);

    Serial2.print(F("  FLT="));
    Serial2.print(fault_code);

    Serial2.print(F("  kP="));
    Serial2.print(kP_copy, 1);
    Serial2.print(F("  kI="));
    Serial2.print(kI_copy, 2);
    Serial2.print(F("  kD="));
    Serial2.print(kD_copy, 2);

    Serial2.println();

    next_millis += SERIAL_COMM_PERIOD_MS;
  }
}
