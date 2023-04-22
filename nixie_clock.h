#ifndef NIXIE_CLOCK
#define NIXIE_CLOCK

#include <stdio.h>
#include <stdlib.h>
#include "nixie_clock_define.h"
#include "pico/stdlib.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"

//---- Switch mode definition ------------
typedef enum{
    normal,
    fade,
    crossfade,
    cloud,
    dotmove
}SwitchMode; 

//---- Nixie Tube Dynamic-Drive Config parameters ----------
typedef struct
{
    datetime_t time;
    uint16_t switch_counter;
    bool flg_time_update;
    bool flg_change;
    uint8_t slice_num0;
    uint8_t disp_duty[6];
    uint8_t brightness;  

    uint8_t num[6];
    uint8_t next_num[6];
    uint8_t disp_change[6];
    uint8_t brightness_auto;

    SwitchMode switch_mode;

} NixieConfig;

//---- Nixie Tube Dynamic-Drive structure -------------
typedef struct nixietube NixieTube;

struct nixietube
{
    NixieConfig conf;

    //---- initialization -----------
    void (*init)(NixieConfig *conf);

    void (*brightness_inc)(NixieConfig *conf);
    void (*brightness_update)(NixieConfig *conf);
    void (*switch_mode_inc)(NixieConfig *conf);

    // dynamic drive task
    void (*dynamic_display_task)(NixieConfig *conf);
    void (*dynamic_clock_task)(NixieConfig *conf);
    void (*dynamic_setting_task)(NixieConfig *conf, uint8_t setting_num);

    // tick second
    void (*clock_tick)(NixieConfig *conf, datetime_t time);
    //
    void (*switch_update)(NixieConfig *conf, datetime_t time);

    // startup animation
    void (*startup_animation)(NixieConfig *conf);

    // off_animation
    void (*dispoff_animation)(NixieConfig *conf);
};

//---- constructor -------------
NixieTube new_NixieTube(NixieConfig Config);

#endif
