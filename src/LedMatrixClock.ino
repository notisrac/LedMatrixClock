#include <RTClib.h>
#include <LedControl.h>


const byte digits[10][8] PROGMEM = {
	{ // 0
		 B00000000
		,B00000111
		,B00000101
		,B00000101
		,B00000101
		,B00000101
		,B00000111
		,B00000000
	},
	{ // 1
		 B00000000
		,B00000010
		,B00000010
		,B00000010
		,B00000010
		,B00000010
		,B00000010
		,B00000000
	},
	{ // 2
		 B00000000
		,B00000111
		,B00000001
		,B00000001
		,B00000111
		,B00000100
		,B00000111
		,B00000000
	},
	{ // 3
		 B00000000
		,B00000111
		,B00000001
		,B00000001
		,B00000011
		,B00000001
		,B00000111
		,B00000000
	},
	{ // 4
		 B00000000
		,B00000101
		,B00000101
		,B00000101
		,B00000111
		,B00000001
		,B00000001
		,B00000000
	},
	{ // 5
		 B00000000
		,B00000111
		,B00000100
		,B00000100
		,B00000111
		,B00000001
		,B00000111
		,B00000000
	},
	{ // 6
		 B00000000
		,B00000111
		,B00000100
		,B00000100
		,B00000111
		,B00000101
		,B00000111
		,B00000000
	},
	{ // 7
		 B00000000
		,B00000111
		,B00000001
		,B00000001
		,B00000001
		,B00000001
		,B00000001
		,B00000000
	},
	{ // 8
		 B00000000
		,B00000111
		,B00000101
		,B00000101
		,B00000111
		,B00000101
		,B00000111
		,B00000000
	},
	{ // 9
		 B00000000
		,B00000111
		,B00000101
		,B00000101
		,B00000111
		,B00000001
		,B00000111
		,B00000000
	},
};

const byte errorMessage[2][8] PROGMEM = {
	{ // Er
		 B00000000
		,B11100000
		,B10000000
		,B10000110
		,B11001000
		,B10001000
		,B11101000
		,B00000000
	},
	{ // r!
		 B00000000
		,B00000100
		,B00000100
		,B01100100
		,B10000100
		,B10000000
		,B10000100
		,B00000000
	}
};

const char daysOfWeek[7][10] PROGMEM = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

/*
* Now we create a new LedControl.
* We use pins 12,11 and 10 on the Arduino for the SPI interface
* Pin 12 is connected to the DATA IN-pin of the first MAX7221
* Pin 11 is connected to the CLK-pin of the first MAX7221
* Pin 10 is connected to the LOAD(/CS)-pin of the first MAX7221
* There will only be a single MAX7221 attached to the arduino
*/
LedControl lc1 = LedControl(12, 11, 10, 2);
RTC_DS3231 rtc;
uint8_t prevMinutes = 99; // 99 fixes a bug: if you turn the unit on at xx:00 the 00 will not display beacuse the currentminutes equal the prevminutes
uint8_t prevHour = 99; // 99 fixes a bug: if you turn the unit on at 00:xx the 00 will not display beacuse the currenthour equal the prevhour

const uint8_t photocellPin = A3;
int photocellReading = 0;
uint8_t displayBrightness = 8;

const uint8_t buzzerPin = 9;

const uint8_t semicolonUpperPin = 5;
const uint8_t semicolonLowerPin = 6;

const uint8_t latchPin = A1;
const uint8_t dataPin = A2;
const uint8_t clockPin = A0;
byte buttonStates = 0;

const byte ledMatrixCount = 2;
// original
//const byte ledMatrixLeft = 0;
//const byte ledMatrixRight = 1;
// upside down
const byte ledMatrixLeft = 1;
const byte ledMatrixRight = 0;

/*
 O4 O3 O2 O1 O6
    O5    O5
*/
const byte buttonMaskEdit =   B00001000; // 4
const byte buttonMaskUp =     B00000100; // 3
const byte buttonMaskDown =   B00000010; // 2
const byte buttonMaskAlarm =  B00000001; // 1
const byte buttonMaskDate =   B00100000; // 6
const byte buttonMaskSnooze = B00010000; // 5
bool editMode = false;
bool editHours = false;
bool editMinutes = false;

DateTime editDateTime;
const TimeSpan oneHourTimeSpan = TimeSpan(0, 1, 0, 0);
const TimeSpan oneMinuteTimeSpan = TimeSpan(0, 0, 1, 0);

/*
* just needs the location of the data pin and the clock pin
* it returns a byte with each bit in the byte corresponding
* to a pin on the shift register. leftBit 7 = Pin 7 / Bit 0= Pin 0
*/
byte shiftIn(int myDataPin, int myClockPin) {
	int i;
	int temp = 0;
	int pinState;
	byte myDataIn = 0;

	pinMode(myClockPin, OUTPUT);
	pinMode(myDataPin, INPUT);

	//we will be holding the clock pin high 8 times (0,..,7) at the
	//end of each time through the for loop

	//at the begining of each loop when we set the clock low, it will
	//be doing the necessary low to high drop to cause the shift
	//register's DataPin to change state based on the value
	//of the next bit in its serial information flow.
	//The register transmits the information about the pins from pin 7 to pin 0
	//so that is why our function counts down
	for (i = 7; i >= 0; i--)
	{
		digitalWrite(myClockPin, 0);
		delayMicroseconds(2);
		temp = digitalRead(myDataPin);
		if (temp) {
			pinState = 1;
			//set the bit to 0 no matter what
			myDataIn = myDataIn | (1 << i);
		}
		else {
			//turn it off -- only necessary for debuging
			//print statement since myDataIn starts as 0
			pinState = 0;
		}

		//Debuging print statements
		//Serial.print(pinState);
		//Serial.print("     ");
		//Serial.println (dataIn, BIN);

		digitalWrite(myClockPin, 1);

	}
	//debuging print statements whitespace
	//Serial.println();
	//Serial.println(myDataIn, BIN);
	return myDataIn;
}

/*
* Flips a byte. Ex.: 01011 will become 11010
* source: the byte to flip
* returns: the byte with the bits in reversed order
*/
byte flipByte(byte source) {
	char result = 0;
	for (byte i = 0; i < 8; i++) {
		result <<= 1;
		result |= source & 1;
		source >>= 1;
	}
	return result;
}

/*
* Display a two digit number on a 8x8  led matrix, with the 4x8 font set
* addr: address of the display
* number: the number to display
*/
void displayNumber(size_t addr, int number)
{
	byte ones = number % 10;
	byte tens = number / 10;

	for (size_t digitLine = 0; digitLine < 8; digitLine++)
	{
		byte digitOnes = pgm_read_byte(&digits[ones][digitLine]) << (-1 + (addr * 2)); // orig: 1 - addr???
		byte digitTens = pgm_read_byte(&digits[tens][digitLine]) << (-1 + (addr * 2));
		//lc1.setColumn(addr, digitLine, (digitTens << 4) | digitOnes); // original
		lc1.setColumn(addr, 7 - digitLine, flipByte((digitTens << 4) | digitOnes)); // upside down
	}
}

void displayError()
{
	for (size_t i = 0; i < ledMatrixCount; i++)
	{
		for (size_t digitLine = 0; digitLine < 8; digitLine++)
		{
			byte singleLine = pgm_read_byte(&errorMessage[i][digitLine]);
			lc1.setColumn(i, digitLine, singleLine);
		}
	}
}

char* dateTimeToString(DateTime dt)
{
	char dayOfWeek[10];
	//byte i = 0;
	//bool running = true;
	//while (running)
	//{
	//	dayOfWeek[i] = pgm_read_byte(&daysOfWeek[dt.dayOfTheWeek()][i]);
	//	i++;
	//	if (i > 9 || dayOfWeek[i - 1] == '\0')
	//	{
	//		running = false;
	//	}
	//}
	for (size_t i = 0; i < 10; i++)
	{
		dayOfWeek[i] = pgm_read_byte(&daysOfWeek[dt.dayOfTheWeek()][i]);
	}

	// 2014.03.26 19:55:37 sunday
	char dateTimeString[30];
	sprintf_P(dateTimeString, (char*)F("%d.%02d.%02d %02d:%02d:%02d %s"), dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), dayOfWeek);
	return dateTimeString;
}

void displayEditMode()
{
	if (!editMode)
	{
		lc1.setColumn(ledMatrixLeft, 0, B00000000);
		lc1.setColumn(ledMatrixLeft, 7, B00000000);
		lc1.setColumn(ledMatrixRight, 0, B00000000);
		lc1.setColumn(ledMatrixRight, 7, B00000000);
	}
	else if (editHours)
	{
		lc1.setColumn(ledMatrixLeft, 0, B11111111);
		lc1.setColumn(ledMatrixLeft, 7, B11111111);
		lc1.setColumn(ledMatrixRight, 0, B00000000);
		lc1.setColumn(ledMatrixRight, 7, B00000000);
	}
	else if (editMinutes)
	{
		lc1.setColumn(ledMatrixLeft, 0, B00000000);
		lc1.setColumn(ledMatrixLeft, 7, B00000000);
		lc1.setColumn(ledMatrixRight, 0, B11111111);
		lc1.setColumn(ledMatrixRight, 7, B11111111);
	}
}


void setup()
{
	Serial.begin(115200);
	delay(100);

	editMode = false;
	editHours = false;
	editMinutes = false;

	/*
	The MAX72XX is in power-saving mode on startup,
	we have to do a wakeup call
	*/
	lc1.shutdown(ledMatrixLeft, false);
	lc1.shutdown(ledMatrixRight, false);
	/* Set the brightness to a medium values */
	lc1.setIntensity(ledMatrixLeft, displayBrightness);
	lc1.setIntensity(ledMatrixRight, displayBrightness);
	/* and clear the display */
	lc1.clearDisplay(ledMatrixLeft);
	lc1.clearDisplay(ledMatrixRight);

	//displayNumber(0, 5);
	//displayNumber(1, 7);
	//displayError();
	//while (1);

	if (!rtc.begin()) {
		Serial.println(F("Couldn't find RTC"));
		displayError();
		while (1);
	}

	if (rtc.lostPower()) {
		Serial.println(F("RTC lost power, lets set the time!"));
		// following line sets the RTC to the date & time this sketch was compiled
		rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
		// This line sets the RTC with an explicit date & time, for example to set
		// January 21, 2014 at 3am you would call:
		// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
	}
	Serial.println(F("PixelMatrix Clock"));
	Serial.println(F("2016, noti"));
	Serial.println(dateTimeToString(rtc.now()));

	// buzzer
	pinMode(buzzerPin, OUTPUT);
	//noTone(buzzerPin);

	// semicolon
	pinMode(semicolonUpperPin, OUTPUT);
	pinMode(semicolonLowerPin, OUTPUT);
	digitalWrite(semicolonUpperPin, HIGH);
	digitalWrite(semicolonLowerPin, HIGH);

	// buttons
	pinMode(latchPin, OUTPUT);
	pinMode(clockPin, OUTPUT);
	pinMode(dataPin, INPUT);
}

void loop()
{
	DateTime now;
	DateTime nowRTC = rtc.now();
	if (editMode)
	{
		now = editDateTime;
	}
	else
	{
		now = nowRTC;
	}

	// display the hour
	uint8_t currentHour = now.hour();
	if (currentHour != prevHour)
	{
		//Serial.println(dateTimeToString(now));
		displayNumber(ledMatrixLeft, currentHour);
		prevHour = currentHour;
	}

	// display the minutes
	uint8_t currentMinutes = now.minute();
	if (currentMinutes != prevMinutes)
	{
		//Serial.println(dateTimeToString(now));
		displayNumber(ledMatrixRight, currentMinutes);
		prevMinutes = currentMinutes;
	}

	// adjust the brightness
	photocellReading = analogRead(photocellPin);
	uint8_t TMPdisplayBrightness = map(photocellReading, 0, 1023, 0, 15);
	if (((TMPdisplayBrightness + displayBrightness) / 2) != displayBrightness)
	{
		Serial.println(TMPdisplayBrightness);
		displayBrightness = TMPdisplayBrightness;
		lc1.setIntensity(ledMatrixLeft, displayBrightness);
		lc1.setIntensity(ledMatrixRight, displayBrightness);
	}

	// buttons
	//Pulse the latch pin:
	//set it to 1 to collect parallel data
	digitalWrite(latchPin, 1);
	//set it to 1 to collect parallel data, wait
	delayMicroseconds(20);
	//set it to 0 to transmit data serially  
	digitalWrite(latchPin, 0);

	//while the shift register is in serial mode
	//collect each shift register into a byte
	//the register attached to the chip comes in first
	byte newButtonStates = shiftIn(dataPin, clockPin);
	if (buttonStates != newButtonStates)
	{
		//Print out the results.
		//leading 0's at the top of the byte
		//(7, 6, 5, etc) will be dropped before
		//the first pin that has a high input
		//reading  
		buttonStates = newButtonStates;
		Serial.println(buttonStates, BIN);

		if (buttonStates & buttonMaskEdit)
		{
			if (!editMode && !editHours && !editMinutes)
			{
				Serial.println(F("Edit mode: hours"));
				editMode = true;
				editHours = true;
				editMinutes = false;
				editDateTime = rtc.now();
			}
			else if (editMode && editHours && !editMinutes)
			{
				Serial.println(F("Edit mode: minutes"));
				editMode = true;
				editHours = false;
				editMinutes = true;
			}
			else if (editMode && !editHours && editMinutes)
			{
				Serial.println(F("Edit mode: off"));
				editMode = false;
				editHours = false;
				editMinutes = false;
				rtc.adjust(DateTime(editDateTime.year(), editDateTime.month(), editDateTime.day(), editDateTime.hour(), editDateTime.minute(), 0));
			}
		}

		if ((buttonStates & buttonMaskUp) && editMode && editHours)
		{
			Serial.println(F("Edit mode: added an hour"));
			editDateTime = editDateTime + oneHourTimeSpan;
		}

		if ((buttonStates & buttonMaskDown) && editMode && editHours)
		{
			Serial.println(F("Edit mode: subtracted an hour"));
			editDateTime = editDateTime - oneHourTimeSpan;
		}

		if ((buttonStates & buttonMaskUp) && editMode && editMinutes)
		{
			Serial.println(F("Edit mode: added a minute"));
			editDateTime = editDateTime + oneMinuteTimeSpan;
		}

		if ((buttonStates & buttonMaskDown) && editMode && editMinutes)
		{
			Serial.println(F("Edit mode: subtracted a minute"));
			editDateTime = editDateTime - oneMinuteTimeSpan;
		}
	}

	displayEditMode();

	// DST!
	//if (3 == nowRTC.month() && nowRTC.)
	//{

	//}

	delay(100);

	//while (Serial.available() > 0) {

	//	// look for the next valid integer in the incoming serial stream:
	//	int red = Serial.parseInt();
	//	// do it again:
	//	int green = Serial.parseInt();
	//	// do it again:
	//	int blue = Serial.parseInt();

	//	// look for the newline. That's the end of your
	//	// sentence:
	//	if (Serial.read() == '\n') {
	//		// constrain the values to 0 - 255 and invert
	//		// if you're using a common-cathode LED, just use "constrain(color, 0, 255);"
	//		red = 255 - constrain(red, 0, 255);
	//		green = 255 - constrain(green, 0, 255);
	//		blue = 255 - constrain(blue, 0, 255);

	//		// fade the red, green, and blue legs of the LED:
	//		analogWrite(redPin, red);
	//		analogWrite(greenPin, green);
	//		analogWrite(bluePin, blue);

	//		// print the three numbers in one string as hexadecimal:
	//		Serial.print(red, HEX);
	//		Serial.print(green, HEX);
	//		Serial.println(blue, HEX);
	//	}
	//}
}
