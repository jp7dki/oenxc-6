#include "nixie_clock.hpp"

//-----------------------------------------------------------
// Private method
//-----------------------------------------------------------
void NixieTube::disp_num(uint8_t num, uint8_t digit){

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

//-----------------------------------------------------------
// Public method
//-----------------------------------------------------------
NixieTube::NixieTube()
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

    //---- PWM -----------------
    gpio_init(VCONT_PIN);
    gpio_set_dir(VCONT_PIN, GPIO_OUT);
    gpio_put(VCONT_PIN,0);
    gpio_set_function(VCONT_PIN, GPIO_FUNC_PWM);

    slice_num0 = pwm_gpio_to_slice_num(VCONT_PIN);
    pwm_set_wrap(slice_num0, 3000);
    pwm_set_chan_level(slice_num0, PWM_CHAN_A, 1800);
    pwm_set_enabled(slice_num0, true);

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
    brightness = 5;
    brightness_auto = 1;
    switch_mode = crossfade;
    for(uint8_t i=0; i<6; i++){
        num[i] = 10;            // display off
        disp_duty[i] = 100;
        next_num[i] = 0;
    }
    switch_counter=0;
    flg_time_update=false;
    flg_change=false;

}

NixieTube::~NixieTube()
{
    // Nothing to do
}

void NixieTube::disp(uint8_t digit)
{
    disp_num(num[digit], digit);
}

void NixieTube::disp_next(uint8_t digit)
{
    disp_num(next_num[digit], digit);
}

void NixieTube::disp_blank(void)
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
void NixieTube::check_change_num(void)
{
    for(uint8_t i=0; i<6; i++){
        if(num[i] != next_num[i]){
            disp_change[i] = 1;
        }else{
            disp_change[i] = 0;
        }
    }
}

void NixieTube::brightness_inc(void)
{
    brightness++;
    if(brightness==10) brightness = 0;
}

uint8_t NixieTube::get_brightness(void)
{
    return brightness;
}

void NixieTube::brightness_update(void)
{
    // light sensor read
    uint8_t brightness_level;
    if(brightness_auto==1){
        uint32_t result = adc_read();
        brightness_level = result*100/3000;
        if(brightness_level > 100) brightness_level=100;
        if(brightness_level <20) brightness_level=20;
    }else{
        brightness_level = 100;
    }

    pwm_set_chan_level(slice_num0, PWM_CHAN_A, ((1000+160*brightness)*brightness_level/100));
}

void NixieTube::switch_mode_inc(void)
{
    switch_mode = (SwitchMode)((uint8_t)switch_mode + 1);
    if(switch_mode==5) switch_mode = normal;
}

void NixieTube::dynamic_display_task(void)
{
    for(uint8_t i=0;i<6;i++){
        // 通常の切り替え
        disp(i);
        sleep_us(20*disp_duty[i]);

        disp_blank();
        sleep_us(20*(100-disp_duty[i]));

        sleep_us(150);
    }    
}

void NixieTube::dynamic_clock_task(void)
{
    for(uint8_t i=0;i<6;i++){
        // Switch Mode毎の処理
        switch(switch_mode){
            case normal:
                // 通常の切り替え
                disp(i);
                sleep_us(20*disp_duty[i]);
                break;

            case fade:
                // フェード
                if(flg_change && (num[i]!=next_num[i])){
                    if(switch_counter<10){
                        disp(i);
                        sleep_us(1*disp_duty[i]*(20-2*switch_counter));

                        disp_blank();
                        sleep_us(1*disp_duty[i]*2*switch_counter);
                    }else{
                        disp_next(i);
                        sleep_us(1*disp_duty[i]*(2*(switch_counter-10)));

                        disp_blank();
                        sleep_us(1*disp_duty[i]*2*(20-switch_counter));
                    }
                }else{
                    disp(i);
                    sleep_us(20*disp_duty[i]); 
                }
                break;
            case crossfade:
                // クロスフェード
                if(flg_change){
                    disp(i);
                    sleep_us(1*disp_duty[i]*(20-switch_counter));

                    disp_next(i);
                    sleep_us(1*disp_duty[i]*switch_counter);
                }else{
                    disp(i);
                    sleep_us(20*disp_duty[i]);                        
                }
                break;
            case cloud:
                // クラウド(パタパタ)
                disp(i);
                sleep_us(20*disp_duty[i]);
                break;
            case dotmove:
                // ドットムーヴ
                disp(i);
                sleep_us(20*disp_duty[i]);
        }

        disp_blank();
        sleep_us(20*(100-disp_duty[i]));

        sleep_us(150);
    }
}

void NixieTube::dynamic_setting_task(uint8_t setting_num)
{
    // 番号の表示
    if(setting_num>9){
        num[5] = setting_num/10;
    }else{
        num[5] = 10;       // 消灯
    }
    num[4] = setting_num%10 + 0x10;

    switch(setting_num){
        case 1:
            // Switching mode setting
            num[3] = 10;   // 消灯
            num[2] = 10;   // 消灯
            num[1] = 10;   // 消灯
            num[0] = switch_mode;
            break;
        case 2:
            // brightness setting
            num[3] = 10;   // 消灯
            num[2] = 10;   // 消灯
            num[1] = 10;   // 消灯
            num[0] = get_brightness();            
            break;
        case 3:
            // brightness auto on/off
            num[3] = 10;   // 消灯
            num[2] = 10;   // 消灯
            num[1] = 10;   // 消灯
            num[0] = brightness_auto;           
            break;
        case 4:
            // auto-on time setting

            break;
        case 5:
            // auto-off time setting

            break;
        default:
            num[3] = 10;   // 消灯
            num[2] = 10;   // 消灯
            num[1] = 10;   // 消灯
            num[0] = 0;
    }
    
    for(uint8_t i=0;i<6;i++){
        // 通常の切り替え
        disp(i);
        sleep_us(20*disp_duty[i]);

        disp_blank();
        sleep_us(20*(100-disp_duty[i]));

        sleep_us(150);
    }    
}

void NixieTube::clock_tick(datetime_t time)
{
    switch(switch_mode){
        case normal:
            // normal
            num[0] = time.sec%10;
            num[1] = time.sec/10;
            num[2] = time.min%10;
            num[3] = time.min/10;
            num[4] = time.hour%10;
            num[5] = time.hour/10;
            break;
        case fade:
        case crossfade:
            // cross-fade
            next_num[0] = time.sec%10;
            next_num[1] = time.sec/10;
            next_num[2] = time.min%10;
            next_num[3] = time.min/10;
            next_num[4] = time.hour%10;
            next_num[5] = time.hour/10;  
            flg_change=true;
            switch_counter=0;
            break;
        case cloud:
            // cloud
            for(uint8_t i=0;i<6;i++){
                next_num[i] = num[i];
            }
            num[0] = time.sec%10;
            num[1] = time.sec/10;
            num[2] = time.min%10;
            num[3] = time.min/10;
            num[4] = time.hour%10;
            num[5] = time.hour/10; 

            // 数値が変わった桁はdisp_nextを1にする。
            check_change_num();
            flg_change=true;
            switch_counter=0;
            break;
        case dotmove:     
            // dot-move 
            num[0] = time.sec%10;
            num[1] = time.sec/10;
            num[2] = time.min%10;
            num[3] = time.min/10;
            num[4] = time.hour%10;
            num[5] = time.hour/10; 
            flg_change=true;
            switch_counter=0;

    }
}

void NixieTube::switch_update(datetime_t time)
{
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
                    for(uint8_t i=0;i<6;i++){
                        num[i] = next_num[i];
                    }
                    flg_change=false;
                }
            }
            break;
        case cloud:
            if(flg_change){
                if(switch_counter!=20){
                    if((switch_counter%4)==0){
                        for(uint8_t i=0;i<6;i++){
                            if(disp_change[i]==1){
                                num[i]=(num[i]+(switch_counter/4))%10;
                            }
                        }
                    }
                    switch_counter++;
                }else{
                    flg_change=false;
                    
                    num[0] = time.sec%10;
                    num[1] = time.sec/10;
                    num[2] = time.min%10;
                    num[3] = time.min/10;
                    num[4] = time.hour%10;
                    num[5] = time.hour/10; 
                }
            }
            break;
        case dotmove:
            if(flg_change){
                if(switch_counter<97){
                    switch_counter++;

                    if((switch_counter%4)==1){
                        for(uint8_t i=0;i<6;i++){
                            num[i] &= 0x0F;
                        }
                        switch(switch_counter/4){
                            case 0:
                            case 23:
                                num[0] |= 0x10;
                                break;
                            case 1:
                            case 22:
                                num[0] |= 0x20;
                                break;
                            case 2:
                            case 21:
                                num[1] |= 0x10;
                                break;
                            case 3:
                            case 20:
                                num[1] |= 0x20;
                                break;
                            case 4:
                            case 19:
                                num[2] |= 0x10;
                                break;
                            case 5:
                            case 18:
                                num[2] |= 0x20;
                                break;
                            case 6:
                            case 17:
                                num[3] |= 0x10;
                                break;
                            case 7:
                            case 16:
                                num[3] |= 0x20;
                                break;
                            case 8:
                            case 15:
                                num[4] |= 0x10;
                                break;
                            case 9:
                            case 14:
                                num[4] |= 0x20;
                                break;
                            case 10:
                            case 13:
                                num[5] |= 0x10;
                                break;
                            case 11:
                            case 12:
                                num[5] |= 0x20;
                                break;
                        }
                    }

                }else{
                    switch_counter=0;
                    flg_change=false;
                }
            }
    }
}

void NixieTube::startup_animation(void)
{
    uint16_t i,j;
    // number all number check
    sleep_ms(500);

    for(i=0;i<6;i++){
        disp_duty[i]=0;
        num[i]=0;
    }

    for(i=0;i<100;i++){
        for(j=0;j<6;j++){
            disp_duty[j]++;
        }
        sleep_ms(8);
    }

    for(i=0;i<10;i++){
        for(j=0;j<6;j++){
            num[j]=i;
        }
        sleep_ms(300);
    }

    // random number -> firmware version
    for(j=0;j<(6*40+100);j++){
        for(i=0;i<6;i++){
            if(j<(i*20)){
                num[i] = num[i];
            }else if(j<((i*40)+100)){
                num[i] = (uint8_t)(rand()%10)+0x30;
            }else{
                num[i] = version[i];
            }
        }
        sleep_ms(5);
    }    
    sleep_ms(500);
}

void NixieTube::dispoff_animation(void)
{
    for(uint16_t j=0;j<(6*40+100);j++){
        for(uint16_t i=0;i<6;i++){
            if(j<(i*20)){
                num[i] = num[i];
            }else if(j<((i*40)+100)){
                num[i] = (uint8_t)(rand()%10)+0x30;
            }else{
                num[i] = 10;
            }
        }
        sleep_ms(5);
    }    
}