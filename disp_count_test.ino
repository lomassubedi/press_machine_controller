#include "SevSeg.h"
#include "Thread.h"

#define     MEM_SIZE    4

const unsigned char ledPin = 13;

volatile char pulseCount = 0;

const unsigned char clockInPin = 2;

const unsigned char inTrigRun = 3;

const unsigned char inMemoryConfrm = 18;

const unsigned char inUpDownStatus = 4;

const unsigned char inSetting = 5;

const unsigned char outSignalUp = 16;
const unsigned char outSignalDown = 17;

volatile bool flag_setting = false;
volatile bool flag_key_trig = false;

char buffr[250];
volatile char memVal[MEM_SIZE];
volatile char memCount = 0;
volatile char memGetIndex = 0;

volatile char prevDisp = 0;

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
  
  if (!flag_setting) 
    return;
    
  if(digitalRead(inUpDownStatus)) {
    pulseCount++;
    if(pulseCount > 99) pulseCount = 0;
  } else {
    pulseCount--;
    if(pulseCount < 0) pulseCount = 0;
  }
}

void ISRTrigRun() {
  flag_key_trig = true;
  return;
}

bool getSettingIn(void) {
  if(digitalRead(inSetting)) return false;
  else return true;
}

bool getUpDownIn(void) {
  if(digitalRead(inUpDownStatus)) return true;
  else return false;
}

bool getMemSetIn(void) {
  if (digitalRead(inMemoryConfrm)) {
    delay(50);
    while(digitalRead(inMemoryConfrm));
    return true;
  }
  else return false;
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

  pinMode(inSetting, INPUT_PULLUP);

  digitalWrite(outSignalUp, LOW);
  digitalWrite(outSignalDown, LOW);

  pinMode(clockInPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(clockInPin), ISRPulseCount, RISING);

  pinMode(inMemoryConfrm, INPUT);

  pinMode(inTrigRun, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(inTrigRun), ISRTrigRun, FALLING);

  // Init thread
  toggleThread.onRun(toggleCallback);
  toggleThread.setInterval(1000);
}

void loop() {

  // checks if thread should run
  if(toggleThread.shouldRun())
    toggleThread.run();
    
  if(getUpDownIn() && getSettingIn()) {
    flag_setting = true;
    digitalWrite(outSignalUp, HIGH);
    digitalWrite(outSignalDown, LOW);
  } else if((!getUpDownIn()) && getSettingIn()) {
    flag_setting = true;
    digitalWrite(outSignalUp, LOW);
    digitalWrite(outSignalDown, HIGH);
  } else {
    flag_setting = false;
    digitalWrite(outSignalUp, LOW);
    digitalWrite(outSignalDown,LOW);
  }


  // Memory Set 
  if(getMemSetIn() && (!flag_setting)) { // If only display is on stop
    memVal[memCount] = pulseCount;
    memCount++;                         // Increase memory count 
    if(memCount > 4) memCount = 0;     // Reset memory on overflow
    sprintf(buffr, "Memory set at value %d.\tMemory index: %d.",pulseCount, memCount);
    Serial.println(buffr);
  }

  // Memory trigger 
  if(flag_key_trig) {   
     
    if(memCount) {  // If only memory has been set
      if(pulseCount > memVal[memGetIndex]) {
        // Turn ON the Down Signal
        digitalWrite(outSignalUp, LOW);
        digitalWrite(outSignalDown, HIGH);
//        Serial.println("I am here on Down !!");
      } else if(pulseCount < memVal[memGetIndex]) {
        // Turn ON the UP signal
        digitalWrite(outSignalUp, HIGH);
        digitalWrite(outSignalDown, LOW);
      } else {
        // Do nothing
        digitalWrite(outSignalUp, LOW);
        digitalWrite(outSignalDown,LOW);
      }    
      pulseCount = memVal[memGetIndex];
      memGetIndex++;  
      if(memGetIndex >= memCount)  memGetIndex = 0;
    }

    flag_key_trig = false;
  }


  sevseg.setNumber(pulseCount, 0);
  sevseg.refreshDisplay(); // Must run repeatedly
}

