#include "SevSeg.h"
#include "Thread.h"

const unsigned char ledPin = 13;

volatile char pulseCount = 0;

const unsigned char clockInPin = 2;
const unsigned char inMemoryConfrm = 3;

const unsigned char inUpDownStatus = 4;

const unsigned char outSignalUp = 16;
const unsigned char outSignalDown = 17;

char buffr[250];

SevSeg sevseg; //Instantiate a seven segment controller object

// Pin toggle thread !!
Thread toggleThread = Thread();

// callback for toggleThread
void toggleCallback(){
  static bool pinStatus = false;
  pinStatus = !pinStatus;
  digitalWrite(ledPin, pinStatus);

  // Print the count value 
  sprintf(buffr, "Current counter value : %d", pulseCount);
  Serial.println(buffr);
}

void ISRPulseCount() {
  if(digitalRead(inUpDownStatus)) {
    pulseCount++;
    if(pulseCount > 99) pulseCount = 0;
  } else {
    pulseCount--;
    if(pulseCount < 0) pulseCount = 0;
  }
}

void setup() {

	// Display setup
	byte numDigits = 2;
	byte digitPins[] = {14, 15};
	byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12};
	bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
	byte hardwareConfig = P_TRANSISTORS; // See README.md for options
	bool updateWithDelays = false; // Default. Recommended
	bool leadingZeros = true; // Use 'true' if you'd like to keep the leading zeros

	sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
	sevseg.setBrightness(70);

	Serial.begin(115200);
  
  pinMode(ledPin, OUTPUT);
  
  pinMode(inUpDownStatus, INPUT);
  pinMode(inMemoryConfrm, INPUT_PULLUP);

  pinMode(outSignalUp, OUTPUT);
  pinMode(outSignalDown, OUTPUT);

  digitalWrite(outSignalUp, LOW);
  digitalWrite(outSignalDown, LOW);

  pinMode(clockInPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(clockInPin), ISRPulseCount, FALLING);

  // Init thread
  toggleThread.onRun(toggleCallback);
  toggleThread.setInterval(1000);
}

void loop() {

  // checks if thread should run
  if(toggleThread.shouldRun())
    toggleThread.run();
    
  if(digitalRead(inUpDownStatus)) {
    digitalWrite(outSignalUp, HIGH);
    digitalWrite(outSignalDown, LOW);
  } else {
    digitalWrite(outSignalUp, LOW);
    digitalWrite(outSignalDown, HIGH);
  } 

  sevseg.setNumber(pulseCount, 0);
  sevseg.refreshDisplay(); // Must run repeatedly
}

