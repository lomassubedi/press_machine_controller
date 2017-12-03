#include "SevSeg.h"
#include "Thread.h"

#define     MEM_SIZE    4
//Instantiate a seven segment controller object
SevSeg sevseg;
// Pin toggle thread !!
Thread toggleThread = Thread();
Thread printDispThread = Thread();

const unsigned char clockInPin = 2;       // Interrupt enabled Clock receive
const unsigned char inTrigRun = 3;        // Interrupt enabled trigger/run 
const unsigned char inUpDownStatus = 4;   // Up/Down Status Pin
const unsigned char inSetting = 5;
const unsigned char ledPin = 13;
const unsigned char outSignalUp = 16;
const unsigned char outSignalDown = 17;
const unsigned char inMemoryConfrm = 18;

volatile int pulseCount = 0;
volatile bool flag_pulse_detected = false;
volatile bool flag_last_state_set = false;
volatile bool flag_key_trig = false;
volatile bool flag_run_up = false;
volatile bool flag_run_down = false;
volatile bool flag_setting_done = false;

char buffr[250];
volatile int memVal[MEM_SIZE];
volatile char memCount = 0;
volatile char memGetIndex = 0;
volatile int prevDisp = 0;
volatile unsigned char trigCount = 0;
uint16_t prevCountVal = 0;

// callback for toggleThread
void toggleCallback() {

	static bool pinStatus = false;
	pinStatus = !pinStatus;
	digitalWrite(ledPin, pinStatus);
    
}

// Callback for print message
void printDebugMessage() {
  // Print the count value 
  Serial.println("---------------- Debug Console ----------------");
  Serial.println("MEM1   MEM2    MEM3    MEM4");
  sprintf(buffr, "%d     %d      %d      %d", memVal[0], memVal[1], memVal[2], memVal[3]);  
  Serial.println(buffr);
  sprintf(buffr, "TriggerCount: %d, MemCount: %d",trigCount, memCount);
  Serial.println(buffr); 
  sprintf(buffr, "Current Disp Val : %d, Current Memval(trigCount): %d", pulseCount, memVal[trigCount]);
  Serial.println(buffr);
  Serial.println("-----------------------------------------------\r\n");  
}

void ISRPulseCount() {
	flag_pulse_detected = true;
}

void ISRTrigRun() {
    
	delay(50); // Debounce time  

	if(!digitalRead(inTrigRun)) {
		flag_key_trig = true;
		Serial.println("Got trigger/run command from user!!");    
	}
	return;
}

bool getSettingIn(void) {
	if(!digitalRead(inSetting)) return true;
	else return false;
}

bool getUpDownIn(void) {
	if(digitalRead(inUpDownStatus)) return true;
	else return false;
}

bool getMemSetIn(void) {
	if (!digitalRead(inMemoryConfrm)) {
		delay(30);                        // Debounce Delay
   
		while(!digitalRead(inMemoryConfrm)) {
      sevseg.setNumber(1000, 0);
      sevseg.refreshDisplay();
		}
   delay(30);
   return true;

//    if(!digitalRead(inMemoryConfrm))
//		  return true;
//    else return false;
	} else return false;
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

void run(void) {
	if(flag_pulse_detected) {
		flag_pulse_detected = false;
		if(getUpDownIn()) {
			outUp();
			pulseCount++;		
			if(pulseCount > 999) pulseCount = 0;
		} else {
			outDown();
			pulseCount--;		
			if(pulseCount < 0) pulseCount = 0;
		} 
	} 
}

void runUp(void) {

	outUp();
	
	if(flag_pulse_detected) {
		flag_pulse_detected = false;	
		pulseCount++;		
		if(pulseCount > 999) pulseCount = 0;
	} 
}

void runDown(void) {

	outDown();

	if(flag_pulse_detected) {
		flag_pulse_detected = false;	
		pulseCount--;		
		if(pulseCount < 0) pulseCount = 0;
	} 
}

void stop(void) {
 	outClear();
}

void handleTrigger(void) {

	if(memCount < 1) {
		stop();
		return; 									// if no memory has been set return 
	}

//	if(flag_last_state_set || (memCount == 1)) { 	// Stay display halted at first memory count
//		flag_last_state_set = false;
//		pulseCount = memVal[0];						// Come to initial memory location
//		stop();
//		return;
//    }


	if(pulseCount < memVal[trigCount]) {
		flag_run_up = true;                   		// Output at up
	} else if(pulseCount > memVal[trigCount]) {
		flag_run_down = true;                 		// Output at Down
	} else {
		// stop();
	} 	

  prevCountVal = trigCount;
  trigCount++; 
	trigCount = trigCount % memCount;

//	if(prevCountVal == memCount) {
//    pulseCount = memVal[0];           // Come to initial memory location
//    stop();
//    return;
//	}

}

void setup() {

	// Display setup
	byte numDigits = 3;
	byte digitPins[] = {14, 15, 19};
	byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12};
	bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
	byte hardwareConfig = N_TRANSISTORS; // See README.md for options
	bool updateWithDelays = false; // Default. Recommended
	bool leadingZeros = true; // Use 'true' if you'd like to keep the leading zeros

	sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
	sevseg.setBrightness(70);

	Serial.begin(115200);

	pinMode(ledPin, OUTPUT);
	pinMode(inUpDownStatus, INPUT_PULLUP);
	pinMode(inMemoryConfrm, INPUT_PULLUP);
	pinMode(outSignalUp, OUTPUT);
	pinMode(outSignalDown, OUTPUT);
	pinMode(inSetting, INPUT_PULLUP);  

	pinMode(clockInPin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(clockInPin), ISRPulseCount, FALLING);
 
	pinMode(inTrigRun, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(inTrigRun), ISRTrigRun, FALLING);  

	// Init Outputs
	digitalWrite(outSignalUp, LOW);
	digitalWrite(outSignalDown, LOW);

	// Init thread
	toggleThread.onRun(toggleCallback);
	toggleThread.setInterval(200);

   printDispThread.onRun(printDebugMessage);
   printDispThread.setInterval(1000);
}

void loop() {

  // checks if thread should run
	if(toggleThread.shouldRun())
		toggleThread.run();
  /* uncomment if dibug message required */
//  if(printDispThread.shouldRun())
//    printDispThread.run();
    
	if(!flag_setting_done) {
  		if(getSettingIn()) {
  		run();
  		} else {
  			stop();
  		}
  
    	// Memory Set 
  	if(getMemSetIn() && (!getSettingIn())) {  // If only display is on stop  
  		
  		memCount = memCount % MEM_SIZE;       	// Reset memory on overflow
  		memVal[memCount++] = pulseCount;
  
  		sprintf(buffr, "Memory set at value %d.\tMemory index: %d.",pulseCount, memCount);
  		Serial.println(buffr);
  		trigCount = 0;
  		flag_last_state_set = true;
    	}
	}


	// If trigger/run key is pressed !!
	if(flag_key_trig) { 
		flag_key_trig	= false;
		flag_setting_done = true;
		handleTrigger();
	}

	// Continious run up or down
	if(flag_run_up) {
		runUp();
		if(pulseCount == memVal[prevCountVal]) {
			flag_run_up = false;
			stop();
		}
	}

	if(flag_run_down) {
		runDown();
		if(pulseCount == memVal[prevCountVal]) {
			flag_run_down = false;
			stop();
		}
	}

	sevseg.setNumber(pulseCount, 0);
	sevseg.refreshDisplay(); 							// Must run repeatedly
}

