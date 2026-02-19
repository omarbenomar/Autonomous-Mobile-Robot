/*
* Student name:[Omar Ben Omar]
* Student number:[6564062] 
*/


//Imports
#include "hardware/address_mapped.h" 
#include "hardware/gpio.h"           
#include "hardware/pwm.h"
#include "hardware/structs/io_bank0.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/timer.h"
#include "pico/stdlib.h" 
#include <stdio.h>

// Pins
#define MOTOR_A_PWM_PIN 18 
#define MOTOR_A_DIR_PIN 19 
#define MOTOR_B_PWM_PIN 20 
#define MOTOR_B_DIR_PIN 21 

#define PWM_WRAP 25000 


 //Returning time from RP2040 hardware timer
static inline uint64_t get_time_us64() {
  uint32_t lo, hi;
  do {
    hi = timer_hw->timehr;
    lo = timer_hw->timelr;
  } while (hi != timer_hw->timehr);
  return ((uint64_t)hi << 32) | lo;
}


 //Pauses for given time
void delay_ms(uint32_t ms) {
  uint64_t target_time = get_time_us64() + (uint64_t)ms * 1000;
  while (get_time_us64() < target_time) {
  }
}



int main() {
  
  stdio_init_all();
  delay_ms(2000); 


  // GPIO Configuration using Direct Register Writes 
  // Set direction pins (GP19, GP21) to SIO 
  hw_write_masked(&io_bank0_hw->io[MOTOR_A_DIR_PIN].ctrl, 5, 0x1f);
  hw_write_masked(&io_bank0_hw->io[MOTOR_B_DIR_PIN].ctrl, 5, 0x1f);

  // Set for GP19, GP21 to output by setting bits in SIO OE
  sio_hw->gpio_oe_set = (1u << MOTOR_A_DIR_PIN) | (1u << MOTOR_B_DIR_PIN);

  // Set output level to LOW
  hw_clear_bits(&sio_hw->gpio_out,
                (1u << MOTOR_A_DIR_PIN) | (1u << MOTOR_B_DIR_PIN));

  // Set for PWM pins (GP18, GP20) to PWM
  hw_write_masked(&io_bank0_hw->io[MOTOR_A_PWM_PIN].ctrl, 4, 0x1f);
  hw_write_masked(&io_bank0_hw->io[MOTOR_B_PWM_PIN].ctrl, 4, 0x1f);

  // --- PWM Configuration using pwm_hw struct ---
  uint slice_a = pwm_gpio_to_slice_num(MOTOR_A_PWM_PIN); //Motor A
  uint slice_b = pwm_gpio_to_slice_num(MOTOR_B_PWM_PIN); //Motor B

  pwm_hw_t *pwm = pwm_hw;

  // Config Slice A
  pwm->slice[slice_a].csr = 0;
  pwm->slice[slice_a].div = 125 << PWM_CH0_DIV_INT_LSB;
  pwm->slice[slice_a].top = PWM_WRAP;
  pwm->slice[slice_a].csr = PWM_CH0_CSR_EN_BITS;

  // Config Slice B
  pwm->slice[slice_b].csr = 0;
  pwm->slice[slice_b].div = 125 << PWM_CH0_DIV_INT_LSB;
  pwm->slice[slice_b].top = PWM_WRAP;
  pwm->slice[slice_b].csr = PWM_CH0_CSR_EN_BITS;

  while (1) {
    // Constant speed (60% duty) for 4 seconds
    uint16_t duty_60 = PWM_WRAP * 0.60f;

    hw_write_masked(&pwm->slice[slice_a].cc, duty_60, PWM_CH0_CC_A_BITS);
    hw_write_masked(&pwm->slice[slice_b].cc, duty_60, PWM_CH0_CC_A_BITS);

    delay_ms(4000);

    // Increased speed (90% duty) for 4 seconds 
    uint16_t duty_90 = PWM_WRAP * 0.90f;

    hw_write_masked(&pwm->slice[slice_a].cc, duty_90, PWM_CH0_CC_A_BITS);
    hw_write_masked(&pwm->slice[slice_b].cc, duty_90, PWM_CH0_CC_A_BITS);
    
    delay_ms(4000);

    // Slow down from 90% to 0% over 4.5 seconds

    for (int i = 90; i >= 0; i--) {
      uint16_t duty = PWM_WRAP * (i / 100.0f);
      hw_write_masked(&pwm->slice[slice_a].cc, duty, PWM_CH0_CC_A_BITS);
      hw_write_masked(&pwm->slice[slice_b].cc, duty, PWM_CH0_CC_A_BITS);
      delay_ms(50);
    }

    hw_write_masked(&pwm->slice[slice_a].cc, 0, PWM_CH0_CC_A_BITS);
    hw_write_masked(&pwm->slice[slice_b].cc, 0, PWM_CH0_CC_A_BITS);
    delay_ms(2000);
  }
  return 0;
}