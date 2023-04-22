#ifndef NIXIE_CLOCK_DEFINE
#define NIXIE_CLOCK_DEFINE

//--------------------------------------
// PIN assign
//--------------------------------------
#define K0_PIN 11
#define K1_PIN 10
#define K2_PIN 9
#define K3_PIN 8
#define K4_PIN 7
#define K5_PIN 6
#define K6_PIN 5
#define K7_PIN 4
#define K8_PIN 3
#define K9_PIN 2
#define KR_PIN 0
#define KL_PIN 1
#define DIGIT1_PIN 22
#define DIGIT2_PIN 16
#define DIGIT3_PIN 17
#define DIGIT4_PIN 18
#define DIGIT5_PIN 19
#define DIGIT6_PIN 15

#define VCONT_PIN 14
#define DBG_TX_PIN 12
#define DBG_RX_PIN 13
#define PPS_PIN 20
#define GPS_TX_PIN 21
#define SWA_PIN 23
#define SWB_PIN 24
#define SWC_PIN 25
#define DBGLED_PIN 26
#define PPSLED_PIN 27
#define HVEN_PIN 28
#define LSENSOR_PIN 29

//--------------------------------------
// Peripheral
//--------------------------------------
#define UART_DEBUG uart0
#define UART_GPS uart1

#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY UART_PARITY_NONE

// UART1(GPS)
#define BAUD_RATE_GPS 9600

#endif