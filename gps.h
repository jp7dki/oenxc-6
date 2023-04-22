#ifndef GPS
#define GPS

//-------------------------------------------------------
//- Header files include
//-------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include "nixie_clock.h"
#include "pico/stdlib.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/rtc.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "pico/util/datetime.h"
#include "pico/binary_info.h"

//---- GPS Config parameters ---------------------
typedef struct 
{
    uint8_t gps_time[16];
    uint8_t gps_date[16];
    datetime_t gps_datetime;
    uint8_t gps_valid;
    uint16_t rx_counter;
    uint16_t rx_sentence_counter;
} GpsConfig;

//---- GPS structure --------------------
typedef struct gps Gps;

struct gps
{
    GpsConfig conf;

    //---- initialization -------------
    void (*init)(GpsConfig *conf, irq_handler_t *rx_irq_callback, irq_handler_t *pps_irq_callback);
    bool (*receive)(GpsConfig *conf, char char_recv);
    void (*pps_led_on)(void);
    void (*pps_led_off)(void);
};

//---- constructor -------------
Gps new_Gps(GpsConfig Config);

#endif 