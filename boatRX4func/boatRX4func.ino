/*  ==============================================================
    ==========  Multiple functions from RC channel (3) ===========
    ==========            Steve Lomax 2023    ====================
    ==========        free for personal use only  ================
    ========== Please credit if using or adapting ================
    ==============================================================
   This monitors a 3 position switch on an auxiliary RC channel 3 on a PLANET TS2+2 TX/RX
   Originally written to control cabin/navigation lights, fire water cannons and control 
   a 2 function searchlight on a model boat.
   NOTES: The searchlight and searchservo requires the following mod to the transmitter. 
   This mod is NOT necessary for just the water pump and lights. 
   The code will work without the searchlight or mod.
    1) Fit a momentary 'push to make' button switch in a suitable place in the transmitter. 
    2) solder a 3.3K ohm resistor in series with the switch 
    3) solder flying wires from the resistor and switch between the centre and bottom 
        terminals of the channel 3 switch.

4 devices on one channel (named WATER, LIGHTS. SEARCHLIGHT and searchServo here)
    Water channel has pre-set PWM level (to suit pump motor voltage),
    LIGHTS channel has remote changable variable PWM level
    The searchlight servo on the prototype has a 4:1 gear ratio (180 degree servo gives 2 full rotations of the searchlight)
    The prototype used bronze wipers at the base of the brass rotating 
      rod to power the Searchlight LED. 
      As the light rotation is limited flexible cable should suffice without the need for 
      wipers or a split axle
    Short presses of button toggles servo light, a long press enters servo scan mode.
   Setup:
      Arduino (pro mini used but UNO should work if space permits)
      INPUT on pin 2 (must be an interrupt pin) from the signal ouput on the RX (ch3)
      7.6 volts from battery to RAW pin OR 5V from RX to Vcc pin.  GND pin to  battery 0v and RX Ov 
      3 x  N channel MOSFETS (IRF540N)
      Water and lights must use a PWM output pin, servo and searchlight may use any I/O pin
        All gates via 100R to Arduino pin with 10K gate pull down resistor to GND. 
        Source to GND
        Drain to pump/LED GND, 
        Pump/LED Device +ve, to +ve supply (voltage/current to suit devices)
      Back EMF diode reverse biased across pump motor
      ensure motor can run on PWM (or set PWM to 255 and use buck converter)
      LEDs with limit resistors for lighting
      The first 8 lines in the code below may need to be adjusted to suit hardware or preferences.

   Operation:
      Channel 3 TX Switch
        WATER (pin 6) runs whilst switch is LOW or LEFT
        LIGHTS (pin 5) toggle on/off each time the switch is moved from middle to HIGH or RIGHT
        SEARCHLIGHT LED (pin 3) toggles on short push switch 
        SERVOPIN (pin 4) scans searchlight on button push after a long press. 
        Leaving the ch3 switch high for more than 3 seconds when lights are ON will  cause
            lights to slowly fade up from 0 to full over about 10 seconds.
            This will repeat whilst the switch is up.
        CH3 SWITCH Centre will switch off WATER and toggle lights either on or off or fix the new level.
            The new level will remain each time the lights are switched on unless re-adjusted.
            The new level will be revert to full each time the device powers up.
        Button Press can be made irrespective of switch position

*/

#include <Servo.h>

Servo searchServo;
const int INPIN = 2;   // must be interrupt capable
const int WATER = 6;   // Water Pin. PWM capable Water is RX low. while switch is down
const int LIGHTS = 11;//5;  // lights pin. PWM capable. toggle each time TX switch is up (TX - HIGH) for more than 0.5s
const int SEARCHLIGHT = 12;//3;
const int SSERVOPIN = 3;
const int FIXED_PWM = 90;  // 0 (off) to 255 (full) for water pump
const int SERVO_SPEED = 6; // 1 (fast) to 10 (slow)
const int LONG_PUSH_DURATION = 3000; // long push to initiate dimming or search scan mode (ms)

// typical RX Output servo pulse durations with 3K3 resistor and switch mod
//  1110 water on
//  1336 water on and toggle searchlight
//  1508 water off
//  1788 water off & toggle searchlight
//  1904 toggle lights
//  1950 lights with searchlight both pressed
//
//  logic function
//  < 1200 water on only (<THRESHOLD1)
//  >=1200 < 1400 water on & toggle searchlight (>THRESHOLD1 <THRESHOLD2)
//  >=1400 < 1750 water off (>THRESHOLD2 <THRESHOLD3)
//  >=1750 <1850  toggle searchlight (>THRESHOLD3 <THRESHOLD4)
//  >=1850 < 1930 toggle lights (>THRESHOLD4 <THRESHOLD5)
// >= 1930 Toggle searchlight (>THRESHOLD5)

const int THRESHOLD1 = 1200;
const int THRESHOLD2 = 1400;
const int THRESHOLD3 = 1750;
const int THRESHOLD4 = 1850;
const int THRESHOLD5 = 1930;
// these thresholds may need adjusting for specific hardware.
bool lightsStatus = 0;       // tracks whether lights are on
bool switchOn = 0;           // tracks when the s=witch is in the upper position
unsigned long switchOnTime;  //marks the current time when the switch is first moved up

bool pushOn = 0;             // tracks when the push is pressed
unsigned long pushOnTime;    //marks the current time when the push is pressed
unsigned long releaseTime;    //marks the current time the search mode remains engaged after releasing the button
bool waterStatus = 1;        //tracks when the water pump is running
//bool searchlightStatus = 0;  //tracks when the searchlight is on
//bool searchlightOutput = 0;
byte lightsOutput = 0;             // the output value to be sent to the lights.
byte lightsLevel = 255;            // sets the initial lights level
volatile bool pulseStarted = LOW;  // flag to denote that the servo pulse from RX has risen
int servoPos = 90;
bool servoRev = 0;
bool scanning = 0;


void setup() {
  Serial.begin(250000);  // start serial for output
  Serial.println("Water Pump, search light, searchlight servo and Lighting from RC Auxilliary RX3 by Steve Lomax for Personal use only");
  pinMode(INPIN, INPUT_PULLUP);
  pinMode(WATER, OUTPUT);
  pinMode(SEARCHLIGHT, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(INPIN), initiate, CHANGE);  // sets pin 2 to trigger initiate routine when changed
  searchServo.attach(SSERVOPIN);
}

//int potentiometer(){
//  int pot = analogRead(A3);
// // Serial.print("                          pot = ");
//  //Serial.println(pot);
//  pot = map (pot,0,1024, 1000, 2000);
//  //Serial.print("                          map = ");
//  //Serial.println(pot);
//
//  return pot;
//}
void initiate() {  // runs each time the pulse raises on pin 2
  pulseStarted = true;
}

int interrogate() {        // determines the RX pulse duration and sends it back to the calling instruction.
  unsigned long fallTime;  // micros() count
  unsigned long riseTime;  //micros() count
  int duration = 0;        //initialise RX pulse duration
  if (pulseStarted) {
    riseTime = micros();                //set start of RX pulse
    while (digitalRead(INPIN) == 1) {}  // wait for RX pulse end. Not very efficient in code but accurate
    fallTime = micros();
    duration = fallTime - riseTime;
    pulseStarted = false;  // end of RX pulse so wait for re-set for next pulse
  }
  return duration;  // send the RX pulse duration back to the main loop returns 0 if there is no new pulse
}

void output(int dur) {  // determine outputs from pulse dur(ation)
  // WATER
  //  < 1200 water on (<THRESHOLD2)
  if (!waterStatus) {        //the command is sent only once for each switch position change
    if (dur < THRESHOLD2) {  //less than 1400 - switch down irrespective of push
      //run water whilst switch is in down position
      analogWrite(WATER, FIXED_PWM);
      Serial.print(" Water ON pulse = ");
      Serial.println(dur);
      waterStatus = 1;
      //delay (1000);
    }
    //  >=1200 < 1400 water on & toggle searchlight (>THRESHOLD1 <THRESHOLD2)
    //  >=1400 < 1750 water off (>THRESHOLD2 <THRESHOLD3)
    //  >=1750 <1850  toggle searchlight (>THRESHOLD3 <THRESHOLD4)
    //  >=1850 < 1930 toggle lights (>THRESHOLD4 <THRESHOLD5)
    // >= 1930 Toggle searchlight (>THRESHOLD5)
  } else {
    if ((dur >= THRESHOLD2) & (dur < THRESHOLD3)) {
      analogWrite(WATER, 0);
      Serial.print(" Water OFF pulse = ");
      Serial.println(dur);
      waterStatus = 0;
    }
  }
  // LIGHTS
  if (dur > THRESHOLD4) {
    //toggle lights on / off once per swithch on position
    if (!switchOn) {  // if switch on action not yet processed
      switchOn = 1;
      lightsStatus = !lightsStatus;
      if (!lightsStatus) {
        lightsOutput = 0;
        Serial.print(" lights off pulse =");
        Serial.println(dur);
      } else {
        lightsOutput = lightsLevel;
        Serial.print(" lights on at level: ");
        Serial.print(lightsOutput);
        Serial.print(" pulse =");
        Serial.println(dur);
        switchOnTime = millis();  //start swich timer
      }
    }
    if (lightsStatus) {
      if (millis() > switchOnTime + LONG_PUSH_DURATION) {               //switch has been up for more than 3 seconds
        if ((millis() - switchOnTime - LONG_PUSH_DURATION) % 2 == 0) {  //increase lights level by 2 every other iteration of the loop
          lightsLevel += 1;
          if (lightsLevel > 253) {
            switchOnTime = millis();  // re-set the timer to maintain full for 3 seconds
          }
          Serial.print(" new level = ");
          Serial.print(lightsLevel);
          Serial.print(" pulse = ");
          Serial.println(dur);
          lightsOutput = lightsLevel;
        }
      }
    }
    analogWrite(LIGHTS, lightsOutput);  // send the value of lightsOutput to the LIGHTS pin
  } else {
    //    Serial.print (" duration is switch off ");
    //    Serial.println (dur);
    switchOn = 0;
    switchOnTime = 0 - 1;  // sets maximum value to cancel the timer.
  }

  //SEARCHLIGHT logic
  /*
  BEHAVIOUR:
  if the button is short pressed toggle the searchlight on/off
  if the button is long pressed and the light is on, change function to scan
  in scan, hold button to move, release button and re-hold to change direction
  if the button is long released scanning stops and will revert to light mode.

  if the button is pressed and not in scanning mode: 
    flip the light on/off
    start the button press timer
  if the button is held in for less than 3 seconds: 
    do nothing 
  if not scanning and the button press exceeds 3 seconds and the light is on:
    enter scanning mode 
    enable servo
  if scanning: 
    sweep the servo whilst the button is held in, auto reversing at ends of servo travel
  if the button is released 
   reset the button press timer
    if registered as pressed:  (first pass)
      start the button release timer, 
      reverse the scan direction, 
      register button as not pressed
    if scanning and button has been released more than 3 seconds:
      exit scanning
      disconnect servo



      
      

  */
  if (((dur >= THRESHOLD1) & (dur < THRESHOLD2)) | ((dur >= THRESHOLD3) & (dur < THRESHOLD4)) | (dur > THRESHOLD5)) {

    if (!pushOn) {    // if new push on action has not yet processed
      pushOn = true;  //mark it as processed

      if (!scanning) {
        pushOnTime = millis();  //start button press timer
        digitalWrite(SEARCHLIGHT, !digitalRead(SEARCHLIGHT));

          //debug printout
          Serial.print(" Searchlight is ");
        Serial.print(digitalRead(SEARCHLIGHT));
        Serial.print(".  Pulse = ");
        Serial.println(dur);
      }
    }
    if ((!scanning) & (millis() - pushOnTime > LONG_PUSH_DURATION) & (digitalRead(SEARCHLIGHT)==1)) {  //push has been in for more than 3 seconds
      scanning = true;
      searchServo.attach(SSERVOPIN);
    }
    if (scanning) {
      if ((millis() - pushOnTime - LONG_PUSH_DURATION) % SERVO_SPEED == 0) {  //increment servo every 6 iterations of the loop
        if (servoRev) {
          servoPos--;
        } else {
          servoPos++;
        }
        if (servoPos >= 180) {
          servoRev = 1;
        }
        if (servoPos == 0) {
          servoRev = 0;
        }
        searchServo.write(servoPos); 
      }
    }
  } else {                     //button is released
    pushOnTime = millis();     // reset
    if (pushOn) {              //still registered as pressed
      releaseTime = millis();  //start release timer
      servoRev = !servoRev;    //reverse direction
      pushOn = false;          // mark button push as false
    }
    if ((scanning) & (millis() - releaseTime > LONG_PUSH_DURATION)) {  // scanning and more than 3 seconds of releasetime
      scanning = false;
      searchServo.detach();
    }
  }
}



void loop() {
  int duration = interrogate();  // fetch pulse duration (0 = no pulse)
 // if (duration==0) duration = potentiometer();
  if (duration > 0) {
    output(duration);  //send duration to output function
  }
}
