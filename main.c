//-------------------------------------------------------
// main.c
//-------------------------------------------------------
// OENXC-6 (Okomeya-Electronics NiXie-tube Clock Mark.VI) main program
// 
// Written by jp7dki
//-------------------------------------------------------

//-------------------------------------------------------
//- Header files include
//-------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include "nixie_clock.h"
#include "gps.h"
#include "pico/stdlib.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/rtc.h"
#include "hardware/irq.h"
#include "hardware/flash.h"
#include "hardware/resets.h"
#include "hardware/clocks.h"
#include "hardware/xosc.h"
#include "pico/util/datetime.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"

//-------------------------------------------------------
//- Macro Define 
//-------------------------------------------------------


// UART0(DEBUG) 
#define BAUD_RATE 115200

#define NUM_TASK 10
#define SETTING_MAX_NUM 10

// PINK filter
#define MAX_Z 16

//-------------------------------------------------------
//- Global Variable
//-------------------------------------------------------
uint8_t count;

bool flg_time_correct=false;
bool flg_pps_received=false;

uint16_t pps_led_counter=0;
uint16_t blink_counter[6] = {0,0,0,0,0,0};
uint8_t cursor;
uint8_t setting_num=1;
// setting parameters
uint8_t param_auto_onoff = 0;
uint8_t param_off_time_hour = 7;       // default off time = 22:00
uint8_t param_off_time_min = 16;
uint8_t param_on_time_hour = 6;         // defualt on time = 6:00
uint8_t param_on_time_min = 0;
uint16_t fluctuation_level=0;

bool flg_off = true;
bool flg_on = true;

// task list of delay execution.
typedef void (*func_ptr)(void);
struct TaskList{
    func_ptr func;
    uint16_t delay_10ms;
} task_list[NUM_TASK];

enum OperationMode{
    power_on,
    power_up_animation,
    clock_display,
    settings,
    time_adjust,
    random_disp,
    demo,
    onoff_animation
} operation_mode;

NixieTube nixie_tube;
NixieConfig nixie_conf;

Gps gps;
GpsConfig gps_conf;

//-------------------------------------------------------
//- Function Prototyping
//-------------------------------------------------------
void hardware_init(void);
void delay_execution(void (*func_ptr)(void), uint16_t delay_ms);

bool task_add(func_ptr func, uint16_t delay_10ms);

void check_button(uint8_t SW_PIN, void (*short_func)(void), void (*long_func)(void));
void swa_short_push(void);
void swa_long_push(void);
void swb_short_push(void);
void swb_long_push(void);
void swc_short_push(void);
void swc_long_push(void);
/*
void init_pink(void);
float pinkfilter(float in);
*/

//-------------------------------------------------------
//- IRQ
//-------------------------------------------------------
//---- timer_alarm0 : 1秒ごとの割り込み ----
static void timer_alarm0_irq(void) {
    datetime_t time;
    uint8_t i;

    // Clear the irq
    hw_clear_bits(&timer_hw->intr, 1u << 0);
    uint64_t target = timer_hw->timerawl + 999999; // interval 1s
    timer_hw->alarm[0] = (uint32_t)target;


    // 毎分0秒に消灯・点灯の確認をする
    if((time.sec==0) && (param_auto_onoff==1)){
        if((time.hour==param_off_time_hour) && (time.min==param_off_time_min)){
            // 自動消灯
            flg_off = true;
            operation_mode = onoff_animation;
        }

        if((time.hour==param_on_time_hour) && (time.min==param_on_time_min)){
            // 自動点灯
            flg_on = true;
            operation_mode = onoff_animation;
        }
    }

    rtc_get_datetime(&time);
    // 毎時0分0秒に時刻合わせを行う
    if((time.sec==0) && (time.min==0)){
        flg_time_correct = false;
    }

    if(operation_mode==clock_display){
        nixie_tube.clock_tick(&nixie_conf, time);
    }
}

//---- timer_alarm1 : 10msごとの割り込み ----
static void timer_alerm1_irq(void) {
    uint8_t i;
    datetime_t time;
    // Clear the irq
    hw_clear_bits(&timer_hw->intr, 1u << 1);
    uint64_t target = timer_hw->timerawl + 10000; // interval 40ms
    timer_hw->alarm[1] = (uint32_t)target;

    // Delay task
    for(i=0;i<NUM_TASK;i++){
        if(task_list[i].delay_10ms != 0){
            task_list[i].delay_10ms--;
            if(task_list[i].delay_10ms == 0){
                task_list[i].func();
            }
        }
    }

    // 1PPS LED 
    if(pps_led_counter!=0){
        pps_led_counter--;
        if(pps_led_counter==0){
            gps.pps_led_off();
        }
    }

    // blink
    for(i=0;i<6;i++){
        if(blink_counter[i]!=0){
            blink_counter[i]--;
        }
    }

    rtc_get_datetime(&time);
    nixie_tube.switch_update(&nixie_conf, time);

    // Nixie-Tube brightness(anode current) update
    nixie_tube.brightness_update(&nixie_conf);
}



//---------------------------------------------------------
//- core1 entry
//---------------------------------------------------------
// core1はニキシー管の表示処理のみ行う。
void core1_entry(){
    uint8_t i,j;

    while(1){
        switch(operation_mode){

            // Power up animation
            case power_up_animation:
            case onoff_animation:

                nixie_tube.dynamic_display_task(&nixie_conf);
                break;


            // Clock display
            case clock_display:

                nixie_tube.dynamic_clock_task(&nixie_conf);
                break;
            
            // Setting display
            case settings:

                nixie_tube.dynamic_setting_task(&nixie_conf, setting_num);
                break;

            defalut:
                break;    
        }

    }
}

//---- on_uart_rx : GPSの受信割り込み ---------------------
static irq_handler_t on_uart_rx(){
    int i;
    uint8_t ch;

    // GPSの受信処理 GPRMCのみ処理する
    while(uart_is_readable(UART_GPS)){
        ch = uart_getc(UART_GPS);

        if(gps.receive(&gps_conf, ch)){
            if(flg_pps_received==true){
                // RTCをリセットしておく
                reset_block(RESETS_RESET_RTC_BITS);
                unreset_block_wait(RESETS_RESET_RTC_BITS);
                rtc_init();

                datetime_t t = gps_conf.gps_datetime;

                rtc_set_datetime(&t);
                flg_pps_received=false;
            }
        }
    }
}

//---- GPIO割り込み(1PPS) ----
static irq_handler_t gpio_callback(uint gpio, uint32_t event){
    datetime_t time;
    uint8_t i;

    if(flg_time_correct==false){
        uint64_t target = timer_hw->timerawl + 999999; // interval 1s
        timer_hw->alarm[0] = (uint32_t)target;
        flg_time_correct=true;
        flg_pps_received=true;
    }

    if(operation_mode==clock_display){
        // 1PPS_LED ON (200ms)
        gps.pps_led_on();
        task_add(gps.pps_led_off, 20);
    }

}

//---------------------------------------------------------
//- main function
//---------------------------------------------------------
// 
int main(){

    uint16_t i,j;
    uint16_t count_sw;
    datetime_t time;
    uint64_t target;

    // task initialization
    for(i=0;i<NUM_TASK;i++){
        task_list[i].delay_10ms = 0;
    }

    // mode initialization
    operation_mode = clock_display;

    bi_decl(bi_program_description("This is a test program for nixie6."));

    hardware_init();

    nixie_tube = new_NixieTube(nixie_conf);
    nixie_tube.init(&nixie_conf);

    gps = new_Gps(gps_conf);
    gps.init(&gps_conf, on_uart_rx, gpio_callback);

    // Timer Settings
    hw_set_bits(&timer_hw->inte, 1u<<0);        // Alarm0
    irq_set_exclusive_handler(TIMER_IRQ_0, timer_alarm0_irq);
    irq_set_enabled(TIMER_IRQ_0, true);
    target = timer_hw->timerawl + 1000000; // interval 1s
    timer_hw->alarm[0] = (uint32_t)target;

    hw_set_bits(&timer_hw->inte, 1u<<1);        // Alarm1
    irq_set_exclusive_handler(TIMER_IRQ_1, timer_alerm1_irq);
    irq_set_enabled(TIMER_IRQ_1, true);
    target = timer_hw->timerawl + 10000;   // interval 10ms
    timer_hw->alarm[1] = (uint32_t)target;

    count = 0;

    // Core1 Task 
    multicore_launch_core1(core1_entry);

    // Power-Up-Animation
    operation_mode = power_up_animation;
    
    nixie_tube.startup_animation(&nixie_conf);

    // Clock Display mode
    operation_mode = clock_display;

    while (1) {
        if(operation_mode==onoff_animation){
            if(flg_off){
                // off_animation
                nixie_tube.dispoff_animation(&nixie_conf);
                flg_off = false;
            }

            if(flg_on){

            }
        }

        //---- SWA -----------------------
        check_button(SWA_PIN, swa_short_push, swa_long_push);

        //---- SWB -----------------------
        check_button(SWB_PIN, swb_short_push, swb_long_push);

        //---- SWC -----------------------
        check_button(SWC_PIN, swc_short_push, swc_long_push);

    }
}


//-------------------------------------------------
// Hardware initialization
//-------------------------------------------------
void hardware_init(void)
{   
    gpio_init(DBG_TX_PIN);
    gpio_init(DBG_RX_PIN);
    gpio_set_dir(DBG_TX_PIN, GPIO_OUT);
    gpio_set_dir(DBG_RX_PIN, GPIO_IN);
    gpio_put(DBG_TX_PIN,0);
    gpio_set_function(DBG_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(DBG_RX_PIN, GPIO_FUNC_UART);

    gpio_init(SWA_PIN);
    gpio_init(SWB_PIN);
    gpio_init(SWC_PIN);
    gpio_set_dir(SWA_PIN, GPIO_IN);
    gpio_set_dir(SWB_PIN, GPIO_IN);
    gpio_set_dir(SWC_PIN, GPIO_IN);
    gpio_pull_up(SWA_PIN);
    gpio_pull_up(SWB_PIN);
    gpio_pull_up(SWC_PIN);

    gpio_init(DBGLED_PIN);
    gpio_set_dir(DBGLED_PIN, GPIO_OUT);
    gpio_put(DBGLED_PIN,0);

    //---- UART ----------------
    // UART INIT(Debug)
    uart_init(UART_DEBUG, 2400);
    int __unused actual = uart_set_baudrate(UART_DEBUG, BAUD_RATE);
    uart_set_hw_flow(UART_DEBUG, false, false);
    uart_set_format(UART_DEBUG, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_DEBUG, false);
    uart_set_irq_enables(UART_DEBUG, false, false);

    //---- RTC ------------------
    datetime_t t = {
        .year = 2023,
        .month = 03,
        .day = 18,
        .dotw = 1,
        .hour = 12,
        .min = 34,
        .sec = 56
    };

    rtc_init();
    rtc_set_datetime(&t);

    // Alarm one a minute
    datetime_t alarm = {
        .year = -1,
        .month = -1,
        .day = -1,
        .dotw = -1,
        .hour = -1,
        .min = -1,
        .sec = 01
    };
//    rtc_set_alarm(&alarm, alarm_callback);    
}

//----------------------------------
// delayed task
//----------------------------------
bool task_add(func_ptr func, uint16_t delay_10ms){
    uint8_t i;

    i = 0;
    while((task_list[i].delay_10ms != 0) && (i<NUM_TASK)){
        i++;
    }

    if(i==NUM_TASK){
        return false;
    }else{
        task_list[i].func = func;
        task_list[i].delay_10ms = delay_10ms;
        return true;
    }

}

//---- switch check (only use main function) -------------
void check_button(uint8_t SW_PIN, void (*short_func)(void), void (*long_func)(void))
{
    uint16_t count_sw = 0;

    if(!gpio_get(SW_PIN)){
        sleep_ms(100);
        while((!gpio_get(SW_PIN)) && (count_sw<200)){
            count_sw++;
            sleep_ms(10);
        }

        if(count_sw==200){
            // Long-push
            long_func();
        }else{
            // Short-push
            short_func();
        }

        while(!gpio_get(SW_PIN));
    }
}

void swa_short_push(void)
{
    switch(operation_mode){
        case clock_display:

            break;
        case settings:

            break;
    }
}

void swa_long_push(void)
{
    switch(operation_mode){
        case clock_display:
            operation_mode = settings;
            break;
        case settings:
            operation_mode = clock_display;
            break;
    }
}

void swb_short_push(void)
{
    switch(setting_num){
        case 1:
            // Switching mode 
            nixie_tube.switch_mode_inc(&nixie_conf);
            break;
        case 2:
            // Brightness setting
            nixie_tube.brightness_inc(&nixie_conf);
            break;
        case 3:
            // Brightness auto setting
            if(nixie_tube.conf.brightness_auto==0){
                nixie_tube.conf.brightness_auto=1;
            }else{
                nixie_tube.conf.brightness_auto=0;
            }
            break;
        case 4:
            // Auto on/off setting

            break;
        case 5:
            // Auto on time

            break;
        case 6:
            // Auto off time

            break;
        case 7:
            // Jetlag setting 

            break;
        case 8:
            // GPS time correction on/off

            break;
        case 9:
            // LED setting
            
            break;
        case 10:
            // 1/f fraction setting

            break;
        default:
            break;

    }
}

void swb_long_push(void)
{
    gpio_put(DBGLED_PIN,1);
}

void swc_short_push(void)
{
    switch(operation_mode){
        case clock_display:

            break;
        case settings:
            setting_num++;
            if(setting_num > SETTING_MAX_NUM){
                setting_num = 1;
            }
            break;
    }
}

void swc_long_push(void)
{
    gpio_put(DBGLED_PIN,1);    
}

/*
//---- pink filter -----------------------
void init_pink(void) {
    extern float   z[MAX_Z];
    extern float   k[MAX_Z];
    int             i;

    for (i = 0; i < MAX_Z; i++)
        z[i] = 0;
    k[MAX_Z - 1] = 0.5;
    for (i = MAX_Z - 1; i > 0; i--)
        k[i - 1] = k[i] * 0.25;
}

float pinkfilter(float in) {
    extern float   z[MAX_Z];
    extern float   k[MAX_Z];
    static float   t = 0.0;
    float          q;
    int             i;

    q = in;
    for (i = 0; i < MAX_Z; i++) {
        z[i] = (q * k[i] + z[i] * (1.0 - k[i]));
        q = (q + z[i]) * 0.5;
    }
    return (t = 0.75 * q + 0.25 * t); 
} */
