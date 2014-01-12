JY_MCU_3208_Clock_Thermometer
=============================
The JY-MCU 3208 is a cheap led-matrix display module that comes fitted with an Atmega8 microcontroller.

This is my firmware which is a work in progress.  This is written in the Arduino Environment though borrows heavily from code by Dr Jones (http://club.dx.com/forums/forums.dx/threadid.1118199) which was written in C++

So far the module is working well with the original microcontroller using it's internal 8mhz clock.  Temperature is working with a Dallas DS1820 module (the oldest in this family) though should also work with the DS18S20 and DS18B20 temperature sensors.

Code is in place that utilises a Maxim DS3202 RTC chip for more accurate time keeping.  I am using a supercapacitor to back up the clock when the device is not powered..
