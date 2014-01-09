//#define F_CPU 8000000L// Remant from original code
#include <OneWire.h>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 16

// Setup a oneWire instance to communicate with Dallas temperature IC)
OneWire tempSensor(ONE_WIRE_BUS);

// Setup the clock
#include <DS1302.h>

// Init the DS1302
DS1302 rtc(10, 9, 8);

// Init a Time-data structure
Time t;

// This array stores the numbers that we use a lot in the clock
PROGMEM byte bigdigits[10][6] = {
	{ 126, 129, 129, 129, 129, 126 },		// 0
	{ 128, 132, 130, 255, 128, 128 },		// 1
	{ 130, 193, 161, 145, 137, 134 },		// 2
	{ 66, 129, 137, 137, 137, 118 },		// 3
	{ 0x3f, 0x20, 0x20, 0xfc, 0x20, 0x20 }, // 4
	{ 0x4f, 0x89, 0x89, 0x89, 0x89, 0x71 }, // 5
	{ 0x7e, 0x89, 0x89, 0x89, 0x89, 0x72 }, // 6
	{ 0x03, 0x01, 0xc1, 0x31, 0x0d, 0x03 }, // 7
	{ 0x76, 0x89, 0x89, 0x89, 0x89, 0x76 }, // 8
	{ 0x46, 0x89, 0x89, 0x89, 0x89, 0x7e }, // 9
};

//pins and macros

#define HTport   PORTB
#define HTddr    DDRB
#define HTstrobe 3		// These are the AVR PortB numbers, not the Arduino equivelants
#define HTclk    4
#define HTdata   5

#define HTclk0    HTport&=~(1<<HTclk)
#define HTclk1    HTport|= (1<<HTclk)
#define HTstrobe0 HTport&=~(1<<HTstrobe)
#define HTstrobe1 HTport|= (1<<HTstrobe)
#define HTdata0   HTport&=~(1<<HTdata)
#define HTdata1   HTport|= (1<<HTdata)
#define HTpinsetup() do{  HTddr |=(1<<HTstrobe)|(1<<HTclk)|(1<<HTdata); HTport|=(1<<HTstrobe)|(1<<HTclk)|(1<<HTdata);  }while(0)

// Key Definitions. 
// We will do all of this using C++ commands as it is more efficient than using the Arduino functions

#define key1 ((PIND&(1<<7))==0) // read the status of PD7 a bit quicker that using digitalRead(7);
#define key2 ((PIND&(1<<6))==0) // ditto for PD6  (Pin 6 on a normal Ardudino NG)
#define key3 ((PIND&(1<<5))==0) // ditto for PD5  (Pin 5 on a normal Ardudino NG)

// set as Keys as output and all high to enable the AVR's pullup resistors
#define keysetup() do{ DDRD&=0xff-(1<<7)-(1<<6)-(1<<5); PORTD|=(1<<7)+(1<<6)+(1<<5); }while(0)  //input, pull up

//the screen array, 1 byte = 1 column, left to right, lsb at top. 
byte leds[32];

#define HTstartsys   0b100000000010 //start system oscillator
#define HTstopsys    0b100000000000 //stop sytem oscillator and LED duty    <default
#define HTsetclock   0b100000110000 //set clock to master with internal RC  <default
#define HTsetlayout  0b100001000000 //NMOS 32*8 // 0b100-0010-ab00-0  a:0-NMOS,1-PMOS; b:0-32*8,1-24*16   default:ab=10
#define HTledon      0b100000000110 //start LEDs
#define HTledoff     0b100000000100 //stop LEDs    <default
#define HTsetbright  0b100101000000 //set brightness b=0..15  add b<<1  //0b1001010xxxx0 xxxx:brightness 0..15=1/16..16/16 PWM
#define HTblinkon    0b100000010010 //Blinking on
#define HTblinkoff   0b100000010000 //Blinking off  <default
#define HTwrite      0b1010000000   // 101-aaaaaaa-dddd-dddd-dddd-dddd-dddd-... aaaaaaa:nibble adress 0..3F   (5F for 24*16)

volatile byte sec = 5;				// this is volatile as it is altered within an interrupt
byte sec0 = 200;					// 
byte minute = 00;					// Set the minutes
byte hour = 12;
byte changing, bright = 3;
byte brights[4] = { 0, 2, 6, 15 }; //brightness levels

unsigned long lastMillis = 0;			// If using the millis function to keep time


void HTsend(uint16_t data, byte bits) {  //MSB first
	uint16_t bit = ((uint16_t)1) << (bits - 1);
	while (bit) {
		HTclk0;
		if (data & bit) HTdata1; else HTdata0;
		HTclk1;
		bit >>= 1;
	}
}

void HTcommand(uint16_t data) {
	HTstrobe0;
	HTsend(data, 12);
	HTstrobe1;
}

void HTsendscreen(void) {
	HTstrobe0;
	HTsend(HTwrite, 10);
	for (byte mtx = 0; mtx < 4; mtx++)  //sending 8x8-matrices left to right, rows top to bottom, MSB left
		for (byte row = 0; row < 8; row++) {  //while leds[] is organized in columns for ease of use.
			byte q = 0;
			for (byte col = 0; col < 8; col++)  q = (q << 1) | ((leds[col + (mtx << 3)] >> row) & 1);
			HTsend(q, 8);
		}
		HTstrobe1;
}

void HTsetup() {  //setting up the display
	HTcommand(HTstartsys);
	HTcommand(HTledon);
	HTcommand(HTsetclock);
	HTcommand(HTsetlayout);
	HTcommand(HTsetbright + (8 << 1));
	HTcommand(HTblinkoff);
}

void HTbrightness(byte b) {
	HTcommand(HTsetbright + ((b & 15) << 1));
}

inline void clocksetup() {  // CLOCK, interrupt every second
	ASSR |= (1 << AS2);    //timer2 async from external quartz
	TCCR2 = 0b00000101;    //normal,off,/128; 32768Hz/256/128 = 1 Hz
	TIMSK |= (1 << TOIE2); //enable timer2-overflow-int
	sei();               //enable interrupts
}

// CLOCK interrupt
//ISR(TIMER2_OVF_vect) {     //timer2-overflow-int
//	sec++;
//}

//void incsec(byte add) {
//	sec += add;
//	while (sec >= 60) {
//		sec -= 60;  minute++;
//		while (minute >= 60) {
//			minute -= 60;  hour++;
//			while (hour >= 24) {
//				hour -= 24;
//			}//24hours
//		}//60min
//	}//60sec
//}

void incsec(byte add) {
	sec += add;
	while (sec >= 60) {
		if (add == 0){  // we are not setting the clock
			getTime();
			return; 
		} else {
			sec -= 60;  minute++;
			while (minute >= 60) {
				minute -= 60;  hour++;
				while (hour >= 24) {
					hour -= 24;
				}//24hours
			}//60min
		}//60sec
	}
}

void decsec(byte sub) {
	while (sub > 0) {
		if (sec > 0) sec--;
		else {
			sec = 59;
			if (minute > 0) minute--;
			else {
				minute = 59;
				if (hour > 0) hour--;
				else { hour = 23;
				}
			}//hour
		}//minute
		sub--;
	}//sec
}

byte clockhandler(void) {
	if (sec == sec0) return 0;   //check if something changed
	sec0 = sec;
	incsec(0);  //just carry over
	return 1;
}

//----------- clock render ----------
void renderclock(void) {
	byte col = 0;
	for (byte i = 0; i < 6; i++) leds[col++] = pgm_read_byte(&bigdigits[hour / 10][i]);
	leds[col++] = 0;
	for (byte i = 0; i < 6; i++) leds[col++] = pgm_read_byte(&bigdigits[hour % 10][i]);
	leds[col++] = 0;
	if (sec % 2) {
		leds[col++] = 0x66;// leds[col++] = 0x66; 
	}
	else {
		leds[col++] = 0; // leds[col++] = 0; 
	}
	leds[col++] = 0;
	for (byte i = 0; i<6; i++) leds[col++] = pgm_read_byte(&bigdigits[minute / 10][i]);
	leds[col++] = 0;
	for (byte i = 0; i<6; i++) leds[col++] = pgm_read_byte(&bigdigits[minute % 10][i]);
	leds[col++] = 0;
	leds[col++] = (sec>6) + ((sec>13) << 1) + ((sec > 20) << 2) + ((sec > 26) << 3) + ((sec > 33) << 4) + ((sec > 40) << 5) + ((sec > 46) << 6) + ((sec > 53) << 7);
}

void renderTemperature(void) {
	int celcius = getTemp();  // Find the temperature in C
	// float celciusdecimal = (sensors.getTempCByIndex(0) - celcius) * 100;

	//int celcius = 1234;
	byte high = celcius / 100;
	byte low =  celcius % 100;
	byte col = 0;
	leds[col++] = 0;
	leds[col++] = 0;
	leds[col++] = 0;
	for (byte i = 0; i < 6; i++) leds[col++] = pgm_read_byte(&bigdigits[high / 10][i]);
	leds[col++] = 0;
	for (byte i = 0; i < 6; i++) leds[col++] = pgm_read_byte(&bigdigits[high %  10][i]);
	leds[col++] = 0;
	leds[col++] = B10000000;
	leds[col++] = 0;
	for (byte i = 0; i < 6; i++) leds[col++] = pgm_read_byte(&bigdigits[low / 10][i]);
	leds[col++] = 0;
	leds[col++] = B01100000;
	leds[col++] = B10010000;
	leds[col++] = B10010000;
	leds[col++] = 0;
	leds[col++] = 0;
	leds[col++] = 0;
}

void setup() {
	// Set the clock to run-mode, and disable the write protection
	rtc.halt(false);
	rtc.writeProtect(false);

	// The following lines can be commented out to use the values already stored in the DS1302
	//rtc.setDOW(FRIDAY);        // Set Day-of-Week to FRIDAY
	//rtc.setTime(15, 0, 0);     // Set the time to 12:00:00 (24hr format)
	//rtc.setDate(6, 8, 2010);   // Set the date to August 6th, 2010
	//// sensors.begin();
	HTpinsetup();
	HTsetup();
	keysetup();
	//clocksetup();

	for (byte i = 0; i<32; i++) leds[i] = 0b01010101 << (i % 2);  HTsendscreen();
	getTime();
} 

void loop(){
	// Get data from the DS1302


	if (key1) {
		if (changing>250) incsec(20);
		else { changing++; incsec(1); }
	}
	else if (key2) {
		if (changing > 250) decsec(20);
		else { changing++; decsec(1); }
	}
	else if (key3) {
		if (!changing) {
			changing = 1;
			bright = (bright + 1) % 4;
			HTbrightness(brights[bright]);
		}
	} else {  // no buttons are pressed.
		if (changing > 1){

		}
		
		changing = 0;
	}

	if (sec < 3 && changing == 0){
		renderTemperature();
		HTsendscreen();
	}
	else {
		HTsendscreen();
		if (clockhandler()) {
			renderclock();
			HTsendscreen();
		}
	}

	if (lastMillis + 1000 <= millis()){
		sec++;
		lastMillis = millis();
	}
}

void requestTemp(){
	// Reset the bus so that all devices are in listening mode
	tempSensor.reset();

	// Broadcast the address of the device so that it goes into listening mode
	tempSensor.skip();

	// the 0x44 command tells the tempSensor to measure the temp and store it in its scratchpad

	tempSensor.write(0x44, 1);
	// We wait a second because it takes time to take the measurement and store a value
	delay(1000);
}

int getTemp(){
	byte i;
	//byte present = 0;
	byte data[12];
	byte addr[8];

	int T_Reading;
	int Sign_Bit;
	int Tc_100; 

	//tempSensor.reset();
	//if(!tempSensor.search(addr)) {
	//	tempSensor.reset_search();
	//	// return;
	//}
	// Reset the bus so that all devices are in listening mode
	tempSensor.reset();

	// Broadcast the address of the device so that it goes into listening mode
	//tempSensor.select(addr[0]);

	tempSensor.skip();
	// the 0x44 command tells the tempSensor to measure the temp and store it in its scratchpad

	tempSensor.write(0x44, 1);
	// We wait a second because it takes time to take the measurement and store a value
	delay(1000);
	// Hopefully there will be a couple of bits in the scratchpad that represent the temp

	tempSensor.reset();
	tempSensor.skip();
	tempSensor.write(0xBE);

	// The tempSensor will now send us 9 bytes (bytes 0 - 8)
	for(i=0;i<9;i++){
		data[i]=tempSensor.read();
	}

	// bytes 0 and 1 are where the temperature is stored
	//data[1] = B00000001;
	//data[0] = B10010001;

	T_Reading=(data[1] << 8) + data[0];
	Sign_Bit=T_Reading & 0x8000;  // test most sig bit
	if(Sign_Bit){  // check to see if the sign bit indicates a negative value
		T_Reading=(T_Reading ^ 0xffff) + 1;  // 2's comp
	}

	// We will now calculate the temperature * 100.  This means I can avoid using floats 
	// which keeps the program size down!

	// Uncomment if using the tempSensor part which is 12bit resolution by default
	//Tc_100 = (6 * T_Reading) + (T_Reading / 4);  // multiply by (100 * 0.0625) or 6.25

	// uncomment this if using DS1820 (and DS18S20) part which is 9 bit resolution
	Tc_100 = (T_Reading)*50;  //  This is the raw reading /2*100

	return Tc_100;
}

void getTime(){

	t = rtc.getTime();
	hour = t.hour;
	minute =  t.min;
	sec = t.sec;
}