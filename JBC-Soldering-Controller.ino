/* Copyright (c) 2017 timothyjager — MIT License.
   Порт на STM32F103C8T6 (Blue Pill). Деталі порту - в коментарях _00/_02. */
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "FreeSans6pt7b.h"
#include <Fonts/FreeSans9pt7b.h>
#include "DSEG7Classic_Bold12pt7b.h"
#include "DSEG7Classic_Bold24pt7b.h"
#include <PID_v1.h>
#include <Adafruit_INA219.h>
#include <IWatchdog.h>
#include "ads1118.h"
