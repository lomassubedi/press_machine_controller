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
volatile bool flag_last_state_set = false;

volatile bool flag_run_counter = false;
volatile bool flag_run_count_up = false;
volatile bool flag_run_count_down = false;
volatile bool flag_last_state_trig = false;

#define     MEM_SIZE    4

char buffr[250];
volatile int memVal[MEM_SIZE];
volatile char memCount = 0;
volatile char memGetIndex = 0;
volatile int prevDisp = 0;
volatile unsigned char trigCount = 0;


// callback for toggleThread
void toggleCallback() {
  static bool pinStatus = false;
  pinStatus = !pinStatus;
  digitalWrite(ledPin, pinStatus);

  // Print the count value 
  Serial.println("---------------- Debug Console ----------------");
  sprintf(buffr, "Current counter value : %d", pulseCount);
  Serial.println(buffr);
  Serial.println(" ------ Memory buffer content -------- ");
  Serial.println("MEM1   MEM2    MEM3    MEM4");
  sprintf(buffr, "%d     %d      %d      %d", memVal[0], memVal[1], memVal[2], memVal[3]);  
  Serial.println(buffr);
  sprintf(buffr, "TriggerCount: %d, MemCount: %d",trigCount, memCount);
  Serial.println(buffr); 
  Serial.println("-----------------------------------------------\r\n");
}

void ISRPulseCount() {
  
  if (flag_setting || flag_run_counter) {
//    Serial.println("All flag okay at interrupt.");
    if(digitalRead(inUpDownStatus) || flag_run_count_up) {
      pulseCount++;
      if(pulseCount > 999) pulseCount = 0;
    } else {
      pulseCount--;
      if(pulseCount < 0) pulseCount = 0;
    }
  }
}

void ISRTrigRun() {
  flag_key_trig = true;
  Serial.println("Got trigger/run command from user");
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
    delay(50);                        // Debounce Delay
    while(digitalRead(inMemoryConfrm));
    return true;
  }
  else return false;
}

void outUp(void) {
  digitalWrite(outSignalUp, HIGH);
  digitalWrite(outSignalDown, LOW);
}

void outDown(void) {
  digitalWrite(outSignalUp, LOW);
  digitalWrite(outSignalDown, HIGH);
}

void outClear(void) {
  digitalWrite(outSignalUp, LOW);
  digitalWrite(outSignalDown, LOW);
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

  pinMode(inMemoryConfrm, INPUT);

  delay(1000);
  
  pinMode(clockInPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(clockInPin), ISRPulseCount, RISING);
  
  pinMode(inTrigRun, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(inTrigRun), ISRTrigRun, RISING);  

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
    outUp();
  } else if((!getUpDownIn()) && getSettingIn()) {
    flag_setting = true;
    outDown();
  } else {
    flag_setting = false;
    outClear();
  }


  // Memory Set 
  if(getMemSetIn() && (!flag_setting)) {  // If only display is on stop
    memCount = memCount % MEM_SIZE;       // Reset memory on overflow
    memVal[memCount++] = pulseCount;
    
    flag_last_state_set = true;
    trigCount = 0;
    
    sprintf(buffr, "Memory set at value %d.\tMemory index: %d.",pulseCount, memCount);
    Serial.println(buffr);
  }

  // ---- If trigger pressed and system is running in Normal mode
  if(flag_key_trig) {
    Serial.println("Processing trigger key in here !!");
    flag_key_trig = false;
    if(flag_last_state_set || (memCount == 1)) { // Stay display halted at first memory count
      flag_last_state_set = false;
      pulseCount = memVal[trigCount];
    } else {
            
      trigCount = trigCount % memCount;
      trigCount++;
      
      if(pulseCount < memVal[trigCount]) {
        outDown();                  // Output at Down
        flag_run_counter = true;    // Run the counter process
//        flag_last_state_trig = true;
      } else if(pulseCount > memVal[trigCount]) {
        outUp();                    // Output at up
        flag_run_counter = true;   // Run the counter process
//        flag_last_state_trig = true;
      } else {
        outClear();
//        flag_run_counter = false;
      }
    }
    flag_last_state_trig = true;
  }

  if(flag_last_state_trig) {
    flag_last_state_trig = false;
    if(pulseCount == memVal[trigCount]) {
      outClear();
      flag_run_counter = false;    
    }
  }

  sevseg.setNumber(pulseCount, 0);
  sevseg.refreshDisplay(); // Must run repeatedly
}

