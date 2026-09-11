/* Copyright (c) 2017 timothyjager
   JBC-Soldering-Controller
   MIT License. See LICENSE file for details.
*/

//----------------Main Loop------------------------

void loop(void)
{
  IWatchdog.reload();

  bool update_display_now = false;

  //==== Кнопка енкодера: коротке / утримання / утримання+обертання ====
  static bool btn_prev = false;
  static uint32_t btn_down_since = 0;
  static bool btn_long_fired = false;
  static bool btn_edit_mode = false;
  static bool btn_pending_switch = false;
  static int16_t knob_pos_last = 0;
  static bool first_loop = true;

  bool btn_now = (digitalRead(ENC_BUTTON) == false);

  if (first_loop) {
    knob_pos_last = EncoderRead();
    first_loop = false;
  }

  if (btn_now && !btn_prev)
  {
    btn_down_since = millis();
    btn_long_fired = false;
    btn_edit_mode = false;
    btn_pending_switch = true;
  }

  // УТРИМАННЯ без обертання -> наступний профіль
  if (btn_now && !btn_long_fired && btn_pending_switch &&
      (millis() - btn_down_since >= PROFILE_LONG_PRESS_MS))
  {
    btn_long_fired = true;
    btn_edit_mode = true;

    prof.active = (prof.active + 1) % NUM_PROFILES;

    params.setpoint = prof.temp[prof.active];

    EncoderWrite((int32_t)params.setpoint);
    status.encoder_pos = (int16_t)params.setpoint;
    knob_pos_last = (int16_t)params.setpoint;

    if (myPID.GetMode() == AUTOMATIC)
    {
      noInterrupts();
      status.pid_setpoint = params.setpoint;
      interrupts();
    }

    update_display_now = true;
  }

  if (!btn_now && btn_prev)
  {
    if (!btn_long_fired)
    {
      if (tc_open_fault)
      {
        bool clearable = false;
        switch (fault_code)
        {
          case 1: clearable = (status.adc_counts < TC_OPEN_COUNTS_THRESHOLD); break;
          case 2: clearable = (status.tip_temperature_c < HARD_MAX_TEMP_C - 50.0); break;
          case 3: clearable = (status.tip_temperature_c < (double)params.setpoint + OVERSHOOT_LIMIT_C); break;
          default: clearable = true; break;
        }
        if (clearable) { tc_open_fault = false; fault_code = 0; }
      }
      else params.heater_enabled = !params.heater_enabled;
    }
    btn_edit_mode = false;
    btn_long_fired = false;
    btn_pending_switch = false;
  }

  btn_prev = btn_now;

  //==== Енкодер ====
  status.encoder_pos = EncoderRead();

  int16_t delta = status.encoder_pos - knob_pos_last;

  if (delta != 0)
  {
    if (btn_now && !btn_long_fired)
    {
      btn_long_fired = true;
      btn_pending_switch = false;
      btn_edit_mode = true;
    }

    if (btn_edit_mode)
    {
      prof.temp[prof.active] += delta;
      if (prof.temp[prof.active] < 0) prof.temp[prof.active] = 0;
      if (prof.temp[prof.active] > (int16_t)MAX_SETPOINT_C)
        prof.temp[prof.active] = (int16_t)MAX_SETPOINT_C;
      params.setpoint = prof.temp[prof.active];
    }
    else
    {
      params.setpoint += delta;
      if (params.setpoint < 0) params.setpoint = 0;
      if (params.setpoint > MAX_SETPOINT_C) params.setpoint = MAX_SETPOINT_C;
    }

    EncoderWrite((int32_t)params.setpoint);
    status.encoder_pos = (int16_t)params.setpoint;

    if (myPID.GetMode() == AUTOMATIC)
    {
      noInterrupts();
      status.pid_setpoint = params.setpoint;
      interrupts();
    }

    update_display_now = true;
  }

  knob_pos_last = status.encoder_pos;

  //==== Узгодження режиму (бажання + підставка + аварія) ====
  bool in_cradle = (digitalRead(CRADLE_SENSOR) == false);

  byte desired_pid_mode = (params.heater_enabled && !in_cradle && !tc_open_fault)
                          ? AUTOMATIC : MANUAL;

  if (desired_pid_mode != params.pid_mode)
  {
    noInterrupts();
    params.pid_mode = desired_pid_mode;
    myPID.SetMode(params.pid_mode);
    if (params.pid_mode == AUTOMATIC) status.pid_setpoint = params.setpoint;
    else { status.pid_output = 0; status.pid_setpoint = 0; }
    interrupts();
  }

  //==== Тепловий захист, шар 2 ====
  {
    noInterrupts();
    double tip_now = status.tip_temperature_c;
    double out_now = status.pid_output;
    interrupts();

    if (tip_now > HARD_MAX_TEMP_C) { tc_open_fault = true; fault_code = 2; }

    // ВИПРАВЛЕНО: Гнучка перевірка динаміки температури при охолодженні
    static uint32_t ov_since = 0;
    static uint32_t temp_check_time = 0;
    static double temp_at_check_start = 0;
    static double temp_at_prev_check = 0;
    static float last_setpoint_for_ov = params.setpoint;
    
    // Якщо setpoint змінився - скидаємо таймер перевірки
    if (abs(params.setpoint - last_setpoint_for_ov) > 1.0) {
      ov_since = 0;
      temp_check_time = 0;
      temp_at_check_start = 0;
      temp_at_prev_check = 0;
      last_setpoint_for_ov = params.setpoint;
    }

    if (params.pid_mode == AUTOMATIC &&
        tip_now > (double)params.setpoint + OVERSHOOT_LIMIT_C)
    {
      if (ov_since == 0) {
        ov_since = millis();
        temp_at_check_start = tip_now;
        temp_at_prev_check = tip_now;
        temp_check_time = millis();
      }
      else if (millis() - ov_since > OVERSHOOT_TIME_MS) {
        // Перевіряємо динаміку температури кожні 5 секунд
        bool temp_is_dropping = false;
        if (temp_check_time > 0 && millis() - temp_check_time >= 5000) {
          // Порівнюємо з попередньою перевіркою
          double temp_diff = temp_at_prev_check - tip_now;
          
          // Якщо температура впала хоча б на 1°C за 5 секунд - це охолодження
          if (temp_diff >= 1.0) {
            temp_is_dropping = true;
          }
          
          // Оновлюємо для наступної перевірки
          temp_at_prev_check = tip_now;
          temp_check_time = millis();
        }
        
        // Активуємо помилку тільки якщо температура НЕ падає взагалі
        if (!temp_is_dropping && millis() - ov_since > 15000) {
          // Додатково чекаємо 15 секунд перед помилкою
          tc_open_fault = true; 
          fault_code = 3;
        }
      }
    }
    else ov_since = 0;

    static uint32_t st_since = 0;
    static double st_tip0 = 0;

    if (params.pid_mode == AUTOMATIC &&
        out_now > (double)MAX_HEATER_DUTY_TICKS * STALL_OUTPUT_FRAC)
    {
      if (st_since == 0) { st_since = millis(); st_tip0 = tip_now; }
      else if (millis() - st_since > STALL_TIME_MS)
      {
        if (tip_now - st_tip0 < STALL_MIN_RISE_C) { tc_open_fault = true; fault_code = 4; }
        else { st_since = millis(); st_tip0 = tip_now; }
      }
    }
    else st_since = 0;
  }

#define POWER_MEASURE_PERIOD_MS 250
  static long power_next_millis = millis() + POWER_MEASURE_PERIOD_MS;
  if (millis() > power_next_millis)
  {
    UpdatePowerMeasurement();
    power_next_millis += POWER_MEASURE_PERIOD_MS;
  }

  ProcessSerialComm();

  if (led_update_pending)
  {
    led_update_pending = false;
    updateLEDStatus();
  }

  updateDisplay(update_display_now);
  SaveSettingsIfNeeded();
  SaveProfilesIfNeeded();
}

void SaveSettingsIfNeeded(void)
{
#define EEPROM_SAVE_DELAY_MS 3000
  static float last_setpoint = params.setpoint;
  static float last_kP = params.kP;
  static float last_kI = params.kI;
  static float last_kD = params.kD;
  static long last_change_millis = 0;
  static bool dirty = false;

  if (params.setpoint != last_setpoint || params.kP != last_kP ||
      params.kI != last_kI || params.kD != last_kD)
  {
    last_setpoint = params.setpoint;
    last_kP = params.kP; last_kI = params.kI; last_kD = params.kD;
    last_change_millis = millis();
    dirty = true;
  }

  if (dirty && (millis() - last_change_millis >= EEPROM_SAVE_DELAY_MS))
  {
    nvol.magic = NVOL_MAGIC;
    nvol.setpoint = params.setpoint;
    nvol.kP = params.kP; nvol.kI = params.kI; nvol.kD = params.kD;
    EEPROM.put(NVOL_EEPROM_ADDR, nvol);
    dirty = false;
  }
}

void SaveProfilesIfNeeded(void)
{
  static int16_t last_t0 = prof.temp[0];
  static int16_t last_t1 = prof.temp[1];
  static int16_t last_t2 = prof.temp[2];
  static uint8_t last_act = prof.active;
  static long last_change = 0;
  static bool dirty = false;

  if (prof.temp[0] != last_t0 || prof.temp[1] != last_t1 ||
      prof.temp[2] != last_t2 || prof.active != last_act)
  {
    last_t0 = prof.temp[0]; last_t1 = prof.temp[1]; last_t2 = prof.temp[2];
    last_act = prof.active;
    last_change = millis();
    dirty = true;
  }

  if (dirty && (millis() - last_change >= 3000))
  {
    EEPROM.put(PROF_EEPROM_ADDR, prof);
    dirty = false;
  }
}
