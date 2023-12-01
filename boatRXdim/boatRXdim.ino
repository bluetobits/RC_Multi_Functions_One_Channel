/*
 * Multiple functions from auxiliary RC channel (3) Steve Lomax 2023 free for personal use only. 
 * This monitors a 3 position switch on an auxiliary RC channel.
 * Originally written to control cabin lights and fire water cannons on a model boat from Aux channel 3.
 *    2 devices on one channel (named WATER and LIGHTS here)
 *    Water channel has pre-set PWM level, 
 *    LIGHTS channel has remote changable variable PWM level 
 * Setup:
 *    Arduino (pro mini used but UNO should work)
 *    INPUT on pin 2 (must be an interrupt pin) from the signal ouput on the RX (ch3) 
 *    7.6 volts from battery to RAW pin or 5V from RX to Vcc pin.  GND pin to 0v. RX Ov to GND
 *    2 N channel MOSFETS (IRF540N)
 *      gate via 100R to PWM Arduino pin with 10K pull down resistor to GND.
 *      Source to GND
 *      Drain to device GND, csthode or 0v
 *      Device +ve, anode or Vcc to RAW or VCC (voltage/current to suit devices) 
 *    Back EMF diode across any motors
 *    ensure motors can run on PWM (or set PWM to 255)
 *    LEDs with limit resistors for lighting
 *    The first 6 lines in the code below may need to be adjusted to suit hardware.
 *    
 * Operation:
 *    Channel 3 TX Switch
 *      WATER (pin 6) runs whilst switch is LOW or LEFT
 *      LIGHTS (pin 5) toggle on/off each time the switch is moved from middle to HIGH or RIGHT
 *      Leaving the switch high for more than 3 seconds when lights are ON will  cause 
 *          lights to slowly fade up from 0 to full over about 10 seconds. 
 *          This will repeat whilst the switch is up.
 *      SWITCH Centre will switch off WATER and toggle lights either on or off or fix the new level.
 *          The new level will remain each time the lights are switched on unless re-adjusted. 
 *          The new level will be revert to full each time the device powers up.
 *          
 */


const int INPIN = 2; // must be interrupt capable
const int WATER = 6 ;// Water Pin. PWM capable Water is RX low. while switch is down
const int LIGHTS = 5;// lights pin. PWM capable. toggle each time TX switch is up (TX - HIGH) for more than 0.5s
const int FIXED_PWM = 80;// 0 (off) to 255 (full) for water pump
const int HIGH_THRESHOLD = 1600; // This is the signal from the RX to the servo. 
//           It may need adjusting to suit RX. 1500 is usually a servo mid-point
const int LOW_THRESHOLD = 1300; // may need adjusting to suit RX

bool lightsStatus = 0;// tracks whether lights are on 
bool switchOn = 0;// tracks when the s=witch is in the upper position
unsigned long switchOnTime; //marks the current time when the switch is first moved up 
bool waterStatus = 1; //tracks when the water pump is running
byte lightsOutput = 0; // the output value to be sent to the lights.
byte lightsLevel = 255;// sets the initial lights level
volatile bool pulseStarted = LOW;// flag to denote that the servo pulse from RX has risen

void setup() {
  Serial.begin(250000);  // start serial for output
  Serial.println("Water Pump and Lighting from RC Auxilliary RX3 by Steve Lomax for Personal use only");
  pinMode(INPIN, INPUT_PULLUP);
  pinMode(WATER, OUTPUT);
  pinMode(LIGHTS, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(INPIN), initiate, CHANGE);// sets pin 2 to trigger initiate routine when changed
}


void initiate() {// runs each time the pulse raises on pin 2
  pulseStarted = true;
}

int interrogate() {// determines the RX pulse duration and sends it back to the calling instruction.
  unsigned long fallTime;// micros() count
  unsigned  long riseTime; //micros() count
  int duration = 0;//initialise RX pulse duration
  if (pulseStarted) {
    riseTime = micros();//set start of RX pulse
    while (digitalRead(INPIN) == 1) {}// wait for RX pulse end. Not very efficient in code but accurate
    fallTime = micros();
    duration = fallTime - riseTime;
    pulseStarted = false;// end of RX pulse so wait for re-set for next pulse
  }
  return duration; // send the RX pulse duration back to the main loop returns 0 if there is no new pulse
}

void output(int dur) {// determine outputs from pulse dur(ation)
  // WATER
  if (dur < LOW_THRESHOLD) {
    //run water whilst switch is in down position
    if (!waterStatus) { //the command is sent only once for each switch position change
      analogWrite(WATER, FIXED_PWM);
      Serial.println (" Water ON ");
      waterStatus = 1;
    }
  } else {
    if (waterStatus) {
      analogWrite(WATER, 0);
      Serial.println (" Water OFF ");
      waterStatus = 0;
    }
  }
  // LIGHTS
  if (dur > HIGH_THRESHOLD) {
    //toggle lights on / off once per swithch on position
    if (!switchOn) {// if switch on action not yet processed
      switchOn = 1;
      lightsStatus = !lightsStatus;
      if (!lightsStatus) {
        lightsOutput = 0;
        Serial.println (" lights off ");
      } else {
        lightsOutput = lightsLevel;
        Serial.print (" lights on at level: ");
        Serial.println (lightsOutput);
        switchOnTime = millis();//start swich timer
      }
    }
    if (lightsStatus) {
      if (millis()  > switchOnTime + 3000) { //switch has been up for more than 3 seconds
        if ((millis() - switchOnTime - 3000) % 2 == 0) {//increase lights level by 2 every other iteration of the loop
          lightsLevel+=2 ;
          if (lightsLevel > 253) { 
            switchOnTime = millis() ;// re-set the timer to maintain full for 3 seconds
          }
          Serial.print (" new level = ");
          Serial.println (lightsLevel);
          lightsOutput = lightsLevel;
        }
      }
    }
    analogWrite(LIGHTS, lightsOutput);// send the value of lightsOutput to the LIGHTS pin
  } else {
    //    Serial.print (" duration is switch off ");
    //    Serial.println (dur);
    switchOn = 0;
    switchOnTime = 0 - 1;// sets maximum value to cancel the timer. 
  }
}

void loop() {
  int duration = interrogate();// fetch pulse duration (0 = no pulse)
  if (duration > 0) {
    output(duration); //send duration to output function 
  }
}
