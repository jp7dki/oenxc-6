#ifndef NIXIE_CLOCK
#define NIXIE_CLOCK

#include <stdio.h>
#include <stdlib.h>
#include "nixie_clock_define.hpp"
#include "pico/stdlib.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

// Nixie-tube
class NixieTube
{
private:
    datetime_t time;
    uint16_t switch_counter;
    bool flg_time_update;
    bool flg_change;
    uint8_t slice_num0;
    uint8_t disp_duty[6];
    uint8_t brightness;
    const uint8_t cathode_port[10] = {
        K0_PIN,
        K1_PIN,
        K2_PIN,
        K3_PIN,
        K4_PIN,
        K5_PIN,
        K6_PIN,
        K7_PIN,
        K8_PIN,
        K9_PIN
    };
    const uint8_t anode_port[6] = {
        DIGIT1_PIN,
        DIGIT2_PIN,
        DIGIT3_PIN,
        DIGIT4_PIN,
        DIGIT5_PIN,
        DIGIT6_PIN
    };

    void disp_num(uint8_t num, uint8_t digit);

public:
    uint8_t num[6];
    uint8_t next_num[6];
    uint8_t disp_change[6];
    uint8_t brightness_auto;
    enum SwitchMode{
        normal,
        fade,
        crossfade,
        cloud,
        dotmove
    } switch_mode;

    NixieTube();
    ~NixieTube();

    void disp(uint8_t digit);
    void disp_next(uint8_t digit);
    void disp_blank(void);
    void check_change_num(void);
    void brightness_inc(void);
    uint8_t get_brightness(void);
    void brightness_update(void);
    void switch_mode_inc(void);

    // dynamic drive task
    void dynamic_display_task(void);
    void dynamic_clock_task(void);
    void dynamic_setting_task(uint8_t setting_num);

    // tick second
    void clock_tick(datetime_t time);

    //
    void switch_update(datetime_t time);

    // startup animation
    void startup_animation(void);

    // off_animation
    void dispoff_animation(void);
};

#endif

