/* Copyright (c) 2017 timothyjager — MIT License. */
#include "display_helpers.h"

// ---- Геометрія шкал ----
#define BAR_Y        15
#define BAR_H        100
#define BAR_W        13
#define BAR_L_X      6
#define BAR_R_X      141
#define BAR_LABEL_TOP_BASELINE_Y     12
#define BAR_LABEL_BOTTOM_BASELINE_Y  126
#define MAIN_NUM_BASELINE_Y          68
#define SET_ROW_BASELINE_Y           100
#define MID_ROW_BASELINE_Y           123
#define SET_ROW_X    21
// ---- Кнопки профілів (верх, на місці колишнього бейджа) ----
#define PROF_BTN_Y     2
#define PROF_BTN_H     14
#define PROF_BTN_W     28
#define PROF_BTN_GAP   4
#define PROF_BTN_X0    32
#define COLOR_PROF_BG_ACTIVE   0xFDE0
#define COLOR_PROF_TEXT_ACTIVE ST7735_BLACK
#define COLOR_PROF_FRAME       0x8410
#define COLOR_PROF_TEXT        ST7735_WHITE
// ---- Кольори ----
#define COLOR_BACKGROUND   ST7735_BLACK
#define COLOR_BAR_L        ST7735_YELLOW
#define COLOR_BAR_R        ST7735_ORANGE
#define COLOR_BAR_LABLE_L  ST7735_YELLOW
#define COLOR_BAR_LABLE_R  ST7735_ORANGE
#define COLOR_BACK_DIGIT   0x2124
#define COLOR_SET          0x867D
#define COLOR_VOLTAGE      0xFD20
#define COLOR_1118         0xAFE5
// ---- Кольори ВЕЛИКИХ цифр за режимом ----
#define COLOR_MAIN_OFF       0x8410          // "OFF" сірим
#define COLOR_MAIN_ON        ST7735_WHITE    // температура - гріє
#define COLOR_MAIN_STANDBY   ST7735_BLUE     // температура - у підставці
#define COLOR_MAIN_FAULT     ST7735_RED      // "Err"

void updateDisplay(bool update_now)
{
#define DISPLAY_UPDATE_PERIOD 250
  static long next_millis = millis() + DISPLAY_UPDATE_PERIOD;
  static bool first_draw = true;
  static int16_t last_setpoint      = -32768;
  static int16_t last_temp          = -32768;
  static int16_t last_voltage_mv    = -32768;
  static int16_t last_current_ma    = -32768;
  static int16_t last_pwm_pct       = -1;

  if (millis() > next_millis || update_now)
  {
    noInterrupts();
    int16_t tip_temperature_copy = status.tip_temperature_c;
    int16_t voltage_mv_copy      = status.adapter_voltage_mv;
    int16_t current_ma_copy      = status.current_sense_ma;
    int16_t ic_temp_copy         = ADS1118_INT_TEMP_C(status.adc_ic_temp_counts);
    double  pid_output_copy      = status.pid_output;
    interrupts();
    int16_t setpoint_copy = (int16_t)params.setpoint;
    int16_t pwm_pct_copy  = (int16_t)((pid_output_copy * 100.0) / MAX_HEATER_DUTY_TICKS);
    OperatingMode mode_copy = GetOperatingMode();
    bool voltage_changed = (voltage_mv_copy != last_voltage_mv);
    bool current_changed = (current_ma_copy != last_current_ma);

    if (first_draw)
    {
      display.fillScreen(COLOR_BACKGROUND);
      display.drawRect(BAR_L_X, BAR_Y, BAR_W, BAR_H, ST7735_WHITE);
      display.drawRect(BAR_R_X, BAR_Y, BAR_W, BAR_H, ST7735_WHITE);
      display.setFont(&DSEG7Classic_Bold12pt7b);
      display.setTextColor(COLOR_SET);
      display.setCursor(SET_ROW_X, SET_ROW_BASELINE_Y);
      display.print("5ET");
      display.setFont(&FreeSans6pt7b);
      display.setTextColor(COLOR_BAR_LABLE_R);
      display.setCursor(BAR_R_X + BAR_W / 2 - 15, BAR_LABEL_BOTTOM_BASELINE_Y);
      display.print("PWM");
      first_draw = false;
    }

    // ============ Кнопки профілів (на місці бейджа) ============
    {
      static int16_t last_pt[NUM_PROFILES] = { -1, -1, -1 };
      static int8_t  last_active = -1;
      for (uint8_t i = 0; i < NUM_PROFILES; i++)
      {
        bool is_act = (i == prof.active);
        bool was_act = (i == last_active);
        if (is_act != was_act || prof.temp[i] != last_pt[i])
        {
          int16_t x = PROF_BTN_X0 + i * (PROF_BTN_W + PROF_BTN_GAP);
          if (is_act)
            display.fillRect(x, PROF_BTN_Y, PROF_BTN_W, PROF_BTN_H, COLOR_PROF_BG_ACTIVE);
          else {
            display.fillRect(x, PROF_BTN_Y, PROF_BTN_W, PROF_BTN_H, COLOR_BACKGROUND);
            display.drawRect(x, PROF_BTN_Y, PROF_BTN_W, PROF_BTN_H, COLOR_PROF_FRAME);
          }
          char b[5];
          snprintf(b, sizeof(b), "%d", prof.temp[i]);
          display.setFont(&FreeSans6pt7b);
          int16_t bx, by; uint16_t bw, bh;
          display.getTextBounds(b, 0, 0, &bx, &by, &bw, &bh);
          display.setTextColor(is_act ? COLOR_PROF_TEXT_ACTIVE : COLOR_PROF_TEXT);
          display.setCursor(x + (PROF_BTN_W - (int16_t)bw) / 2, PROF_BTN_Y + 11);
          display.print(b);
          last_pt[i] = prof.temp[i];
        }
      }
      last_active = prof.active;
    }

    // ============ Ліва шкала - реальна потужність (INA219) ============
    if (voltage_changed || current_changed)
    {
      int32_t power_mw = ((int32_t)voltage_mv_copy * (int32_t)current_ma_copy) / 1000L;
      float power_w = power_mw / 1000.0f;
      int16_t power_pct = (int16_t)((power_w * 100.0f) / MAX_DISPLAY_POWER_W);
      if (power_pct < 0) power_pct = 0;
      if (power_pct > 100) power_pct = 100;
      display.setFont(&FreeSans6pt7b);
      int16_t bar_center_x = BAR_L_X + (BAR_W / 2);
      int16_t bx, by; uint16_t bw, bh;
      {
        char pct_buf[8];
        snprintf(pct_buf, sizeof(pct_buf), "%d%%", power_pct);
        display.getTextBounds(pct_buf, 0, 0, &bx, &by, &bw, &bh);
        static char pct_last[8] = "";
        drawLabelCalm(pct_buf, COLOR_BAR_LABLE_L, &FreeSans6pt7b,
                      0, BAR_LABEL_TOP_BASELINE_Y - 10, 30, 12,
                      bar_center_x - (int16_t)bw / 2, BAR_LABEL_TOP_BASELINE_Y, pct_last);
      }
      {
        char w_buf[8];
        snprintf(w_buf, sizeof(w_buf), "%dW", (int)power_w);
        display.getTextBounds(w_buf, 0, 0, &bx, &by, &bw, &bh);
        static char watt_last[8] = "";
        drawLabelCalm(w_buf, COLOR_BAR_LABLE_L, &FreeSans6pt7b,
                      0, BAR_LABEL_BOTTOM_BASELINE_Y - 10, 30, 12,
                      bar_center_x - (int16_t)bw / 2, BAR_LABEL_BOTTOM_BASELINE_Y, watt_last);
      }
      int16_t fillH = (int16_t)((BAR_H - 2) * (power_pct / 100.0f));
      display.fillRect(BAR_L_X + 1, BAR_Y + 1, BAR_W - 2, (BAR_H - 2) - fillH, COLOR_BACKGROUND);
      display.fillRect(BAR_L_X + 1, BAR_Y + BAR_H - 1 - fillH, BAR_W - 2, fillH, COLOR_BAR_L);
    }

    // ============ Права шкала - шпаруватість ШІМ ============
    if (pwm_pct_copy != last_pwm_pct)
    {
      if (pwm_pct_copy < 0) pwm_pct_copy = 0;
      if (pwm_pct_copy > 100) pwm_pct_copy = 100;
      display.setFont(&FreeSans6pt7b);
      char buf[8];
      snprintf(buf, sizeof(buf), "%d%%", pwm_pct_copy);
      int16_t bx, by; uint16_t bw, bh;
      display.getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
      int16_t bar_center_x = BAR_R_X + (BAR_W / 2);
      static char pwm_last[8] = "";
      drawLabelCalm(buf, COLOR_BAR_LABLE_R, &FreeSans6pt7b,
                    128, BAR_LABEL_TOP_BASELINE_Y - 10, 32, 12,
                    bar_center_x - (int16_t)bw / 2, BAR_LABEL_TOP_BASELINE_Y, pwm_last);
      int16_t fillH = (int16_t)((BAR_H - 2) * (pwm_pct_copy / 100.0f));
      display.fillRect(BAR_R_X + 1, BAR_Y + 1, BAR_W - 2, (BAR_H - 2) - fillH, COLOR_BACKGROUND);
      display.fillRect(BAR_R_X + 1, BAR_Y + BAR_H - 1 - fillH, BAR_W - 2, fillH, COLOR_BAR_R);
      last_pwm_pct = pwm_pct_copy;
    }

    // ============ Великі цифри: OFF / температура / Err ============
    static char last_buf[4] = "";
    static uint16_t last_main_color = 0xFFFF;
    char buf[4];
    uint16_t main_color;
    switch (mode_copy)
    {
      case MODE_FAULT:   memcpy(buf, "Err", 4); main_color = COLOR_MAIN_FAULT;   break;
      case MODE_OFF:     memcpy(buf, "0FF", 4); main_color = COLOR_MAIN_OFF;     break;
      case MODE_STANDBY: snprintf(buf, sizeof(buf), "%3d", tip_temperature_copy);
        main_color = COLOR_MAIN_STANDBY;                        break;
      case MODE_ON:
      default:           snprintf(buf, sizeof(buf), "%3d", tip_temperature_copy);
        main_color = COLOR_MAIN_ON;                             break;
    }
    if (tip_temperature_copy != last_temp || main_color != last_main_color)
    {
      display.setFont(&DSEG7Classic_Bold24pt7b);
      int16_t bx, by; uint16_t total_w, bh;
      display.getTextBounds("888", 0, MAIN_NUM_BASELINE_Y, &bx, &by, &total_w, &bh);
      uint16_t char_step = total_w / 3;
      int16_t startX = ((160 - total_w) / 2) - 2;
      for (uint8_t i = 0; i < 3; i++)
      {
        if (buf[i] != last_buf[i] || main_color != last_main_color)
        {
          int16_t charX = startX + (i * char_step);
          drawCharCalm('8', COLOR_BACK_DIGIT, buf[i], main_color,
                       &DSEG7Classic_Bold24pt7b,
                       charX, MAIN_NUM_BASELINE_Y - 46, 38, 50,
                       charX, MAIN_NUM_BASELINE_Y);
        }
      }
      memcpy(last_buf, buf, sizeof(buf));
      last_temp = tip_temperature_copy;
      last_main_color = main_color;
    }

    // ============ SET рядок ============
    static char last_setpoint_buf[4] = "";
    if (setpoint_copy != last_setpoint)
    {
      char sbuf[4];
      snprintf(sbuf, sizeof(sbuf), "%3d", setpoint_copy);
      display.setFont(&DSEG7Classic_Bold12pt7b);
      int16_t bx, by; uint16_t total_w, bh;
      display.getTextBounds("888", 0, SET_ROW_BASELINE_Y, &bx, &by, &total_w, &bh);
      uint16_t char_step = total_w / 3;
      int16_t startX = SET_ROW_X + 63;
      for (uint8_t i = 0; i < 3; i++)
      {
        if (sbuf[i] != last_setpoint_buf[i])
        {
          int16_t charX = startX + (i * char_step);
          drawCharCalm('8', COLOR_BACK_DIGIT, sbuf[i], COLOR_SET,
                       &DSEG7Classic_Bold12pt7b,
                       charX, SET_ROW_BASELINE_Y - 23, 20, 26,
                       charX, SET_ROW_BASELINE_Y);
        }
      }
      memcpy(last_setpoint_buf, sbuf, sizeof(buf));
      last_setpoint = setpoint_copy;
    }

    // ============ Напруга ============
    if (voltage_changed)
    {
      char v_buf[8];
      dtostrf(voltage_mv_copy / 1000.0, 0, 1, v_buf);
      strcat(v_buf, "V");
      static char volt_last[8] = "";
      drawLabelCalm(v_buf, COLOR_VOLTAGE, &FreeSans9pt7b,
                    30, MID_ROW_BASELINE_Y - 14, 52, 18,
                    30, MID_ROW_BASELINE_Y, volt_last);
    }

    // ============ Температура ADS1118 (холодний спай) ============
    {
      const int16_t TEMP_RIGHT_X = 124;

      char t_buf[8];
      snprintf(t_buf, sizeof(t_buf), "%d", ic_temp_copy);
      static char temp_last[8] = "";
      if (strcmp(t_buf, temp_last) != 0)
      {
        display.setFont(&FreeSans9pt7b);
        int16_t bx, by; uint16_t wd, wc, bh;
        display.getTextBounds(t_buf, 0, 0, &bx, &by, &wd, &bh);   // ширина цифр
        display.getTextBounds("C",   0, 0, &bx, &by, &wc, &bh);   // ширина "C"
        uint16_t total = wd + 6 + wc;              // цифри + коло + C
        int16_t x0 = TEMP_RIGHT_X - (int16_t)total;
        display.fillRect(TEMP_RIGHT_X - 44, MID_ROW_BASELINE_Y - 14, 44, 18, COLOR_BACKGROUND);
        display.setTextColor(COLOR_1118);
        display.setCursor(x0, MID_ROW_BASELINE_Y);
        display.print(t_buf);
        display.drawCircle(x0 + wd + 3, MID_ROW_BASELINE_Y - 10, 2, COLOR_1118); // знак °
        display.setCursor(x0 + wd + 6, MID_ROW_BASELINE_Y);
        display.print("C");
        strcpy(temp_last, t_buf);
      }
    }

    last_voltage_mv = voltage_mv_copy;
    last_current_ma = current_ma_copy;
    if (!update_now) next_millis += DISPLAY_UPDATE_PERIOD;
  }
}
