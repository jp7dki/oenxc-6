#include <stdio.h>
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
#define UART_DEBUG uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

// UART1(GPS)
#define UART_GPS uart1
#define BAUD_RATE_GPS 9600


const uint KR_PIN = 0;
const uint KL_PIN = 1;
const uint K9_PIN = 2;
const uint K8_PIN = 3;
const uint K7_PIN = 4;
const uint K6_PIN = 5;
const uint K5_PIN = 6;
const uint K4_PIN = 7;
const uint K3_PIN = 8;
const uint K2_PIN = 9;
const uint K1_PIN = 10;
const uint K0_PIN = 11;
const uint DBG_TX_PIN = 12;
const uint DBG_RX_PIN = 13;
const uint VCONT_PIN = 14;
const uint DIGIT6_PIN = 15;
const uint DIGIT2_PIN = 16;
const uint DIGIT3_PIN = 17;
const uint DIGIT4_PIN = 18;
const uint DIGIT5_PIN = 19;
const uint PPS_PIN = 20;
const uint GPS_TX_PIN = 21;
const uint DIGIT1_PIN = 22;
const uint SWA_PIN = 23;
const uint SWB_PIN = 24;
const uint SWC_PIN = 25;
const uint DBGLED_PIN = 26;
const uint PPSLED_PIN = 27;
const uint HVEN_PIN = 28;
const uint LSENSOR_PIN = 29;

uint8_t count;

uint8_t disp_num[6];
uint8_t disp_duty[6];

uint16_t rx_counter;
uint16_t rx_sentence_counter;
uint8_t gps_time[16];
uint8_t gps_date[16];
uint8_t gps_valid=0;
bool flg_time_correct=false;
const uint8_t GPS_HEADER[7] = {'$','G','P','R','M','C',','};

uint16_t pps_led_counter=0;

//-------------------------------------------------------
//- Function Prototyping
//-------------------------------------------------------
void hardware_init(void);
void disp_nixie(uint num, uint digit);
void delay_execution(void (*func_ptr)(void), uint16_t delay_ms);

//-------------------------------------------------------
//- IRQ
//-------------------------------------------------------
//---- timer_alarm0 : 1秒ごとの割り込み ----
// RTCによる更新
static void timer_alarm0_irq(void) {
    datetime_t time;
    uint8_t i;

    // Clear the irq
    hw_clear_bits(&timer_hw->intr, 1u << 0);
    uint64_t target = timer_hw->timerawl + 999999; // interval 1s
    timer_hw->alarm[0] = (uint32_t)target;

    if(flg_time_correct==false){
        rtc_get_datetime(&time);

        disp_num[0] = time.hour/10;
        disp_num[1] = time.hour%10;
        disp_num[2] = time.min/10;
        disp_num[3] = time.min%10;
        disp_num[4] = time.sec/10;
        disp_num[5] = time.sec%10;

        for(i=0;i<6;i++){
            uart_putc(UART_DEBUG,'0'+disp_num[i]);
        }
        uart_putc(UART_DEBUG, '\n');
        uart_putc(UART_DEBUG, '\r');
        
        gpio_put(PPSLED_PIN, 1);
        sleep_ms(100);
        gpio_put(PPSLED_PIN, 0);
    }

}

//---- timer_alarm1 : 10msごとの割り込み ----
static void timer_alerm1_irq(void) {
    // Clear the irq
    hw_clear_bits(&timer_hw->intr, 1u << 1);
    uint64_t target = timer_hw->timerawl + 10000; // interval 40ms
    timer_hw->alarm[1] = (uint32_t)target;

    if(pps_led_counter!=0){
        pps_led_counter--;
        if(pps_led_counter==0){
            gpio_put(PPSLED_PIN, 0);
        }
    }

}

//---- GPIO割り込み(1PPS) ----
void gpio_callback(){
    datetime_t time;
    uint8_t i;

    rtc_get_datetime(&time);

    disp_num[0] = time.hour/10;
    disp_num[1] = time.hour%10;
    disp_num[2] = time.min/10;
    disp_num[3] = time.min%10;
    disp_num[4] = time.sec/10;
    disp_num[5] = time.sec%10;

    for(i=0;i<6;i++){
        uart_putc(UART_DEBUG,'0'+disp_num[i]);
    }
    uart_putc(UART_DEBUG, '\n');
    uart_putc(UART_DEBUG, '\r');

    // 1PPS_LED ON (200ms)
    gpio_put(PPSLED_PIN, 1);
    pps_led_counter=20;
}

//---- uart_rx : GPSの受信割り込み --------------------
void on_uart_rx(){
    int i;
    uint8_t ch;
    uint8_t hour,min,sec;

    // GPSの受信処理 GPRMCのみ処理する
    while(uart_is_readable(UART_GPS)){
        ch = uart_getc(UART_GPS);

        switch(rx_sentence_counter){
            case 0:
                // $GPRMC待ち
                if(GPS_HEADER[rx_counter]==ch){
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
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;
                }else{
                    gps_time[rx_counter]=ch;
                    rx_counter++;
                }
                break;
            case 2:
                // Status取得
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;
                }else{
                    gps_valid=ch;
                    rx_counter++;
                }
                break;
            case 3:
                // 緯度
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;
                }else{
                    // 読み捨て
                    rx_counter++;
                }
                break;
            case 4:
                // 北緯or南緯
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;
                }else{
                    // 読み捨て
                    rx_counter++;
                }
                break;
            case 5:
                // 経度
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;
                }else{
                    // 読み捨て 
                    rx_counter++;
                }
                break;
            case 6:
                // 東経or西経
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;
                }else{
                    // 読み捨て
                    rx_counter++;
                }
                break;
            case 7:
                // 地表における移動の速度
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;
                }else{
                    // 読み捨て
                    rx_counter++;
                }
                break;
            case 8:
                // 地表における移動の真方位
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;
                }else{
                    // 読み捨て
                    rx_counter++;
                }
                break;
            case 9:
                // 時刻取得
                if(ch==','){
                    rx_sentence_counter++;
                    rx_counter=0;

                    if((gps_valid=='A') && (flg_time_correct==false)){
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

                        flg_time_correct=true;
                    }

                    
                }else{
                    gps_date[rx_counter]=ch;
                    rx_counter++;
                }
                break;
            default:
                rx_sentence_counter=0;
                rx_counter=0;
        }
    }
}

//---------------------------------------------------------
//- core1 entry
//---------------------------------------------------------
// core1はニキシー管の表示処理のみ行う。
void core1_entry(){
    uint8_t i;

    while(1){
        for(i=0;i<6;i++){
            disp_nixie(disp_num[i],i);
            sleep_us(5*disp_duty[i]);
            disp_nixie(10,6); // blank time
            sleep_us(5*(100-disp_duty[i]));
            sleep_us(300);
        }
    }
}

//---------------------------------------------------------
//- main function
//---------------------------------------------------------
// 
int main(){

    uint8_t i;
    datetime_t time;

    bi_decl(bi_program_description("This is a test program for nixie6."));

    hardware_init();

    // タイマのセッティング
    hw_set_bits(&timer_hw->inte, 1u<<0);        // Alarm0
    irq_set_exclusive_handler(TIMER_IRQ_0, timer_alarm0_irq);
    irq_set_enabled(TIMER_IRQ_0, true);
    uint64_t target = timer_hw->timerawl + 1000000; // interval 1s
    timer_hw->alarm[0] = (uint32_t)target;

    hw_set_bits(&timer_hw->inte, 1u<<1);        // Alarm1
    irq_set_exclusive_handler(TIMER_IRQ_1, timer_alerm1_irq);
    irq_set_enabled(TIMER_IRQ_1, true);
    target = timer_hw->timerawl + 100000;   // interval 100ms
    timer_hw->alarm[1] = (uint32_t)target;

    // display number reset
    for(i=0;i<6;i++){
        disp_num[i]=0;
        disp_duty[i]=100;
    }

    count = 0;

    // Core1に処理を割り当て
    multicore_launch_core1(core1_entry);

    // GPIO割り込み設定(1PPS)
    gpio_set_irq_enabled_with_callback(PPS_PIN, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

    while (1) {

    }
}


//-------------------------------------------------
// hardware initialization
//-------------------------------------------------
void hardware_init(void)
{   
//    stdio_init_all();

    gpio_init(KR_PIN);
    gpio_init(KL_PIN);
    gpio_init(K9_PIN);
    gpio_init(K8_PIN);
    gpio_init(K7_PIN);
    gpio_init(K6_PIN);
    gpio_init(K5_PIN);
    gpio_init(K4_PIN);
    gpio_init(K3_PIN);
    gpio_init(K2_PIN);
    gpio_init(K1_PIN);
    gpio_init(K0_PIN);
    gpio_set_dir(KR_PIN, GPIO_OUT);
    gpio_set_dir(KL_PIN, GPIO_OUT);
    gpio_set_dir(K9_PIN, GPIO_OUT);
    gpio_set_dir(K8_PIN, GPIO_OUT);
    gpio_set_dir(K7_PIN, GPIO_OUT);
    gpio_set_dir(K6_PIN, GPIO_OUT);
    gpio_set_dir(K5_PIN, GPIO_OUT);
    gpio_set_dir(K4_PIN, GPIO_OUT);
    gpio_set_dir(K3_PIN, GPIO_OUT);
    gpio_set_dir(K2_PIN, GPIO_OUT);
    gpio_set_dir(K1_PIN, GPIO_OUT);
    gpio_set_dir(K0_PIN, GPIO_OUT);
    gpio_put(KR_PIN,0);
    gpio_put(KL_PIN,0);
    gpio_put(K9_PIN,0);
    gpio_put(K8_PIN,0);
    gpio_put(K7_PIN,0);
    gpio_put(K6_PIN,0);
    gpio_put(K5_PIN,0);
    gpio_put(K4_PIN,0);
    gpio_put(K3_PIN,0);
    gpio_put(K2_PIN,0);
    gpio_put(K1_PIN,0);
    gpio_put(K0_PIN,0);

    gpio_init(DBG_TX_PIN);
    gpio_init(DBG_RX_PIN);
    gpio_set_dir(DBG_TX_PIN, GPIO_OUT);
    gpio_set_dir(DBG_RX_PIN, GPIO_IN);
    gpio_put(DBG_TX_PIN,0);
    gpio_set_function(DBG_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(DBG_RX_PIN, GPIO_FUNC_UART);

    gpio_init(VCONT_PIN);
    gpio_set_dir(VCONT_PIN, GPIO_OUT);
    gpio_put(VCONT_PIN,0);
    gpio_set_function(VCONT_PIN, GPIO_FUNC_PWM);

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

    gpio_init(HVEN_PIN);
    gpio_set_dir(HVEN_PIN, GPIO_OUT);
    gpio_put(HVEN_PIN,1);

    gpio_init(LSENSOR_PIN);
    gpio_set_dir(LSENSOR_PIN, GPIO_IN);

    //---- PWM -----------------
    uint slice_num0 = pwm_gpio_to_slice_num(VCONT_PIN);
    pwm_set_wrap(slice_num0, 3000);
    pwm_set_chan_level(slice_num0, PWM_CHAN_A, 1185);
    pwm_set_enabled(slice_num0, true);

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

    //---- ADC ------------------
    adc_init();
    adc_gpio_init(LSENSOR_PIN);
    adc_select_input(3);

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

void disp_nixie(uint num, uint digit){

    // all Cathode OFF
    gpio_put(KR_PIN,0);
    gpio_put(KL_PIN,0);
    gpio_put(K9_PIN,0);
    gpio_put(K8_PIN,0);
    gpio_put(K7_PIN,0);
    gpio_put(K6_PIN,0);
    gpio_put(K5_PIN,0);
    gpio_put(K4_PIN,0);
    gpio_put(K3_PIN,0);
    gpio_put(K2_PIN,0);
    gpio_put(K1_PIN,0);
    gpio_put(K0_PIN,0);

    // all Anode OFF
    gpio_put(DIGIT1_PIN,0);
    gpio_put(DIGIT2_PIN,0);
    gpio_put(DIGIT3_PIN,0);
    gpio_put(DIGIT4_PIN,0);
    gpio_put(DIGIT5_PIN,0);
    gpio_put(DIGIT6_PIN,0);

    switch(digit){
        case 0:
            gpio_put(DIGIT1_PIN, 1);
            break;
        
        case 1:
            gpio_put(DIGIT2_PIN, 1);
            break;
        
        case 2:
            gpio_put(DIGIT3_PIN, 1);
            break;
            
        case 3:
            gpio_put(DIGIT4_PIN, 1);
            break;
            
        case 4:
            gpio_put(DIGIT5_PIN, 1);
            break;
            
        case 5:
            gpio_put(DIGIT6_PIN, 1);
            break;
    }

    switch(num){
        case 0:
            gpio_put(K0_PIN,1);
            break;

        case 1:
            gpio_put(K1_PIN,1);
            break;

        case 2:
            gpio_put(K2_PIN,1);
            break;

        case 3:
            gpio_put(K3_PIN,1);
            break;

        case 4:
            gpio_put(K4_PIN,1);
            break;

        case 5:
            gpio_put(K5_PIN,1);
            break;

        case 6:
            gpio_put(K6_PIN,1);
            break;

        case 7:
            gpio_put(K7_PIN,1);
            break;

        case 8:
            gpio_put(K8_PIN,1);
            break;

        case 9:
            gpio_put(K9_PIN,1);
            break;

    }
}

void delay_execution(void (*func_ptr)(void), uint16_t delay_ms){
    
}