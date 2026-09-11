/* Copyright (c) 2017 timothyjager
   JBC-Soldering-Controller
   MIT License. See LICENSE file for details.

   Типи структур status_struct/system_parameters_struct винесені в окремий
   .h просто для охайності (спільні типи, на які посилаються кілька .ino-
   файлів) - окремого stm32_it.cpp у проєкті більше немає (перейшли на
   HardwareTimer, див. коментар у _02_interrupts.ino), тож це вже не строга
   вимога компіляції, а просто організаційна зручність.
*/
#ifndef PROJECT_TYPES_H
#define PROJECT_TYPES_H

#include <Arduino.h>

//System Parameters Data Structure
typedef struct {
  byte  pid_mode;          //PID mode - Automatic=1, Manual=0 (ПОХІДНЕ значення -
                            //рахується щоцикл в loop() з heater_enabled + CRADLE_SENSOR,
                            //не встановлюється напряму кнопкою/підставкою)
  byte  heater_enabled;    //БАЖАННЯ користувача (чи хоче він, щоб паяльник грів) -
                            //перемикається кнопкою енкодера. На відміну від pid_mode,
                            //підставка це поле НЕ чіпає - лише тимчасово форсить
                            //pid_mode=MANUAL, поки паяльник у підставці. Завдяки
                            //цьому після зняття з підставки нагрів коректно
                            //відновлюється, якщо користувач раніше його вмикав.
  byte  simulate_input;     //this allows us to override the actual input (temperature reading) using the tuning app
  int16_t idle_temp_c;
  int16_t output_override;
  float setpoint;
  float kP;
  float kI;
  float kD;
  float simulated_input;
} system_parameters_struct;

//Status Variables Struct - hold global status values
typedef struct {
  byte gpio_port_b;                //STM32: молодший байт GPIOA->IDR  (було PINB на AVR)
  byte gpio_port_c;                //STM32: старший байт GPIOA->IDR   (було PINC на AVR)
  byte gpio_port_d;                //STM32: молодший байт GPIOB->IDR  (було PIND на AVR)
  byte gpio_port_e;                //STM32: старший байт GPIOB->IDR   (було PINE на AVR)
  int16_t encoder_pos;             //Enocder Position
  int16_t adapter_voltage_mv;      //Input power adapter voltage in millivolts
  int16_t adc_counts;              //ADC value read by ADS1118
  int16_t adc_ic_temp_counts;      //internal temp of ADS1118
  int16_t current_sense_ma;        //current sense in milliamps (перейменовано з current_sense_mv - у оригіналі
                                    //поле звалось "_mv", хоча коментар і зміст завжди були міліамперами)
  double pid_setpoint;             //setpoint of the PID loop      (double: сумісність з PID_v1, див. примітку в _00_globals.ino)
  double tip_temperature_c;        //input value of the PID loop   (double: сумісність з PID_v1, див. примітку в _00_globals.ino)
  double pid_output;              //computed output value of the PID loop (double: сумісність з PID_v1, див. примітку в _00_globals.ino)
} status_struct;

#endif // PROJECT_TYPES_H
