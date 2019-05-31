/* MIDI MJoy
   Joystick to MIDI converter

   You must select MIDI or MIDI+SERIAL from the "Tools > USB Type" menu for this code to compile.

   To change the name of the USB-MIDI device, edit the STR_PRODUCT define
   in the /Applications/Arduino.app/Contents/Java/hardware/teensy/avr/cores/usb_midi/usb_private.h
   file. You may need to clear your computers cache of MIDI devices for the name change to be applied.
*/

#include <Bounce.h>

//#define DEBUG_SERIAL

//The number of push buttons
const int NUM_OF_BUTTONS = 4;

//The number of joystick axes
const int NUM_OF_AXES = 4;

// the MIDI channel number to send messages
const int MIDI_CHAN = 1;

// Create Bounce objects for each button and switch. The Bounce object
// automatically deals with contact chatter or "bounce", and
// it makes detecting changes very simple.
// 5 = 5 ms debounce time which is appropriate for good quality mechanical push buttons.
// If a button is too "sensitive" to rapid touch, you can increase this time.

//button debounce time
const int DEBOUNCE_TIME = 5;

Bounce buttons[NUM_OF_BUTTONS] =
{
  Bounce (0, DEBOUNCE_TIME),
  Bounce (1, DEBOUNCE_TIME),
  Bounce (2, DEBOUNCE_TIME),
  Bounce (3, DEBOUNCE_TIME)
};

//Variable that stores the current builtin led state
int ledLit = 0;
elapsedMillis sinceLedLit;

// Analog values of the joystick axes
const int AXIS_INF_THRESHOLD = 2000;
const int AXIS_CAL_THRESHOLD = 400;
const int AXIS_GRAVITY_DELTA = 48;

struct axis {
  int pin;
  int rawMin;
  int rawMax;
  int ccNum;
  int ccVal;
  bool calibrated;
  elapsedMillis sincePlugged;
};

axis axes[NUM_OF_AXES]; 

// Variable that stores the currend DEVICE mode 
int deviceMode = 1;

// Arrays the store the exact note and CC messages each push button will send.
const int MIDI_NOTE_BASE = 40;
const int MIDI_NOTE_VEL = 127;
const int MIDI_CC_BASE = 1;
const int MIDI_NOTE_NUMS[NUM_OF_BUTTONS] = {40, 41, 42, 43};

void blinkLed()
{
  if (ledLit == 0)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    ledLit = 1;
    sinceLedLit = 0; 
  }
}

void resetAxis(int idx)
{
  if ((idx >= 0) && (idx < NUM_OF_AXES))
  {
    axes[idx].pin = A0 + idx;
    axes[idx].rawMin = 4095;
    axes[idx].rawMax = 0;
    axes[idx].ccNum = 0;
    axes[idx].ccVal = 0;
    axes[idx].calibrated = false;
  }
}
//==============================================================================
//==============================================================================
//==============================================================================
//The setup function. Called once when the Teensy is turned on or restarted

void setup()
{
#ifdef DEBUG_SERIAL
  Serial.begin(9600); // USB is always 12 Mbit/sec
#endif
  
  pinMode(LED_BUILTIN, OUTPUT);

  for (int i = 0; i < 8; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  // set analog resolution and averaging
  analogReadResolution(12);
  analogReadAveraging(10);

  for (int i = 0; i < NUM_OF_AXES; i++) {
    resetAxis(i);
  }  
}

//==============================================================================
//==============================================================================
//==============================================================================
//The loop function. Called over-and-over once the setup function has been called.

void loop()
{
  int ccv, ccn, scaledValue;
  
  if (ledLit)
  {
    if (sinceLedLit > 50) {
      digitalWrite(LED_BUILTIN, LOW);
      ledLit = 0;
    }
  }  

  //==============================================================================
  // Set device mode according to the hw switch position

  for (int k = 4; k < 8; k++) {
    if (digitalRead(k) == 0) {
      int swPosition = 8-k;
      if (deviceMode != swPosition) {
        deviceMode = swPosition;
#ifdef DEBUG_SERIAL              
        Serial.print("deviceMode=");
        Serial.println(deviceMode, DEC);       
#endif        
      }      
      break;
    }
  }
  
  //==============================================================================
  // Update all the buttons/switch. There should not be any long
  // delays in loop(), so this runs repetitively at a rate
  // faster than the buttons could be pressed and released.
  for (int i = 0; i < NUM_OF_BUTTONS; i++)
  {
    buttons[i].update();
  }

  //==============================================================================
  // Check the status of each push button

  for (int i = 0; i < NUM_OF_BUTTONS; i++)
  {

    //========================================
    // Check each button for "falling" edge.
    // Falling = high (not pressed - voltage from pullup resistor) to low (pressed - button connects pin to ground)

    if (buttons[i].fallingEdge())
    {
      usbMIDI.sendNoteOn(MIDI_NOTE_BASE + i, MIDI_NOTE_VEL, MIDI_CHAN);    
      //
      blinkLed();
            
#ifdef DEBUG_SERIAL      
      Serial.print("button ");
      Serial.print(i, DEC);
      Serial.println(" press");
#endif      
    }

    //========================================
    // Check each button for "rising" edge
    // Rising = low (pressed - button connects pin to ground) to high (not pressed - voltage from pullup resistor)

    else if (buttons[i].risingEdge())
    {
      usbMIDI.sendNoteOff(MIDI_NOTE_BASE + i, 0, MIDI_CHAN);
      //
      blinkLed();

#ifdef DEBUG_SERIAL
      Serial.print("button ");
      Serial.print(i, DEC);
      Serial.println(" release");
#endif      
    }
  } //for (int i = 0; i < NUM_OF_BUTTONS; i++)

  //==============================================================================
  // Check analog axex

  for (int i = 0; i < NUM_OF_AXES; i++)
  {
    int rawValue = analogRead(axes[i].pin);
    
    if (rawValue > AXIS_INF_THRESHOLD) {      

      if (axes[i].sincePlugged > 500) { // avoid calibration for 0.5 second after plugging
        if (rawValue < axes[i].rawMin) axes[i].rawMin = rawValue;
        if (rawValue > axes[i].rawMax) axes[i].rawMax = rawValue;
      }

      axes[i].calibrated = ((axes[i].rawMax - axes[i].rawMin) > AXIS_CAL_THRESHOLD);

      if (axes[i].calibrated) {

        scaledValue = map(rawValue, axes[i].rawMin, axes[i].rawMax, 0, 1023);

        if (deviceMode == 1) {
          ccn = i+1;
          ccv = map(scaledValue, 0, 1023, 0, 127);
          if (i%2 == 0) ccv = 127 - ccv;
        }   

        else

        if (deviceMode == 2) {
          ccn = i+1;
          if ((scaledValue > 512-AXIS_GRAVITY_DELTA) && (scaledValue < 512+AXIS_GRAVITY_DELTA)) {
            ccv = 64;
          }
          else {
            ccv = (scaledValue < 512) ? map(scaledValue, 0, 511-AXIS_GRAVITY_DELTA, 0, 63) : map(scaledValue, 512+AXIS_GRAVITY_DELTA, 1023, 65, 127);
            if (i%2 == 0) ccv = 127 - ccv;
          }  
        }   

        else
        
        if (deviceMode == 3) {             
          if ( abs(scaledValue-512) <= AXIS_GRAVITY_DELTA ) scaledValue = 512; // center gravity

          if (scaledValue < 512) {
            ccn = 5 + (i*2);
            ccv = max(0, 127 - map(scaledValue, 0, 511-AXIS_GRAVITY_DELTA, 0, 127));
          }
          else {
            ccn = 5 + (i*2) + 1;
            ccv = max(0, map(scaledValue, 512+AXIS_GRAVITY_DELTA, 1023, 0, 127));            
          }

          if (axes[i].ccNum != ccn) {
            if (axes[i].ccNum > 0) {
              usbMIDI.sendControlChange(axes[i].ccNum, 0, MIDI_CHAN);
#ifdef DEBUG_SERIAL          
              Serial.print("CC="); Serial.print(axes[i].ccNum);
              Serial.print(" value="); Serial.println(0);
#endif                          
            }
            axes[i].ccVal = -1;
          }
        } // if (deviceMode == 3)

        else {
          // it's better to provide a default mode for unexpected values of deviceMode
          ccn = i+1;
          ccv = map(scaledValue, 0, 1023, 0, 127);         
        }

//      ccv = min(ccv, 127);

        if (axes[i].ccVal != ccv) {
          usbMIDI.sendControlChange(ccn, ccv, MIDI_CHAN);
          blinkLed();
#ifdef DEBUG_SERIAL          
//        Serial.print("RAW INTERVAL=");
//        Serial.print(axes[i].rawMax - axes[i].rawMin);
          Serial.print("RAW VALUE=");
          Serial.print(rawValue);
          Serial.print(" RAW MIN=");
          Serial.print(axes[i].rawMin);
          Serial.print(" RAW MAX=");
          Serial.print(axes[i].rawMax);
          Serial.print(" SCALED VALUE=");
          Serial.print(scaledValue);
          Serial.print(" CC="); Serial.print(ccn);
          Serial.print(" value="); Serial.println(ccv);
#endif
        }

        axes[i].ccNum = ccn;                      
        axes[i].ccVal = ccv;
                
      } // if (axes[i].calibrated)
      
    } // if (rawValue > AXIS_INF_THRESHOLD)
       
    else {           
      if (axes[i].calibrated) {
        resetAxis(i);
        //
#ifdef DEBUG_SERIAL                
        Serial.print("Axis="); Serial.print(i);
        Serial.println(" reset");
#endif
      }
      axes[i].sincePlugged = 0;
    }
    
  } // for (int i = 0; i < NUM_OF_AXES; i++)

  //==============================================================================
  // MIDI Controllers should discard incoming MIDI messages.
  // http://forum.pjrc.com/threads/24179-Teensy-3-Ableton-Analog-CC-causes-midi-crash
  while (usbMIDI.read())
  {
    // ignoring incoming messages, so don't do anything here.
  }

  // loop delay
#ifdef DEBUG_SERIAL  
  delay(10);
#else
  delay(5);
#endif
}
