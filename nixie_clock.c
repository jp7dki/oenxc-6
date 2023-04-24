#include "nixie_clock.h"


float z[MAX_Z];
float k[MAX_Z];

//---- cathode port definition ------------
uint8_t cathode_port[10] = {
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

//---- anode port definition -------------
uint8_t anode_port[6] = {
    DIGIT1_PIN,
    DIGIT2_PIN,
    DIGIT3_PIN,
    DIGIT4_PIN,
    DIGIT5_PIN,
    DIGIT6_PIN
};

//---- firmware version ----
const uint8_t version[6] = {0,0,0,1+0x10,0,0};

//-----------------------------------------------------------
// Private method
//-----------------------------------------------------------
static void disp_num(uint8_t num, uint8_t digit){

    // all Cathode OFF
    gpio_put(KR_PIN,0);
    gpio_put(KL_PIN,0);
    
    for(uint8_t i=0;i<10;i++){
        gpio_put(cathode_port[i],0);
    }

    // all Anode OFF    
    for(uint8_t i=0;i<6;i++){
        gpio_put(anode_port[i],0);
    }

    // Selected Anode and Cathode ON
    gpio_put(anode_port[digit],1);
    gpio_put(cathode_port[num&0x0F],1);

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

static void disp(NixieConfig *conf, uint8_t digit)
{
    disp_num(conf->num[digit], digit);
}

static void disp_next(NixieConfig *conf, uint8_t digit)
{
    disp_num(conf->next_num[digit], digit);
}

static void disp_blank(void)
{
    // all Cathode OFF
    gpio_put(KR_PIN,0);
    gpio_put(KL_PIN,0);
    
    for(uint8_t i=0;i<10;i++){
        gpio_put(cathode_port[i],0);
    }

    // all Anode OFF    
    for(uint8_t i=0;i<6;i++){
        gpio_put(anode_port[i],0);
    }
}

// 数値が変わった桁のチェック
static void check_change_num(NixieConfig *conf)
{
    for(uint8_t i=0; i<6; i++){
        if(conf->num[i] != conf->next_num[i]){
            conf->disp_change[i] = 1;
        }else{
            conf->disp_change[i] = 0;
        }
    }
}

static uint8_t get_brightness(NixieConfig *conf)
{
    return conf->brightness;
}

//---- pink filter -----------------------
static void init_pink(void) {
    extern float   z[MAX_Z];
    extern float   k[MAX_Z];
    int             i;

    for (i = 0; i < MAX_Z; i++)
        z[i] = 0;
    k[MAX_Z - 1] = 0.5;
    for (i = MAX_Z - 1; i > 0; i--)
        k[i - 1] = k[i] * 0.25;
}

static float pinkfilter(float in) {
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
} 

//-----------------------------------------------------------
// Public method
//-----------------------------------------------------------

// init: initialization peripheral
static void nixie_init(NixieConfig *conf)
{
    //---- GPIO Initialization -----------------
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

    //---- PWM -----------------
    gpio_init(VCONT_PIN);
    gpio_set_dir(VCONT_PIN, GPIO_OUT);
    gpio_put(VCONT_PIN,0);
    gpio_set_function(VCONT_PIN, GPIO_FUNC_PWM);

    conf->slice_num0 = pwm_gpio_to_slice_num(VCONT_PIN);
    pwm_set_wrap(conf->slice_num0, 3000);
    pwm_set_chan_level(conf->slice_num0, PWM_CHAN_A, 1800);
    pwm_set_enabled(conf->slice_num0, true);

    //---- ADC(Light sensor) ---------
    adc_init();
    adc_gpio_init(LSENSOR_PIN);
    gpio_set_dir(LSENSOR_PIN, GPIO_IN);
    adc_select_input(3);

    //---- High voltage power-supply -----
    gpio_init(HVEN_PIN);
    gpio_set_dir(HVEN_PIN, GPIO_OUT);
    gpio_put(HVEN_PIN,1);

    //---- variable initialization ----
    conf->brightness = 5;
    conf->brightness_auto = 1;
    conf->switch_mode = crossfade;
    for(uint8_t i=0; i<6; i++){
        conf->num[i] = 10;            // display off
        conf->disp_duty[i] = 100;
        conf->next_num[i] = 0;
    }
    conf->switch_counter=0;
    conf->flg_time_update=false;
    conf->flg_change=false;
    conf->auto_onoff=1;
    conf->auto_off_time.hour = 22;
    conf->auto_off_time.min = 0;
    conf->auto_on_time.hour = 6;
    conf->auto_on_time.min = 0;
    conf->time_difference.hour = 9;
    conf->time_difference.min = 0;
    conf->gps_correction = 1;
    conf->led_setting = 1;
    conf->fluctuation_level = 0;

    // pink-filter initialization
    init_pink();
}

// brightness_inc: nixie-tube brightness increment
static void nixie_brightness_inc(NixieConfig *conf)
{
    conf->brightness++;
    if(conf->brightness==10) conf->brightness = 0;
}

// brightness_update: nixie-tube brightness update
static void nixie_brightness_update(NixieConfig *conf)
{
    // light sensor read
    uint16_t brightness_level;
    if(conf->brightness_auto==1){
        uint32_t result = adc_read();
        brightness_level = result*100/3000;

        if(brightness_level > 100) brightness_level=100;
        if(brightness_level <20) brightness_level=20;
    }else{
        // light sensor don't read
        brightness_level = 100;
    }

    // 1/fゆらぎ処理
    int rd=random()/(RAND_MAX/10000)-5000;      // 乱数生成
    float pr = pinkfilter(rd); 
    brightness_level += pr*conf->fluctuation_level;

    uint16_t current_setting = 500+300*(conf->brightness+1)*brightness_level/100;

    // limitter
    if(current_setting > 2800){
        current_setting = 2800;
    }else if(current_setting < 100){
        current_setting = 100;
    }

    pwm_set_chan_level(conf->slice_num0, PWM_CHAN_A, current_setting);
    //pwm_set_chan_level(conf->slice_num0, PWM_CHAN_A, ((1000+160*conf->brightness)*brightness_level/100));
}

// switch_mode_inc: nixie-tube mode increment
static void nixie_switch_mode_inc(NixieConfig *conf)
{
    conf->switch_mode = (SwitchMode)((uint8_t)(conf->switch_mode) + 1);
    if(conf->switch_mode==5) conf->switch_mode = normal;
}

// dynamic_display_task: dynamic-drive display task
static void nixie_dynamic_display_task(NixieConfig *conf)
{
    for(uint8_t i=0;i<6;i++){
        // 通常の切り替え
        disp(conf, i);
        sleep_us(20*conf->disp_duty[i]);

        disp_blank();
        sleep_us(20*(100-conf->disp_duty[i]));

        sleep_us(150);
    }    
}

// dynamic_clock_task: dynamic-drive clock task
static void nixie_dynamic_clock_task(NixieConfig *conf)
{
    for(uint8_t i=0;i<6;i++){
        // Switch Mode毎の処理
        switch(conf->switch_mode){
            case normal:
                // 通常の切り替え
                disp(conf, i);
                sleep_us(20*conf->disp_duty[i]);
                break;

            case fade:
                // フェード
                if(conf->flg_change && (conf->num[i]!=conf->next_num[i])){
                    if(conf->switch_counter<10){
                        disp(conf, i);
                        sleep_us(1*conf->disp_duty[i]*(20-2*conf->switch_counter));

                        disp_blank();
                        sleep_us(1*conf->disp_duty[i]*2*conf->switch_counter);
                    }else{
                        disp_next(conf, i);
                        sleep_us(1*conf->disp_duty[i]*(2*(conf->switch_counter-10)));

                        disp_blank();
                        sleep_us(1*conf->disp_duty[i]*2*(20-conf->switch_counter));
                    }
                }else{
                    disp(conf, i);
                    sleep_us(20*conf->disp_duty[i]); 
                }
                break;
            case crossfade:
                // クロスフェード
                if(conf->flg_change){
                    disp(conf, i);
                    sleep_us(1*conf->disp_duty[i]*(20-conf->switch_counter));

                    disp_next(conf, i);
                    sleep_us(1*conf->disp_duty[i]*conf->switch_counter);
                }else{
                    disp(conf, i);
                    sleep_us(20*conf->disp_duty[i]);                        
                }
                break;
            case cloud:
                // クラウド(パタパタ)
                disp(conf, i);
                sleep_us(20*conf->disp_duty[i]);
                break;
            case dotmove:
                // ドットムーヴ
                disp(conf, i);
                sleep_us(20*conf->disp_duty[i]);
        }

        disp_blank();
        sleep_us(20*(100-conf->disp_duty[i]));

        sleep_us(150);
    }
}

// dynamic_setting_task: dynamic-drive setting task
static void nixie_dynamic_setting_task(NixieConfig *conf, uint8_t setting_num)
{
    // 番号の表示
    if(setting_num>9){
        conf->num[5] = setting_num/10;
    }else{
        conf->num[5] = 10;       // 消灯
    }
    conf->num[4] = setting_num%10 + 0x10;

    switch(setting_num){
        case 1:
            // Switching mode setting
            conf->num[3] = 10;   // 消灯
            conf->num[2] = 10;   // 消灯
            conf->num[1] = 10;   // 消灯
            conf->num[0] = conf->switch_mode;
            break;
        case 2:
            // brightness setting
            conf->num[3] = 10;   // 消灯
            conf->num[2] = 10;   // 消灯
            conf->num[1] = 10;   // 消灯
            conf->num[0] = get_brightness(conf);            
            break;
        case 3:
            // brightness auto on/off
            conf->num[3] = 10;   // 消灯
            conf->num[2] = 10;   // 消灯
            conf->num[1] = 10;   // 消灯
            conf->num[0] = conf->brightness_auto;           
            break;
        case 4:
            // auto-on/off setting            
            conf->num[3] = 10;   // 消灯
            conf->num[2] = 10;   // 消灯
            conf->num[1] = 10;   // 消灯
            conf->num[0] = conf->auto_onoff;
            break;
        case 5:
            // auto-on time setting
            conf->num[3] = conf->auto_on_time.hour/10;
            conf->num[2] = conf->auto_on_time.hour%10;
            conf->num[1] = conf->auto_on_time.min/10; 
            conf->num[0] = conf->auto_on_time.min%10;
            conf->num[conf->cursor] = conf->num[conf->cursor] + 0x10;
            break;
        case 6:
            // auto-off time setting
            conf->num[3] = conf->auto_off_time.hour/10;
            conf->num[2] = conf->auto_off_time.hour%10;
            conf->num[1] = conf->auto_off_time.min/10; 
            conf->num[0] = conf->auto_off_time.min%10;
            conf->num[conf->cursor] = conf->num[conf->cursor] + 0x10;
            break;
        case 7:
            // time_difference setting
            conf->num[3] = conf->time_difference.hour/10;
            conf->num[2] = conf->time_difference.hour%10;
            conf->num[1] = conf->time_difference.min/10; 
            conf->num[0] = conf->time_difference.min%10;
            conf->num[conf->cursor] = conf->num[conf->cursor] + 0x10;
            break;
        case 8:
            // GPS time correction on/off       
            conf->num[3] = 10;   // 消灯
            conf->num[2] = 10;   // 消灯
            conf->num[1] = 10;   // 消灯
            conf->num[0] = conf->gps_correction;
            break;
        case 9:
            // LED setting    
            conf->num[3] = 10;   // 消灯
            conf->num[2] = 10;   // 消灯
            conf->num[1] = 10;   // 消灯
            conf->num[0] = conf->led_setting;            
            break;
        case 10:
            // fluctuation level
            conf->num[3] = 10;   // 消灯
            conf->num[2] = conf->fluctuation_level/100;
            conf->num[1] = (conf->fluctuation_level/10)%10;
            conf->num[0] = conf->fluctuation_level%10;
            conf->num[conf->cursor] = conf->num[conf->cursor] + 0x10;
            break;
        
        default:
            break;
    }
    
    for(uint8_t i=0;i<6;i++){
        // 通常の切り替え
        disp(conf, i);
        sleep_us(20*conf->disp_duty[i]);

        disp_blank();
        sleep_us(20*(100-conf->disp_duty[i]));

        sleep_us(150);
    }    
}

// clock_tick: clock tick
static void nixie_clock_tick(NixieConfig *conf, datetime_t time)
{
    switch(conf->switch_mode){
        case normal:
            // normal
            conf->num[0] = time.sec%10;
            conf->num[1] = time.sec/10;
            conf->num[2] = time.min%10;
            conf->num[3] = time.min/10;
            conf->num[4] = time.hour%10;
            conf->num[5] = time.hour/10;
            break;
        case fade:
        case crossfade:
            // cross-fade
            conf->next_num[0] = time.sec%10;
            conf->next_num[1] = time.sec/10;
            conf->next_num[2] = time.min%10;
            conf->next_num[3] = time.min/10;
            conf->next_num[4] = time.hour%10;
            conf->next_num[5] = time.hour/10;  
            conf->flg_change=true;
            conf->switch_counter=0;
            break;
        case cloud:
            // cloud
            for(uint8_t i=0;i<6;i++){
                conf->next_num[i] = conf->num[i];
            }
            conf->num[0] = time.sec%10;
            conf->num[1] = time.sec/10;
            conf->num[2] = time.min%10;
            conf->num[3] = time.min/10;
            conf->num[4] = time.hour%10;
            conf->num[5] = time.hour/10; 

            // 数値が変わった桁はdisp_nextを1にする。
            check_change_num(conf);
            conf->flg_change=true;
            conf->switch_counter=0;
            break;
        case dotmove:     
            // dot-move 
            conf->num[0] = time.sec%10;
            conf->num[1] = time.sec/10;
            conf->num[2] = time.min%10;
            conf->num[3] = time.min/10;
            conf->num[4] = time.hour%10;
            conf->num[5] = time.hour/10; 
            conf->flg_change=true;
            conf->switch_counter=0;

    }
}

// switch_update: switching display update task
static void nixie_switch_update(NixieConfig *conf, datetime_t time)
{
    //---- Switch Mode -----------------------
    switch(conf->switch_mode){
        case normal:
            break;

        case fade:
        case crossfade:
            if(conf->flg_change){
                if(conf->switch_counter!=20){
                    conf->switch_counter++;
                }else{
                    for(uint8_t i=0;i<6;i++){
                        conf->num[i] = conf->next_num[i];
                    }
                    conf->flg_change=false;
                }
            }
            break;
        case cloud:
            if(conf->flg_change){
                if(conf->switch_counter!=20){
                    if((conf->switch_counter%4)==0){
                        for(uint8_t i=0;i<6;i++){
                            if(conf->disp_change[i]==1){
                                conf->num[i]=(conf->num[i]+(conf->switch_counter/4))%10;
                            }
                        }
                    }
                    conf->switch_counter++;
                }else{
                    conf->flg_change=false;
                    
                    conf->num[0] = time.sec%10;
                    conf->num[1] = time.sec/10;
                    conf->num[2] = time.min%10;
                    conf->num[3] = time.min/10;
                    conf->num[4] = time.hour%10;
                    conf->num[5] = time.hour/10; 
                }
            }
            break;
        case dotmove:
            if(conf->flg_change){
                if(conf->switch_counter<97){
                    conf->switch_counter++;

                    if((conf->switch_counter%4)==1){
                        for(uint8_t i=0;i<6;i++){
                            conf->num[i] &= 0x0F;
                        }
                        switch(conf->switch_counter/4){
                            case 0:
                            case 23:
                                conf->num[0] |= 0x10;
                                break;
                            case 1:
                            case 22:
                                conf->num[0] |= 0x20;
                                break;
                            case 2:
                            case 21:
                                conf->num[1] |= 0x10;
                                break;
                            case 3:
                            case 20:
                                conf->num[1] |= 0x20;
                                break;
                            case 4:
                            case 19:
                                conf->num[2] |= 0x10;
                                break;
                            case 5:
                            case 18:
                                conf->num[2] |= 0x20;
                                break;
                            case 6:
                            case 17:
                                conf->num[3] |= 0x10;
                                break;
                            case 7:
                            case 16:
                                conf->num[3] |= 0x20;
                                break;
                            case 8:
                            case 15:
                                conf->num[4] |= 0x10;
                                break;
                            case 9:
                            case 14:
                                conf->num[4] |= 0x20;
                                break;
                            case 10:
                            case 13:
                                conf->num[5] |= 0x10;
                                break;
                            case 11:
                            case 12:
                                conf->num[5] |= 0x20;
                                break;
                        }
                    }

                }else{
                    conf->switch_counter=0;
                    conf->flg_change=false;
                }
            }
    }
}

// startup_animetion: nixie-tube startup animation
static void nixie_startup_animation(NixieConfig *conf)
{
    uint16_t i,j;
    // number all number check
    sleep_ms(500);

    for(i=0;i<6;i++){
        conf->disp_duty[i]=0;
        conf->num[i]=0;
    }

    for(i=0;i<100;i++){
        for(j=0;j<6;j++){
            conf->disp_duty[j]++;
        }
        sleep_ms(8);
    }

    for(i=0;i<10;i++){
        for(j=0;j<6;j++){
            conf->num[j]=i;
        }
        sleep_ms(300);
    }

    // random number -> firmware version
    for(j=0;j<(6*40+100);j++){
        for(i=0;i<6;i++){
            if(j<(i*20)){
                conf->num[i] = conf->num[i];
            }else if(j<((i*40)+100)){
                conf->num[i] = (uint8_t)(rand()%10)+0x30;
            }else{
                conf->num[i] = version[i];
            }
        }
        sleep_ms(5);
    }    
    sleep_ms(500);
}

// dispoff_animation: nixie-tube display off animation
static void nixie_dispoff_animation(NixieConfig *conf)
{
    for(uint16_t j=0;j<(6*40+100);j++){
        for(uint16_t i=0;i<6;i++){
            if(j<(i*20)){
                conf->num[i] = conf->num[i];
            }else if(j<((i*40)+100)){
                conf->num[i] = (uint8_t)(rand()%10)+0x30;
            }else{
                conf->num[i] = 10;
            }
        }
        sleep_ms(5);
    }    
}

static void nixie_dispon_animation(NixieConfig *conf)
{
    datetime_t t;

    rtc_get_datetime(&t);

    for(uint16_t j=0;j<(6*40+100);j++){
        for(uint16_t i=0;i<6;i++){
            if(j<(i*20)){
                conf->num[i] = 10;
            }else if(j<((i*40)+100)){
                conf->num[i] = (uint8_t)(rand()%10)+0x30;
            }else{
                switch(i){
                    case 0:
                        conf->num[0] = t.sec%10;
                        break;
                    case 1:
                        conf->num[1] = t.sec/10;
                        break;
                    case 2:
                        conf->num[2] = t.min%10;
                        break;
                    case 3:
                        conf->num[3] = t.min/10;
                        break;
                    case 4:
                        conf->num[4] = t.hour%10;
                        break;
                    case 5:
                        conf->num[5] = t.hour/10;
                        break; 
                }
            }
        }
        sleep_ms(5);
    }    
}

//---- nixie_auto_ontime_add : auto-on time add sequence ---------------------
static void nixie_time_add(NixieConfig *conf, datetime_t *time)
{
    switch(conf->cursor){
        case 0:
            if(time->min%10==9){
                time->min -= 9;
            }else{
                time->min++;
            }
            break;
        case 1:
            if(time->min/10==5){
                time->min = time->min%10;
            }else{
                time->min += 10;
            }
            break;
        case 2:
            if(time->hour%10 == 9){
                time->hour -= 9;
            }else if(time->hour == 23){
                time->hour = 20;
            }else{
                time->hour++;
            }
            break;
        case 3:
            if(time->hour > 13){
                time->hour = time->hour%10;
            }else{
                time->hour += 10;
            }
            break;
    }
}

//---- get_time_diffrence_correction --------------------------
datetime_t nixie_get_time_difference_correction(NixieConfig *conf, datetime_t time)
{
    datetime_t t = time;
    int8_t min, hour;

    min = t.min + conf->time_difference.min;

    if(min > 59){
        t.hour++;
        min -= 60;
    }

    if(min < 0){
        t.hour--;
        min += 60;
    }

    hour = t.hour + conf->time_difference.hour;

    if(hour > 23){
        hour -= 24;
    }

    if(hour < 0){
        hour += 24;
    }

    t.hour = hour;
    t.min = min;

    return t;
}

static void nixie_fluctuation_level_add(NixieConfig *conf)
{
    switch(conf->cursor){
        case 0:
            if(conf->fluctuation_level%10==9){
                conf->fluctuation_level -= 9;
            }else{
                conf->fluctuation_level++;
            }
            break;
        case 1:
            if((conf->fluctuation_level/10)%10==9){
                conf->fluctuation_level -= 90;
            }else{
                conf->fluctuation_level += 10;
            }
            break;
        case 2:
            if(conf->fluctuation_level/100 == 9){
                conf->fluctuation_level -= 900;
            }else{
                conf->fluctuation_level += 100;
            }
            break;
    }
}

// constractor
NixieTube new_NixieTube(NixieConfig Config)
{
    return ((NixieTube){
        .conf = Config,
        .init = nixie_init,
        
        .brightness_inc = nixie_brightness_inc,
        .brightness_update = nixie_brightness_update,
        .switch_mode_inc = nixie_switch_mode_inc,
        .dynamic_display_task = nixie_dynamic_display_task,
        .dynamic_clock_task = nixie_dynamic_clock_task,
        .dynamic_setting_task = nixie_dynamic_setting_task,
        .clock_tick = nixie_clock_tick,
        .switch_update = nixie_switch_update,
        .startup_animation = nixie_startup_animation,
        .dispoff_animation = nixie_dispoff_animation,
        .dispon_animation = nixie_dispon_animation,
        .time_add = nixie_time_add,
        .get_time_difference_correction = nixie_get_time_difference_correction,
        .fluctuation_level_add = nixie_fluctuation_level_add
    });
}