/*
 * Student name: [Omar Ben Omar]
 * Student number: [6564062]
 */

#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/structs/adc.h"
#include "hardware/structs/io_bank0.h"
#include "hardware/structs/pads_bank0.h"
#include "hardware/structs/pwm.h"
#include "hardware/structs/sio.h"
#include "pico/stdlib.h"
#include <stdio.h>

// Definitions
#define PIN_SENSOR_L 26
#define PIN_SENSOR_R 27
#define PIN_MOTOR_A_PWM 18
#define PIN_MOTOR_A_DIR 19
#define PIN_MOTOR_B_PWM 20
#define PIN_MOTOR_B_DIR 21

// Constants 
#define ADC_THRESHOLD 1100
#define PWM_WRAP 25000
#define BASE_SPEED 18750      
#define BOOST_SPEED 22500     
#define TURN_FAST_SPEED 16000
#define TURN_SLOW_SPEED 0     

// Functions
void adc_init_hw() {
  adc_hw->cs = ADC_CS_EN_BITS;
  while (!(adc_hw->cs & ADC_CS_READY_BITS)) {
    tight_loop_contents();
  }
}

void adc_gpio_init_hw(uint pin) {
  // set function to null
  io_bank0_hw->io[pin].ctrl = 0x1f << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // disable pulls / input enable
  pads_bank0_hw->io[pin] =
      (pads_bank0_hw->io[pin] &
       ~(PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_PDE_BITS |
         PADS_BANK0_GPIO0_IE_BITS)) |
      PADS_BANK0_GPIO0_OD_BITS;
}

void dir_gpio_init_hw(uint pin) {
  // set to sio (5)
  io_bank0_hw->io[pin].ctrl = 5 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // set output enable
  sio_hw->gpio_oe_set = (1ul << pin);

  // set output clear
  sio_hw->gpio_clr = (1ul << pin);
}

uint16_t adc_read_hw(uint ch) {
  hw_write_masked(&adc_hw->cs, (ch << ADC_CS_AINSEL_LSB), ADC_CS_AINSEL_BITS);
  hw_set_bits(&adc_hw->cs, ADC_CS_START_ONCE_BITS);
  while (!(adc_hw->cs & ADC_CS_READY_BITS)) {
    tight_loop_contents();
  }
  return (uint16_t)adc_hw->result;
}

void pwm_init_hw(uint pin) {
  // set function to pwm 4
  io_bank0_hw->io[pin].ctrl = 4 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  uint slice = (pin >> 1) & 7;
  uint channel = pin & 1;
  pwm_hw->slice[slice].div = (4 << PWM_CH0_DIV_INT_LSB);
  pwm_hw->slice[slice].top = PWM_WRAP;
  if (channel == 0) {
    hw_write_masked(&pwm_hw->slice[slice].cc, 0, PWM_CH0_CC_A_BITS);
  } else {
    hw_write_masked(&pwm_hw->slice[slice].cc, 0, PWM_CH0_CC_B_BITS);
  }
  pwm_hw->slice[slice].csr |= PWM_CH0_CSR_EN_BITS;
}

void pwm_set_duty_hw(uint pin, uint16_t duty) {
  uint slice = (pin >> 1) & 7;
  uint channel = pin & 1;
  if (duty > PWM_WRAP)
    duty = PWM_WRAP;
  if (channel == 0) {
    hw_write_masked(&pwm_hw->slice[slice].cc, (duty << PWM_CH0_CC_A_LSB),
                    PWM_CH0_CC_A_BITS);
  } else {
    hw_write_masked(&pwm_hw->slice[slice].cc, (duty << PWM_CH0_CC_B_LSB),
                    PWM_CH0_CC_B_BITS);
  }
}

int main() {
  stdio_init_all();
  sleep_ms(2000);

  adc_init_hw();
  adc_gpio_init_hw(PIN_SENSOR_L);
  adc_gpio_init_hw(PIN_SENSOR_R);

  pwm_init_hw(PIN_MOTOR_A_PWM);
  dir_gpio_init_hw(PIN_MOTOR_A_DIR);

  pwm_init_hw(PIN_MOTOR_B_PWM);
  dir_gpio_init_hw(PIN_MOTOR_B_DIR);

  pwm_set_duty_hw(PIN_MOTOR_A_PWM, BOOST_SPEED);
  pwm_set_duty_hw(PIN_MOTOR_B_PWM, BOOST_SPEED);
  sleep_ms(200);

  enum Direction { NONE, LEFT, RIGHT };
  enum Direction last_known_direction = NONE;

  while (true) {
    uint16_t val_l = adc_read_hw(0);
    uint16_t val_r = adc_read_hw(1);

    bool left_is_black = val_l > ADC_THRESHOLD;
    bool right_is_black = val_r > ADC_THRESHOLD;

    printf("L: %d, R: %d -> ", val_l, val_r);

    if (left_is_black && right_is_black) {
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);
    } else if (!left_is_black && !right_is_black) {
      if (last_known_direction == LEFT) {
        // Recover Left
        pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_SLOW_SPEED);
        pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_FAST_SPEED);
      } else if (last_known_direction == RIGHT) {
        // Recover Right
        pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_FAST_SPEED);
        pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SLOW_SPEED);
      } else {
        pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
        pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);
      }
    } else if (left_is_black && !right_is_black) {
      last_known_direction = LEFT;
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_SLOW_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_FAST_SPEED);
    } else if (!left_is_black && right_is_black) {
      last_known_direction = RIGHT;
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_FAST_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SLOW_SPEED);
    }

    sleep_ms(0.3);
  }
}
