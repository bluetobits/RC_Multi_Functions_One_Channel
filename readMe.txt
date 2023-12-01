BoatRXdim.ino
Multiple devices from one RC channel. This example is for lighting and a pump for a model RC boat


 Multiple functions from auxiliary RC channel (3) Steve Lomax 2023 free for personal use only. 


 This monitors a 3 position switch on an auxiliary RC channel.
 Originally written to control cabin lights and fire water cannons on a model boat from Aux channel 3.


2 devices on one channel (named WATER and LIGHTS here)
Water channel has pre-set PWM level, 
LIGHTS channel has remote changable variable PWM level 
 Setup:
Arduino (pro mini used but UNO should work)
INPUT on pin 2 (must be an interrupt pin) from the signal ouput on the RX (ch3) 
7.6 volts from battery to RAW pin or 5V from RX to Vcc pin.GND pin to 0v. RX Ov to GND
2 N channel MOSFETS (IRF540N)
gate via 100R to PWM Arduino pin with 10K pull down resistor to GND.
Source to GND
Drain to device GND, csthode or 0v
Device +ve, anode or Vcc to RAW or VCC (voltage/current to suit devices) 
Back EMF diode across any motors
ensure motors can run on PWM (or set PWM to 255)
LEDs with limit resistors for lighting
The first 6 lines in the code below may need to be adjusted to suit hardware.


 Operation:
Channel 3 TX Switch
WATER (pin 6) runs whilst switch is LOW or LEFT
LIGHTS (pin 5) toggle on/off each time the switch is moved from middle to HIGH or RIGHT
Leaving the switch high for more than 3 seconds when lights are ON willcause 
lights to slowly fade up from 0 to full over about 10 seconds. 
This will repeat whilst the switch is up.
SWITCH Centre will switch off WATER and toggle lights either on or off or fix the new level.
The new level will remain each time the lights are switched on unless re-adjusted. 
The new level will be revert to full each time the device powers up.