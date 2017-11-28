#include "SevSeg.h"
#include "Thread.h"

//Instantiate a seven segment controller object
SevSeg sevseg;

// Pin toggle thread !!
Thread toggleThread = Thread();

const unsigned char clockInPin = 2;       // Interrupt enabled Clock receive
const unsigned char inTrigRun = 3;        // Interrupt enabled trigger/run 
const unsigned char inUpDownStatus = 4;   // Up/Down Status Pin
const unsigned char inSetting = 5;
const unsigned char ledPin = 13;
const unsigned char outSignalUp = 16;
const unsigned char outSignalDown = 17;
const unsigned char inMemoryConfrm = 18;

volatile int pulseCount = 0;
volatile bool flag_setting = false;
volatile bool flag_key_trig = false;

#define     MEM_SIZE    4

char buffr[250];
volatile int memVal[MEM_SIZE];
volatile char memCount = 0;
volatile char memGetIndex = 0;
volatile int prevDisp = 0;


// callback for toggleThread
void toggleCallback() {
  static bool pinStatus = false;
  pinStatus = !pinStatus;
  digitalWrite(ledPin, pinStatus);

  // Print the count value 
  sprintf(buffr, "Current counter value : %d", pulseCount);
  Serial.println(buffr);
  Serial.println(" ------ Memory buffer content -------- ");
  Serial.println("MEM0   MEM1    MEM2    MEM3");
  sprintf(buffr, "%d    %d    %d    %d", memVal[0], memVal[1], memVal[2], memVal[3]);  
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
    delay(50);    // Debounce Delay
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
  if(getMemSetIn() && (!flag_setting)) {  // If only display is on stop
    memCount = memCount % MEM_SIZE;       // Reset memory on overflow
    memVal[memCount++] = pulseCount;
    sprintf(buffr, "Memory set at value %d.\tMemory index: %d.",pulseCount, memCount);
    Serial.println(buffr);
  }
  
//  if(flag_key_trig) {
//    
//  }  

  sevseg.setNumber(pulseCount, 0);
  sevseg.refreshDisplay(); // Must run repeatedly
}

