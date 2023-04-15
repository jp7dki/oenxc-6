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
#include "nixie_clock.hpp"
#include "pico/stdlib.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
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
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

// UART1(GPS)
#define BAUD_RATE_GPS 9600

#define NUM_TASK 10
#define SETTING_MAX_NUM 10

// PINK filter
#define MAX_Z 16

//-------------------------------------------------------
//- Global Variable
//-------------------------------------------------------
uint8_t count;

uint8_t disp_duty[6];

uint16_t rx_counter;
uint16_t rx_sentence_counter;
uint8_t gps_time[16];
uint8_t gps_date[16];
uint8_t gps_valid=0;
bool flg_time_correct=false;
bool flg_time_update=false;
bool flg_change=false;
const uint8_t GPS_HEADER[7] = {'$','G','P','R','M','C',','};

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

enum SwitchMode{
    normal,
    fade,
    crossfade,
    cloud,
    dotmove
};

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

//-------------------------------------------------------
//- Function Prototyping
//-------------------------------------------------------
void hardware_init(void);
void delay_execution(void (*func_ptr)(void), uint16_t delay_ms);
void gps_receive(char char_recv);

bool task_add(func_ptr func, uint16_t delay_10ms);
void pps_led_off(void);
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

    if(operation_mode==clock_display){
        rtc_get_datetime(&time);
        nixie_tube.clock_tick(time);
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
            gpio_put(PPSLED_PIN, 0);
        }
    }

    // blink
    for(i=0;i<6;i++){
        if(blink_counter[i]!=0){
            blink_counter[i]--;
        }
    }

    rtc_get_datetime(&time);
    nixie_tube.switch_update(time);

    // Nixie-Tube brightness(anode current) update
    nixie_tube.brightness_update();
}

//---- GPIO割り込み(1PPS) ----
void gpio_callback(uint gpio, uint32_t event){
    datetime_t time;
    uint8_t i;

    if(flg_time_correct==false){
        uint64_t target = timer_hw->timerawl + 1000000; // interval 1s
        timer_hw->alarm[0] = (uint32_t)target;
        flg_time_correct=true;
        flg_time_update=true;
    }

    if(operation_mode==clock_display){
        // 1PPS_LED ON (200ms)
        gpio_put(PPSLED_PIN, 1);
        task_add(pps_led_off, 20);
    }

}

//---- uart_rx : GPSの受信割り込み --------------------
void on_uart_rx(){
    int i;
    uint8_t ch;

    // GPSの受信処理 GPRMCのみ処理する
    while(uart_is_readable(UART_GPS)){
        ch = uart_getc(UART_GPS);

        gps_receive(ch);
    }
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

                nixie_tube.dynamic_display_task();
                break;


            // Clock display
            case clock_display:

                nixie_tube.dynamic_clock_task();
                break;
            
            // Setting display
            case settings:

                nixie_tube.dynamic_setting_task(setting_num);
                break;

            defalut:
                break;    
        }

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

    // GPIO interrupt setting(1PPS)
    gpio_set_irq_enabled_with_callback(PPS_PIN, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

    // Power-Up-Animation
    operation_mode = power_up_animation;
    
    nixie_tube.startup_animation();

    // Clock Display mode
    operation_mode = clock_display;

    while (1) {
        if(operation_mode==onoff_animation){
            if(flg_off){
                // off_animation
                nixie_tube.dispoff_animation();
                flg_off = false;
            }

            if(flg_on){

            }
        }

        //---- SWA -----------------------
        if(!gpio_get(SWA_PIN)){
            count_sw = 0;
            sleep_ms(100);
            while((!gpio_get(SWA_PIN)) && (count_sw<200)){
                count_sw++;
                sleep_ms(10);
            }

            if(count_sw==200){
                // Long-push
                switch(operation_mode){
                    case clock_display:
                        operation_mode = settings;
                        break;
                    case settings:
                        operation_mode = clock_display;
                        break;
                }
            }else{
                // Short-push
                switch(operation_mode){
                    case clock_display:

                        break;
                    case settings:

                        break;
                }
            }

            while(!gpio_get(SWA_PIN));
        }
        

        //---- SWB -----------------------
        if(!gpio_get(SWB_PIN)){
            count_sw = 0;
            sleep_ms(100);
            while((!gpio_get(SWB_PIN)) && (count_sw<200)){
                count_sw++;
                sleep_ms(10);
            }

            if(count_sw==200){
                // Long-push
                gpio_put(DBGLED_PIN,1);
            }else{
                // Short-push
                switch(operation_mode){
                    case clock_display:

                        break;
                    case settings:

                        switch(setting_num){
                            case 1:
                                // Switching mode 
                                nixie_tube.switch_mode_inc();
                                break;
                            case 2:
                                // Brightness setting
                                nixie_tube.brightness_inc();
                                break;
                            case 3:
                                // Brightness auto setting
                                if(nixie_tube.brightness_auto==0){
                                    nixie_tube.brightness_auto=1;
                                }else{
                                    nixie_tube.brightness_auto=0;
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
                        break;
                }
            }

            while(!gpio_get(SWB_PIN));
        }


        //---- SWC -----------------------
        if(!gpio_get(SWC_PIN)){
            count_sw = 0;
            sleep_ms(100);
            while((!gpio_get(SWC_PIN)) && (count_sw<200)){
                count_sw++;
                sleep_ms(10);
            }

            if(count_sw==200){
                // Long-push
                gpio_put(DBGLED_PIN,1);
            }else{
                // Short-push
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

            while(!gpio_get(SWC_PIN));
        }

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

    gpio_init(DIGIT1_PIN);
    gpio_init(DIGIT2_PIN);
    gpio_init(DIGIT3_PIN);
    gpio_init(DIGIT4_PIN);
    gpio_init(DIGIT5_PIN);
    gpio_init(DIGIT6_PIN);
    gpio_set_dir(DIGIT1_PIN, GPIO_OUT);
    gpio_set_dir(DIGIT2_PIN, GPIO_OUT);
    gpio_set_dir(DIGIT3_PIN, GPIO_OUT);
    gpio_set_dir(DIGIT4_PIN, GPIO_OUT);
    gpio_set_dir(DIGIT5_PIN, GPIO_OUT);
    gpio_set_dir(DIGIT6_PIN, GPIO_OUT);
    gpio_put(DIGIT1_PIN,0);
    gpio_put(DIGIT2_PIN,0);
    gpio_put(DIGIT3_PIN,0);
    gpio_put(DIGIT4_PIN,0);
    gpio_put(DIGIT5_PIN,0);
    gpio_put(DIGIT6_PIN,0);

    gpio_init(PPS_PIN);
    gpio_init(GPS_TX_PIN);
    gpio_set_dir(PPS_PIN, GPIO_IN);
    gpio_set_dir(GPS_TX_PIN, GPIO_IN);
    gpio_set_function(GPS_TX_PIN, GPIO_FUNC_UART);

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
    gpio_init(PPSLED_PIN);
    gpio_set_dir(DBGLED_PIN, GPIO_OUT);
    gpio_set_dir(PPSLED_PIN, GPIO_OUT);
    gpio_put(DBGLED_PIN,0);
    gpio_put(PPSLED_PIN,0);

    //---- UART ----------------
    // UART INIT(Debug)
    uart_init(UART_DEBUG, 2400);
    int __unused actual = uart_set_baudrate(UART_DEBUG, BAUD_RATE);
    uart_set_hw_flow(UART_DEBUG, false, false);
    uart_set_format(UART_DEBUG, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_DEBUG, false);
    uart_set_irq_enables(UART_DEBUG, false, false);

    // UART INIT(GPS)
    uart_init(UART_GPS, 9600);
    int __unused actual1 = uart_set_baudrate(UART_GPS, BAUD_RATE_GPS);
    uart_set_hw_flow(UART_GPS, false, false);
    uart_set_format(UART_GPS, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_GPS, false);
    int UART_IRQ = UART_GPS == uart0 ? UART0_IRQ : UART1_IRQ;

    uart_set_irq_enables(UART_GPS, true, false);
    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);

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


void gps_receive(char char_recv)
{
    uint8_t hour,min,sec;

    switch(rx_sentence_counter){
    case 0:
        // $GPRMC待ち
        if(GPS_HEADER[rx_counter]==char_recv){
            rx_counter++;
        }else{
            rx_counter=0;
        }

        if(rx_counter==7){
            rx_sentence_counter++;
            rx_counter=0;
        }
        break;
    case 1:
        // 時刻取得
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;
        }else{
            gps_time[rx_counter]=char_recv;
            rx_counter++;
        }
        break;
    case 2:
        // Status取得
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;
        }else{
            gps_valid=char_recv;
            rx_counter++;
        }
        break;
    case 3:
        // 緯度
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;
        }else{
            // 読み捨て
            rx_counter++;
        }
        break;
    case 4:
        // 北緯or南緯
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;
        }else{
            // 読み捨て
            rx_counter++;
        }
        break;
    case 5:
        // 経度
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;
        }else{
            // 読み捨て 
            rx_counter++;
        }
        break;
    case 6:
        // 東経or西経
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;
        }else{
            // 読み捨て
            rx_counter++;
        }
        break;
    case 7:
        // 地表における移動の速度
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;
        }else{
            // 読み捨て
            rx_counter++;
        }
        break;
    case 8:
        // 地表における移動の真方位
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;
        }else{
            // 読み捨て
            rx_counter++;
        }
        break;
    case 9:
        // 時刻取得
        if(char_recv==','){
            rx_sentence_counter++;
            rx_counter=0;

            if((gps_valid=='A') && (flg_time_update==true)){
                hour = (gps_time[0]-48)*10+(gps_time[1]-48);
                min = (gps_time[2]-48)*10+(gps_time[3]-48);
                sec = (gps_time[4]-48)*10+(gps_time[5]-48);

                // JSTへの補正
                if(hour>14){
                    hour = hour + 9 -24;
                }else{
                    hour = hour + 9;
                }                        
                
                datetime_t t = {
                    .year = 2000+(gps_date[4]-48)*10+(gps_date[5]-48),
                    .month = (gps_date[2]-48)*10+(gps_date[3]-48),
                    .day = (gps_date[0]-48)*10+(gps_date[1]-48),
                    .dotw = 1,
                    .hour = hour,
                    .min = min,
                    .sec = sec
                };

                // RTCをリセットしておく
                reset_block(RESETS_RESET_RTC_BITS);
                unreset_block_wait(RESETS_RESET_RTC_BITS);
                rtc_init();

                rtc_set_datetime(&t);

                flg_time_update=false;
            }

            
        }else{
            gps_date[rx_counter]=char_recv;
            rx_counter++;
        }
        break;
    default:
        rx_sentence_counter=0;
        rx_counter=0;
    }
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

void pps_led_off(void){
    gpio_put(PPSLED_PIN, 0);
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
