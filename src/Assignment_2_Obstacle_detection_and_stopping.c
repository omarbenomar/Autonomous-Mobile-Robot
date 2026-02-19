/*
* Student name:[Omar Ben Omar]
* Student number:[6564062] 
*/


#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/regs/sio.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"


#define INVERT_MOTOR_A false // false = forward
#define INVERT_MOTOR_B false 
#define ECHO_PIN 8
#define TRIGGER_PIN 9
#define MOTOR_A_PWM 18 
#define MOTOR_B_PWM 20 
#define MOTOR_A_DIR 19
#define MOTOR_B_DIR 21
#define PWM_WRAP 25000
#define TARGET_DUTY 15000
#define DUTY_CYCLE_0 0

// Distance Calculation
#define STOP_DISTANCE_CM 15

// Global variables for ISR 
volatile uint32_t start_time = 0;
volatile uint32_t pulse_duration = 0;
volatile bool new_reading_available = false;

// ISR for Echo Pin 
void gpio_irq_callback(uint gpio, uint32_t events) {
  if (gpio == ECHO_PIN) {
    if (events & GPIO_IRQ_EDGE_RISE) {
      start_time = timer_hw->timelr; //getting start of pulse 
    } else if (events & GPIO_IRQ_EDGE_FALL) {
      uint32_t end_time = timer_hw->timelr; //getting end of pulse 
      if (end_time < start_time) {
        pulse_duration = (0xFFFFFFFF - start_time) + end_time;
      } else {
        pulse_duration = end_time - start_time;
      }
      new_reading_available = true;
    }
  }
}

//Forward/backward
uint16_t calculate_duty_single(uint16_t duty, bool invert) {
  return invert ? (PWM_WRAP - duty) : duty;
}

int main() {
  stdio_init_all();

  //Initialize motor + sensor

  // Sensor
  gpio_init(TRIGGER_PIN);
  gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
  gpio_put(TRIGGER_PIN, 0);
  gpio_init(ECHO_PIN);
  gpio_set_dir(ECHO_PIN, GPIO_IN);

  // Motor 
  gpio_set_function(MOTOR_A_PWM, GPIO_FUNC_PWM);
  gpio_set_function(MOTOR_B_PWM, GPIO_FUNC_PWM);
  gpio_init(MOTOR_A_DIR);
  gpio_init(MOTOR_B_DIR);
  
  sio_hw->gpio_oe_set = (1 << MOTOR_A_DIR) | (1 << MOTOR_B_DIR);

  uint32_t dir_set_mask = 0;
  uint32_t dir_clr_mask = 0;

  if (INVERT_MOTOR_A)
    dir_set_mask |= (1 << MOTOR_A_DIR);
  else
    dir_clr_mask |= (1 << MOTOR_A_DIR);

  if (INVERT_MOTOR_B)
    dir_set_mask |= (1 << MOTOR_B_DIR);
  else
    dir_clr_mask |= (1 << MOTOR_B_DIR);

  sio_hw->gpio_set = dir_set_mask;
  sio_hw->gpio_clr = dir_clr_mask;

  //Pwm for slices
  uint slice_a = pwm_gpio_to_slice_num(MOTOR_A_PWM);
  uint slice_b = pwm_gpio_to_slice_num(MOTOR_B_PWM);

  pwm_config config = pwm_get_default_config();
  pwm_config_set_wrap(&config, PWM_WRAP);
  pwm_config_set_clkdiv(&config, 50.0f);

  // init Slices
  pwm_init(slice_a, &config, true);
  pwm_init(slice_b, &config, true);

  // Intterupt
  gpio_set_irq_enabled_with_callback(ECHO_PIN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &gpio_irq_callback);


  pwm_set_chan_level(slice_a, PWM_CHAN_A,
                     calculate_duty_single(TARGET_DUTY, INVERT_MOTOR_A));
  pwm_set_chan_level(slice_b, PWM_CHAN_A,
                     calculate_duty_single(TARGET_DUTY, INVERT_MOTOR_B));

  while (true) {
    //ultrasonic
    gpio_put(TRIGGER_PIN, 1);
    sleep_us(10);
    gpio_put(TRIGGER_PIN, 0);

    sleep_ms(60);

    if (new_reading_available) {
      uint32_t local_pulse_duration;
      uint32_t status = save_and_disable_interrupts();
      local_pulse_duration = pulse_duration;
      new_reading_available = false; // 
      restore_interrupts(status);
      float distance = (local_pulse_duration * 0.0343) / 2.0;

      // Motor control
      if (distance < STOP_DISTANCE_CM) {
        // Stop
        pwm_set_chan_level(slice_a, PWM_CHAN_A,
                           calculate_duty_single(0, INVERT_MOTOR_A));
        pwm_set_chan_level(slice_b, PWM_CHAN_A,
                           calculate_duty_single(0, INVERT_MOTOR_B));
      } else {
        // Work
        pwm_set_chan_level(slice_a, PWM_CHAN_A,
                           calculate_duty_single(TARGET_DUTY, INVERT_MOTOR_A));
        pwm_set_chan_level(slice_b, PWM_CHAN_A,
                           calculate_duty_single(TARGET_DUTY, INVERT_MOTOR_B));
      }
    }
  }
}