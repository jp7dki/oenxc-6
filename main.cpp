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

#define NUM_TASK 10

//-------------------------------------------------------
//- Global Variable
//-------------------------------------------------------
const uint8_t version[6] = {0,0,0,1+0x10,0,0};
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
uint8_t disp_next[6];
uint8_t disp_change[6];
uint8_t disp_duty[6];

uint16_t rx_counter;
uint16_t rx_sentence_counter;
uint8_t gps_time[16];
uint8_t gps_date[16];
uint8_t gps_valid=0;
bool flg_time_correct=false;
bool flg_change=false;
uint16_t switch_counter=0;
const uint8_t GPS_HEADER[7] = {'$','G','P','R','M','C',','};

uint16_t pps_led_counter=0;
uint16_t blink_counter[6] = {0,0,0,0,0,0};
uint8_t cursor;
uint8_t setting_num=1;

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
} switch_mode;

enum OperationMode{
    power_on,
    power_up_animation,
    clock_display,
    settings,
    time_adjust,
    random_disp,
    demo
} operation_mode;

//-------------------------------------------------------
//- Function Prototyping
//-------------------------------------------------------
void hardware_init(void);
void disp_nixie(uint num, uint digit);
void delay_execution(void (*func_ptr)(void), uint16_t delay_ms);
void gps_receive(char char_recv);

bool task_add(func_ptr func, uint16_t delay_10ms);
void pps_led_off(void);

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

    if(operation_mode==clock_display){
        if(flg_time_correct==false){
            rtc_get_datetime(&time);

            switch(switch_mode){
                case normal:
                    // normal
                    disp_num[0] = time.sec%10;
                    disp_num[1] = time.sec/10;
                    disp_num[2] = time.min%10;
                    disp_num[3] = time.min/10;
                    disp_num[4] = time.hour%10;
                    disp_num[5] = time.hour/10;
                    break;
                case fade:
                case crossfade:
                    // cross-fade
                    disp_next[0] = time.sec%10;
                    disp_next[1] = time.sec/10;
                    disp_next[2] = time.min%10;
                    disp_next[3] = time.min/10;
                    disp_next[4] = time.hour%10;
                    disp_next[5] = time.hour/10;  
                    flg_change=true;
                    switch_counter=0;
                    break;
                case cloud:
                    // cloud
                    for(i=0;i<6;i++){
                        disp_next[i] = disp_num[i];
                    }
                    disp_num[0] = time.sec%10;
                    disp_num[1] = time.sec/10;
                    disp_num[2] = time.min%10;
                    disp_num[3] = time.min/10;
                    disp_num[4] = time.hour%10;
                    disp_num[5] = time.hour/10; 

                    // 数値が変わった桁はdisp_nextを1にする。
                    for(i=0;i<6;i++){
                        if(disp_next[i]==disp_num[i]){
                            disp_change[i]=0;
                        }else{
                            disp_change[i]=1;
                        }
                    }
                    flg_change=true;
                    switch_counter=0;
                    break;
                case dotmove:     
                    // dot-move 
                    disp_num[0] = time.sec%10;
                    disp_num[1] = time.sec/10;
                    disp_num[2] = time.min%10;
                    disp_num[3] = time.min/10;
                    disp_num[4] = time.hour%10;
                    disp_num[5] = time.hour/10; 
                    flg_change=true;
                    switch_counter=0;

            }
        }
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

    //---- Switch Mode -----------------------
    switch(switch_mode){
        case normal:
            break;

        case fade:
        case crossfade:
            if(flg_change){
                if(switch_counter!=20){
                    switch_counter++;
                }else{
                    for(i=0;i<6;i++){
                        disp_num[i] = disp_next[i];
                    }
                    flg_change=false;
                }
            }
            break;
        case cloud:
            if(flg_change){
                if(switch_counter!=20){
                    if((switch_counter%4)==0){
                        for(i=0;i<6;i++){
                            if(disp_change[i]==1){
                                disp_num[i]=(disp_num[i]+(switch_counter/4))%10;
                            }
                        }
                    }
                    switch_counter++;
                }else{
                    flg_change=false;
                    
                    rtc_get_datetime(&time);
                    disp_num[0] = time.sec%10;
                    disp_num[1] = time.sec/10;
                    disp_num[2] = time.min%10;
                    disp_num[3] = time.min/10;
                    disp_num[4] = time.hour%10;
                    disp_num[5] = time.hour/10; 
                }
            }
            break;
        case dotmove:
            if(flg_change){
                if(switch_counter<97){
                    switch_counter++;

                    if((switch_counter%4)==1){
                        for(i=0;i<6;i++){
                            disp_num[i] &= 0x0F;
                        }
                        switch(switch_counter/4){
                            case 0:
                            case 23:
                                disp_num[0] |= 0x10;
                                break;
                            case 1:
                            case 22:
                                disp_num[0] |= 0x20;
                                break;
                            case 2:
                            case 21:
                                disp_num[1] |= 0x10;
                                break;
                            case 3:
                            case 20:
                                disp_num[1] |= 0x20;
                                break;
                            case 4:
                            case 19:
                                disp_num[2] |= 0x10;
                                break;
                            case 5:
                            case 18:
                                disp_num[2] |= 0x20;
                                break;
                            case 6:
                            case 17:
                                disp_num[3] |= 0x10;
                                break;
                            case 7:
                            case 16:
                                disp_num[3] |= 0x20;
                                break;
                            case 8:
                            case 15:
                                disp_num[4] |= 0x10;
                                break;
                            case 9:
                            case 14:
                                disp_num[4] |= 0x20;
                                break;
                            case 10:
                            case 13:
                                disp_num[5] |= 0x10;
                                break;
                            case 11:
                            case 12:
                                disp_num[5] |= 0x20;
                                break;
                        }
                    }

                }else{
                    switch_counter=0;
                    flg_change=false;
                }
            }
    }

    //---- light sensor -------------------
    uint32_t result = adc_read();
    uint8_t duty = result*100/3000;
    if(duty > 100) duty=100;
    if(duty <20) duty=20;


    uint slice_num0 = pwm_gpio_to_slice_num(VCONT_PIN);
    pwm_set_chan_level(slice_num0, PWM_CHAN_A, 1800*duty/100);
    
    for(i=0;i<6;i++){
//        disp_duty[i] = duty;
//        disp_duty[i] = 100;
    }
}

//---- GPIO割り込み(1PPS) ----
void gpio_callback(uint gpio, uint32_t event){
    datetime_t time;
    uint8_t i;

    if(operation_mode==clock_display){
        // Display time update
        rtc_get_datetime(&time);
        switch(switch_mode){
            case normal:
                // normal switch
                disp_num[0] = time.sec%10;
                disp_num[1] = time.sec/10;
                disp_num[2] = time.min%10;
                disp_num[3] = time.min/10;
                disp_num[4] = time.hour%10;
                disp_num[5] = time.hour/10; 
                flg_change=true;
                break;
            
            case fade:
            case crossfade:
                // crossfade switch
                disp_next[0] = time.sec%10;
                disp_next[1] = time.sec/10;
                disp_next[2] = time.min%10;
                disp_next[3] = time.min/10;
                disp_next[4] = time.hour%10;
                disp_next[5] = time.hour/10;  
                flg_change=true;
                switch_counter=0;
                break;

            case cloud:
                // cloud(pata-pata) switch
                for(i=0;i<6;i++){
                    disp_next[i] = disp_num[i];
                }
                disp_num[0] = time.sec%10;
                disp_num[1] = time.sec/10;
                disp_num[2] = time.min%10;
                disp_num[3] = time.min/10;
                disp_num[4] = time.hour%10;
                disp_num[5] = time.hour/10; 

                // 数値が変わった桁はdisp_changeを1にする。
                for(i=0;i<6;i++){
                    if(disp_next[i]==disp_num[i]){
                        disp_change[i]=0;
                    }else{
                        disp_change[i]=1;
                    }
                }
                flg_change=true;
                switch_counter=0;
                break;

            case dotmove:
                // dot-move 
                disp_num[0] = time.sec%10;
                disp_num[1] = time.sec/10;
                disp_num[2] = time.min%10;
                disp_num[3] = time.min/10;
                disp_num[4] = time.hour%10;
                disp_num[5] = time.hour/10; 
                flg_change=true;
                switch_counter=0;
        }

        // 1PPS_LED ON (200ms)
        gpio_put(PPSLED_PIN, 1);
        task_add(pps_led_off, 20);
    }

}

//---- uart_rx : GPSの受信割り込み --------------------
void on_uart_rx(){
    int i;
    uint8_t ch;

    uart_putc(UART_DEBUG, ch);

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
                for(i=0;i<6;i++){
                    // 通常の切り替え
                    disp_nixie(disp_num[i],i);
                    sleep_us(20*disp_duty[i]);

                    disp_nixie(10,6); // blank time
                    sleep_us(20*(100-disp_duty[i]));

                    sleep_us(150);
                }
                break;


            // Clock display
            case clock_display:
                for(i=0;i<6;i++){
                    // Switch Mode毎の処理
                    switch(switch_mode){
                        case normal:
                            // 通常の切り替え
                            disp_nixie(disp_num[i],i);
                            sleep_us(20*disp_duty[i]);
                            break;

                        case fade:
                            // フェード
                            if(flg_change && (disp_num[i]!=disp_next[i])){
                                if(switch_counter<10){
                                    disp_nixie(disp_num[i],i);
                                    sleep_us(1*disp_duty[i]*(20-2*switch_counter));

                                    disp_nixie(10,6);          // nixie off
                                    sleep_us(1*disp_duty[i]*2*switch_counter);
                                }else{
                                    disp_nixie(disp_next[i],i);
                                    sleep_us(1*disp_duty[i]*(2*(switch_counter-10)));

                                    disp_nixie(10,6);          // nixie off
                                    sleep_us(1*disp_duty[i]*2*(20-switch_counter));
                                }
                            }else{
                                disp_nixie(disp_num[i],i);
                                sleep_us(20*disp_duty[i]); 
                            }
                            break;
                        case crossfade:
                            // クロスフェード
                            if(flg_change){
                                disp_nixie(disp_num[i],i);
                                sleep_us(1*disp_duty[i]*(20-switch_counter));

                                disp_nixie(disp_next[i],i);
                                sleep_us(1*disp_duty[i]*switch_counter);
                            }else{
                                disp_nixie(disp_num[i],i);
                                sleep_us(20*disp_duty[i]);                        
                            }
                            break;
                        case cloud:
                            // クラウド(パタパタ)
                            disp_nixie(disp_num[i],i);
                            sleep_us(20*disp_duty[i]);
                            break;
                        case dotmove:
                            // ドットムーヴ
                            disp_nixie(disp_num[i],i);
                            sleep_us(20*disp_duty[i]);
                    }

                    disp_nixie(10,6); // blank time
                    sleep_us(20*(100-disp_duty[i]));

                    sleep_us(150);
                }
                break;
            
            case settings:

                // 番号の表示
                if(setting_num>9){
                    disp_num[5] = setting_num/10;
                }else{
                    disp_num[5] = 10;       // 消灯
                }
                disp_num[4] = setting_num%10 + 0x10;

                switch(setting_num){
                    // 切り替え
                    case 1:
                        disp_num[3] = 10;   // 消灯
                        disp_num[2] = 10;   // 消灯
                        disp_num[1] = 10;   // 消灯
                        disp_num[0] = switch_mode;
                }
                
                for(i=0;i<6;i++){
                    // 通常の切り替え
                    disp_nixie(disp_num[i],i);
                    sleep_us(20*disp_duty[i]);

                    disp_nixie(10,6); // blank time
                    sleep_us(20*(100-disp_duty[i]));

                    sleep_us(150);
                }

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

    // task initialization
    for(i=0;i<NUM_TASK;i++){
        task_list[i].delay_10ms = 0;
    }

    // mode initialization
    switch_mode = fade;
    operation_mode = clock_display;

    bi_decl(bi_program_description("This is a test program for nixie6."));

    hardware_init();

    // Timer Settings
    hw_set_bits(&timer_hw->inte, 1u<<0);        // Alarm0
    irq_set_exclusive_handler(TIMER_IRQ_0, timer_alarm0_irq);
    irq_set_enabled(TIMER_IRQ_0, true);
    uint64_t target = timer_hw->timerawl + 1000000; // interval 1s
    timer_hw->alarm[0] = (uint32_t)target;

    hw_set_bits(&timer_hw->inte, 1u<<1);        // Alarm1
    irq_set_exclusive_handler(TIMER_IRQ_1, timer_alerm1_irq);
    irq_set_enabled(TIMER_IRQ_1, true);
    target = timer_hw->timerawl + 10000;   // interval 10ms
    timer_hw->alarm[1] = (uint32_t)target;

    // display number reset
    for(i=0;i<6;i++){
        disp_num[i]=10;     // display off  
        disp_duty[i]=100;
        disp_next[i]=0;
    }

    count = 0;

    // Core1 Task 
    multicore_launch_core1(core1_entry);

    // GPIO interrupt setting(1PPS)
    gpio_set_irq_enabled_with_callback(PPS_PIN, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);

    // Power-Up-Animation
    operation_mode = power_up_animation;
    
    // number all number check
    sleep_ms(500);

    for(i=0;i<6;i++){
        disp_duty[i]=0;
        disp_num[i]=0;
    }

    for(i=0;i<100;i++){
        for(j=0;j<6;j++){
            disp_duty[j]++;
        }
        sleep_ms(8);
    }

    for(i=0;i<10;i++){
        for(j=0;j<6;j++){
            disp_num[j]=i;
        }
        sleep_ms(300);
    }

    // random number -> firmware version
    rtc_get_datetime(&time);
    srand(time.month+time.day+time.hour+time.min+time.sec);
    for(j=0;j<(6*40+100);j++){
        for(i=0;i<6;i++){
            if(j<(i*20)){
                disp_num[i] = disp_num[i];
            }else if(j<((i*40)+100)){
                disp_num[i] = (uint8_t)(rand()%10)+0x30;
            }else{
                disp_num[i] = version[i];
            }
        }
        sleep_ms(5);
    }
    sleep_ms(500);

    // Clock Display mode
    operation_mode = clock_display;

    while (1) {
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
                                switch_mode = (SwitchMode)((uint8_t)switch_mode + 1);
                                if(switch_mode==5) switch_mode = normal;
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
    //---- GPIO -----------------
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
    pwm_set_chan_level(slice_num0, PWM_CHAN_A, 1800);
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

    switch(num&0x0F){
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

    switch(num&0xF0){
        case 0x10:
            gpio_put(KR_PIN,1);
            break;
        case 0x20:
            gpio_put(KL_PIN,1);
            break;
        case 0x30:
            gpio_put(KR_PIN,1);
            gpio_put(KL_PIN,1);
            break;
    }
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