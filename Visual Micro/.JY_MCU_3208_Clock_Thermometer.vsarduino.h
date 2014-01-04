#ifndef _VSARDUINO_H_
#define _VSARDUINO_H_
//Board = JY-MCU-3208 Optiboot8 (8mhz)
#define __AVR_ATmega8__
#define 
#define ARDUINO 105
#define ARDUINO_MAIN
#define __AVR__
#define F_CPU 8000000L
#define __cplusplus
#define __inline__
#define __asm__(x)
#define __extension__
#define __ATTR_PURE__
#define __ATTR_CONST__
#define __inline__
#define __asm__ 
#define __volatile__

#define __builtin_va_list
#define __builtin_va_start
#define __builtin_va_end
#define __DOXYGEN__
#define __attribute__(x)
#define NOINLINE __attribute__((noinline))
#define prog_void
#define PGM_VOID_P int
            
typedef unsigned char byte;
extern "C" void __cxa_pure_virtual() {;}

void HTsend(uint16_t data, byte bits);
void HTcommand(uint16_t data);
void HTsendscreen(void);
void HTsetup();
void HTbrightness(byte b);
inline void clocksetup();
void incsec(byte add);
void decsec(byte sub);
byte clockhandler(void);
void renderclock(void);
void renderTemperature(void);
//
//
void requestTemp();
int getTemp();

#include "D:\Documents\Documents\Arduino\hardware\JYMCU3208\variants\standard\pins_arduino.h" 
#include "D:\Documents\Documents\Arduino\hardware\JYMCU3208\cores\arduino\arduino.h"
#include "D:\Documents\Documents\Arduino\JY_MCU_3208_Clock_Thermometer\JY_MCU_3208_Clock_Thermometer.ino"
#endif
