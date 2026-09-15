/* Copyright (c) 2017 timothyjager — MIT License. */

void setup(void)
{
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, LOW);

  pinMode(CRADLE_SENSOR, INPUT_PULLUP);
  pinMode(VN5E_CS, INPUT_ANALOG);
  pinMode(ENC_BUTTON, INPUT_PULLUP);
  pinMode(ADS_CS_PIN, OUTPUT);

  setupEncoder();
  analogReadResolution(10);

  Serial2.begin(230400);

  pixels.begin();
  pixels.setBrightness(LED_BRIGHTNESS);
  pixels.setPixelColor(0, pixels.Color(LED_COLOR_OFF));
  pixels.show();

  //==== ВИПРАВЛЕНО: Коректне завантаження з EEPROM ====
  EEPROM.get(NVOL_EEPROM_ADDR, nvol);
  bool nvol_valid = (nvol.magic == NVOL_MAGIC);

  if (nvol_valid) {
    params.setpoint = nvol.setpoint;
    params.kP = nvol.kP; params.kI = nvol.kI; params.kD = nvol.kD;
  } else {
    params.setpoint = 0;
    params.kP = 500.0; params.kI = 1; params.kD = 0.00;
  }

  // Профілі: свої дані або дефолт 200/250/300
  EEPROM.get(PROF_EEPROM_ADDR, prof);

  if (prof.magic != PROF_MAGIC) {
    prof.magic = PROF_MAGIC;
    prof.temp[0] = 200; prof.temp[1] = 250; prof.temp[2] = 300;
    prof.active = 0;
  }

  if (prof.active >= NUM_PROFILES) prof.active = 0;

  // На старті беремо температуру з профілю ЛИШЕ якщо в пам'яті немає останньої уставки
  if (!nvol_valid) {
    params.setpoint = prof.temp[prof.active];
  }

  EncoderWrite((int32_t)params.setpoint);

  SPI_2.begin();
  display.initR(INITR_BLACKTAB);
  display.setRotation(1);
  display.fillScreen(ST7735_BLACK);
  display.setTextWrap(false);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  if (!ina219.begin()) {
    display.setFont(&FreeSans6pt7b);
    display.setTextColor(ST7735_RED);
    display.setCursor(4, 20);
    display.print("INA219 not found!");
    delay(2000);
  }

  {
    uint16_t ina219_config = INA219_CONFIG_BVOLTAGERANGE_32V |
      INA219_CONFIG_GAIN_8_320MV |
      INA219_CONFIG_BADCRES_12BIT_128S_69MS |
      INA219_CONFIG_SADCRES_12BIT_128S_69MS |
      INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;

    Wire.beginTransmission(0x40);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)(ina219_config >> 8));
    Wire.write((uint8_t)(ina219_config & 0xFF));
    Wire.endTransmission();
  }

  //Init ADS1118
  digitalWrite(ADS_CS_PIN, HIGH);

  SPI.begin();
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE1));

  digitalWrite(ADS_CS_PIN, LOW);
  SPI.transfer16(ADS1118_SINGLE_SHOT_INTERNAL_TEMPERATURE);
  digitalWrite(ADS_CS_PIN, HIGH);
  delay(10);

  digitalWrite(ADS_CS_PIN, LOW);
  status.adc_ic_temp_counts = SPI.transfer16(ADS1118_SINGLE_SHOT_INTERNAL_TEMPERATURE);
  digitalWrite(ADS_CS_PIN, HIGH);
  delay(10);

  //Базове читання ТЕРМОПАРИ: щоб фільтр викидів мав коректний baseline
  //і на старті не було "423°C"
  digitalWrite(ADS_CS_PIN, LOW);
  SPI.transfer16(ADS1118_SINGLE_SHOT_ADC); // запускаємо конверсію жала
  digitalWrite(ADS_CS_PIN, HIGH);
  delay(10);

  digitalWrite(ADS_CS_PIN, LOW);
  status.adc_counts = SPI.transfer16(ADS1118_SINGLE_SHOT_INTERNAL_TEMPERATURE); // читаємо жало,
  digitalWrite(ADS_CS_PIN, HIGH);                                              // і лишаємо запущеною внутрішню

  setupHeaterPwmAndAdcTimer();

  myPID.SetMode(MANUAL);
  myPID.SetSampleTime(PWM_PERIOD_MS);
  myPID.SetOutputLimits(0, MAX_HEATER_DUTY_LIMITED);
  myPID.SetTunings(params.kP, params.kI, params.kD);

  UpdatePowerMeasurement();

  IWatchdog.begin(8000000UL); // 8 с: якщо loop зависне - скидання, нагрівач OFF
  
  // Ініціалізація пищалки
  SetupBuzzer();
}
