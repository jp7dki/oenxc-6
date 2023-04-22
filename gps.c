#include "gps.h"

const uint8_t GPS_HEADER[7] = {'$','G','P','R','M','C',','};

//--------------------------------------
// private function
//--------------------------------------



//--------------------------------------
// public function
//--------------------------------------
//---- gps_receive : GPSのセンテンス処理 ------------------
static bool gps_receive(GpsConfig *conf, char char_recv)
{
    uint8_t hour,min,sec;

    switch(conf->rx_sentence_counter){
    case 0:
        // $GPRMC待ち
        if(GPS_HEADER[conf->rx_counter]==char_recv){
            conf->rx_counter++;
        }else{
            conf->rx_counter=0;
        }

        if(conf->rx_counter==7){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }
        break;
    case 1:
        // 時刻取得
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }else{
            conf->gps_time[conf->rx_counter]=char_recv;
            conf->rx_counter++;
        }
        break;
    case 2:
        // Status取得
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }else{
            conf->gps_valid=char_recv;
            conf->rx_counter++;
        }
        break;
    case 3:
        // 緯度
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }else{
            // 読み捨て
            conf->rx_counter++;
        }
        break;
    case 4:
        // 北緯or南緯
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }else{
            // 読み捨て
            conf->rx_counter++;
        }
        break;
    case 5:
        // 経度
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }else{
            // 読み捨て 
            conf->rx_counter++;
        }
        break;
    case 6:
        // 東経or西経
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }else{
            // 読み捨て
            conf->rx_counter++;
        }
        break;
    case 7:
        // 地表における移動の速度
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }else{
            // 読み捨て
            conf->rx_counter++;
        }
        break;
    case 8:
        // 地表における移動の真方位
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;
        }else{
            // 読み捨て
            conf->rx_counter++;
        }
        break;
    case 9:
        // 時刻取得
        if(char_recv==','){
            conf->rx_sentence_counter++;
            conf->rx_counter=0;

            if(conf->gps_valid=='A'){
                hour = (conf->gps_time[0]-48)*10+(conf->gps_time[1]-48);
                min = (conf->gps_time[2]-48)*10+(conf->gps_time[3]-48);
                sec = (conf->gps_time[4]-48)*10+(conf->gps_time[5]-48);

                // JSTへの補正
                if(hour>14){
                    hour = hour + 9 -24;
                }else{
                    hour = hour + 9;
                }                        
                
                conf->gps_datetime.year = 2000+(conf->gps_date[4]-48)*10+(conf->gps_date[5]-48);
                conf->gps_datetime.month = (conf->gps_date[2]-48)*10+(conf->gps_date[3]-48);
                conf->gps_datetime.day = (conf->gps_date[0]-48)*10+(conf->gps_date[1]-48);
                conf->gps_datetime.dotw = 1;
                conf->gps_datetime.hour = hour;
                conf->gps_datetime.min = min;
                conf->gps_datetime.sec = sec;

                return true;
            }

            
        }else{
            conf->gps_date[conf->rx_counter]=char_recv;
            conf->rx_counter++;
        }
        break;
    default:
        conf->rx_sentence_counter=0;
        conf->rx_counter=0;
    }

    return false;
}


static void gps_init(GpsConfig *conf, irq_handler_t *rx_irq_callback, irq_handler_t *pps_irq_callback)
{
    // variable initialization
    conf->gps_valid = 0;

    // peripheral initialization
    gpio_init(PPS_PIN);
    gpio_init(GPS_TX_PIN);
    gpio_set_dir(PPS_PIN, GPIO_IN);
    gpio_set_dir(GPS_TX_PIN, GPIO_IN);
    gpio_set_function(GPS_TX_PIN, GPIO_FUNC_UART);
    
    gpio_init(PPSLED_PIN);
    gpio_set_dir(PPSLED_PIN, GPIO_OUT);
    gpio_put(PPSLED_PIN,0);

    // UART INIT(GPS)
    uart_init(UART_GPS, 9600);
    int __unused actual1 = uart_set_baudrate(UART_GPS, BAUD_RATE_GPS);
    uart_set_hw_flow(UART_GPS, false, false);
    uart_set_format(UART_GPS, DATA_BITS, STOP_BITS, PARITY);
    uart_set_fifo_enabled(UART_GPS, false);
    int UART_IRQ = UART_GPS == uart0 ? UART0_IRQ : UART1_IRQ;

    uart_set_irq_enables(UART_GPS, true, false);
    irq_set_exclusive_handler(UART_IRQ, rx_irq_callback);
    irq_set_enabled(UART_IRQ, true);

    // GPIO interrupt setting(1PPS)
    gpio_set_irq_enabled_with_callback(PPS_PIN, GPIO_IRQ_EDGE_RISE, true, pps_irq_callback);

}

Gps new_Gps(GpsConfig Config)
{
    return ((Gps){
        .conf = Config,
        .init = gps_init,
        .receive = gps_receive

    });
}

