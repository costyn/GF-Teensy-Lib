/*
   Inspiration from: https://learn.adafruit.com/animated-neopixel-gemma-glow-fur-scarf

   By: Costyn van Dongen

   Note: MAX LEDS: 255 (due to use of uint8_t)

   TODO:
       Get rid of MPU interrupt stuff. I don't need it, but removing it without breaking shit is tricky.
       ripple-pal
       - color rain https://www.youtube.com/watch?v=nHBImYTDZ9I  (for strip, not ring)
*/


// Turn on microsecond resolution; needed to sync some routines to BPM
#define _TASK_MICRO_RES
#define _TASK_TIMECRITICAL
//#define USING_MPU
//#define JELLY
//#define FAN
//#define NEWFAN
//#define RING
//#define BALLOON
//#define GLOWSTAFF

#ifdef RING
#define USING_MPU
#endif

#include <FastLED.h>
#include <TaskScheduler.h>
#include <ArduinoTapTempo.h>
#include <LEDRoutines.h>
#include <MPUFunctions.h>

#define DEFAULT_LED_MODE 6

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

// Uncomment for debug output to Serial.
#define DEBUG
//#define DEBUG_WITH_TASK

#ifdef DEBUG
#define DEBUG_PRINT(x)       Serial.print (x)
#define DEBUG_PRINTDEC(x)    Serial.print (x, DEC)
#define DEBUG_PRINTLN(x)     Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x)
#endif

#define TASK_RES_MULTIPLIER 1000

#if defined(GLOWSTAFF)
#define DOTSTAR
#else
#define NEO_PIXEL
#endif


#ifdef NEO_PIXEL
#define CHIPSET     WS2812B
#define LED_PIN     17   // which pin your Neopixels are connected to
//#define NUM_LEDS    16   // how many LEDs you have
#define COLOR_ORDER GRB  // Try mixing up the letters (RGB, GBR, BRG, etc) for a whole new world of color combinations
#endif


#ifdef DOTSTAR
#include <SPI.h>
#define CHIPSET     APA102
#define DATA_PIN  11
#define CLOCK_PIN 13
#define COLOR_ORDER BGR
#define NUM_LEDS    144
#endif

#define DEFAULT_BRIGHTNESS 40  // 0-255, higher number is brighter.
#define DEFAULT_BPM 120
#define BUTTON_PIN  16   // button is connected to pin 3 and GND
#define BUTTON_LED_PIN 3   // pin to which the button LED is attached


#ifdef FAN
#define NUM_LEDS 84
#endif

#ifdef RING
#define NUM_LEDS 64
#endif

#ifdef JELLY
#define NUM_LEDS 64
#define BUTTON_PIN 18
#define DEFAULT_BPM 40
#endif

#ifdef GLOWSTAFF
#define NUM_LEDS 144
#endif

#ifdef NEWFAN
#define LED_PIN 11
#define NUM_LEDS 72
#endif


#define BPM_BUTTON_PIN 19  // button for adjusting BPM

CRGB leds[NUM_LEDS];
uint8_t maxBright = DEFAULT_BRIGHTNESS ;


// BPM and button stuff
boolean longPressActive = false;
ArduinoTapTempo tapTempo;

// Serial input to change patterns, speed, etc
String inputString = "";         // a String to hold incoming data
boolean stringComplete = false;  // whether the string is complete



// A-la-carte routine selection. Uncomment each define below to
// include or not include the routine in the uploaded code.
// Most are kept commented during development for less code to
// and staying within AVR328's flash/ram limits.

#define RT_P_RB_STRIPE
#define RT_P_OCEAN
#define RT_P_HEAT
#define RT_P_LAVA
#define RT_P_PARTY
#define RT_P_FOREST
#define RT_TWIRL1
#define RT_TWIRL2
#define RT_TWIRL4
#define RT_TWIRL6
#define RT_TWIRL2_O
#define RT_TWIRL4_O
#define RT_TWIRL6_O
#define RT_FADE_GLITTER
#define RT_DISCO_GLITTER
//#define RT_RACERS
//#define RT_WAVE
//#define RT_SHAKE_IT
//#define RT_STROBE1
//#define RT_STROBE2
////#define RT_VUMETER  // TODO - broken/unfinished
//#define RT_GLED
//#define RT_HEARTBEAT
#define RT_FASTLOOP
#define RT_FASTLOOP2
#define RT_PENDULUM
#define RT_BOUNCEBLEND
#define RT_JUGGLE_PAL
#define RT_NOISE_LAVA
#define RT_NOISE_PARTY
//#define RT_QUAD_STROBE
//#define RT_PULSE_3
#define RT_PULSE_5_1
#define RT_PULSE_5_2
#define RT_PULSE_5_3
#define RT_THREE_SIN_PAL
//#define RT_BLACK
#define RT_COLOR_GLOW
//#define RT_FAN_WIPE

byte ledMode = DEFAULT_LED_MODE ; // Which mode do we start with

// Routine Palette Rainbow is always included - a safe routine
const char *routines[] = {
  "p_rb",
#ifdef RT_P_RB_STRIPE
  "p_rb_stripe",
#endif
#ifdef RT_P_OCEAN
  "p_ocean",
#endif
#ifdef RT_P_HEAT
  "p_heat",
#endif
#ifdef RT_P_LAVA
  "p_lava",
#endif
#ifdef RT_P_PARTY
  "p_party",
#endif
#ifdef RT_TWIRL1
  "twirl1",
#endif
#ifdef RT_TWIRL2
  "twirl2",
#endif
#ifdef RT_TWIRL4
  "twirl4",
#endif
#ifdef RT_TWIRL6
  "twirl6",
#endif
#ifdef RT_TWIRL2_O
  "twirl2o",
#endif
#ifdef RT_TWIRL4_O
  "twirl4o",
#endif
#ifdef RT_TWIRL6_O
  "twirl6o",
#endif
#ifdef RT_FADE_GLITTER
  "fglitter",
#endif
#ifdef RT_DISCO_GLITTER
  "dglitter",
#endif
#ifdef RT_RACERS
  "racers",
#endif
#ifdef RT_WAVE
  "wave",
#endif
#ifdef RT_SHAKE_IT
  "shakeit",
#endif
#ifdef RT_STROBE1
  "strobe1",
#endif
#ifdef RT_STROBE2
  "strobe2",
#endif
#ifdef RT_GLED
  "gled",
#endif
#ifdef RT_HEARTBEAT
  "heartbeat",
#endif
#ifdef RT_FASTLOOP
  "fastloop",
#endif
#ifdef RT_FASTLOOP2
  "fastloop2",
#endif
#ifdef RT_PENDULUM
  "pendulum",
#endif
#ifdef RT_VUMETER
  "vumeter",
#endif
#ifdef RT_NOISE_LAVA
  "noise_lava",
#endif
#ifdef RT_NOISE_PARTY
  "noise_party",
#endif
#ifdef RT_BOUNCEBLEND
  "bounceblend",
#endif
#ifdef RT_JUGGLE_PAL
  "jugglepal",
#endif
#ifdef RT_QUAD_STROBE
  "quadstrobe",
#endif
#ifdef RT_PULSE_3
  "pulse3",
#endif
#ifdef RT_PULSE_5_1
  "pulse5_1",
#endif
#ifdef RT_PULSE_5_2
  "pulse5_2",
#endif
#ifdef RT_PULSE_5_3
  "pulse5_3",
#endif
#ifdef RT_THREE_SIN_PAL
  "tsp",
#endif
#ifdef RT_COLOR_GLOW
  "color_glow",
#endif
#ifdef RT_FAN_WIPE
  "fan_wipe",
#endif
#ifdef RT_BLACK
  "black",
#endif

};

#define NUMROUTINES (sizeof(routines)/sizeof(char *)) //array size

/* Scheduler stuff */

#define LEDMODE_SELECT_DEFAULT_INTERVAL  50000   // default scheduling time for LEDMODESELECT, in microseconds
void ledModeSelect() ;                          // prototype method
Scheduler runner;
Task taskLedModeSelect( LEDMODE_SELECT_DEFAULT_INTERVAL, TASK_FOREVER, &ledModeSelect);


#define TASK_CHECK_BUTTON_PRESS_INTERVAL  10*TASK_RES_MULTIPLIER   // in milliseconds
void checkButtonPress() ;                       // prototype method
Task taskCheckButtonPress( TASK_CHECK_BUTTON_PRESS_INTERVAL, TASK_FOREVER, &checkButtonPress);


MPUFunctions mpuf = MPUFunctions() ;
uint8_t numLeds = NUM_LEDS ;
LEDRoutines ldr( leds, numLeds, &tapTempo, &taskLedModeSelect );


int aaRealX = 0 ;
int aaRealY = 0 ;
int aaRealZ = 0 ;
int yprX = 0 ;
int yprY = 0 ;
int yprZ = 0 ;

void getDMPData() ; // prototype method
Task taskGetDMPData( 3 * TASK_RES_MULTIPLIER, TASK_FOREVER, &getDMPData);


#ifdef DEBUG_WITH_TASK
void printDebugging() ; // prototype method
Task taskPrintDebugging( 100000, TASK_FOREVER, &printDebugging);
#endif


void getDMPData() {
  mpuf.mGetDMPData() ;
  aaRealX = mpuf.aaRealX;
  aaRealY = mpuf.aaRealY;
  aaRealZ = mpuf.aaRealZ;
  yprX = mpuf.yprX;
  yprY = mpuf.yprY;
  yprZ = mpuf.yprZ;

  static bool inBeat = false; // debounce variable - only update tapTempo once during high acceleration
  static bool firstPress = true ;
  static uint16_t initialYaw ;

  // only count BPM when button is held down
  if ( mpuf.isVertical and longPressActive ) {
    if ( mpuf.maxAccel > 7000 and inBeat == false ) {
      inBeat = true ;
      tapTempo.update(true);
      digitalWrite(BUTTON_LED_PIN, HIGH); // Beat detected, light up to show beat detected

    } else if ( mpuf.maxAccel < 7000 ) {
      inBeat = false ;
      tapTempo.update(false);
      digitalWrite(BUTTON_LED_PIN, LOW); // turn off LED, normal operation
    }
  } else if ( ! mpuf.isVertical and longPressActive ) {
    if ( firstPress ) { // check if this is the first iteration where button is pressed, remember yaw.
      initialYaw = yprX ;
      firstPress = false ;
    }

    if ( taskGetDMPData.getRunCounter() % 30 == 0 ) { // fast blinking to indicate brightness adjustment process
      digitalWrite(BUTTON_LED_PIN, HIGH);
    } else {
      digitalWrite(BUTTON_LED_PIN, LOW);
    }

#define RANGE 30
    int conYprX = constrain( yprX, initialYaw - RANGE, initialYaw + RANGE) ;
    maxBright = map( conYprX, initialYaw - RANGE, initialYaw + RANGE, 0, 255) ;
    //        DEBUG_PRINT(F("\t"));
    //        DEBUG_PRINT(yprX);
    //        DEBUG_PRINT(F("\t"));
    //        DEBUG_PRINT(initialYaw);
    //        DEBUG_PRINT(F("\t"));
    //        DEBUG_PRINT(conYprX);
    //        DEBUG_PRINT(F("\t"));
    //        DEBUG_PRINT(maxBright);
    //        DEBUG_PRINT(F("\t"));
    //        DEBUG_PRINTLN();
    ldr.setMaxBright( maxBright );
  }

  if ( ! longPressActive ) {
    tapTempo.update(false);  // needs to be here even though we are not measuring beats

    if ( tapTempo.beatProgress() > 0.95 ) {
      digitalWrite(BUTTON_LED_PIN, HIGH); // turn on LED, normal operation
    } else {
      digitalWrite(BUTTON_LED_PIN, LOW); // turn on LED, normal operation
    }

    firstPress = true ;
  }
}


void setup() {
  delay( 1000 ); // power-up safety delay
#ifdef NEO_PIXEL
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
#endif
#ifdef DOTSTAR
  FastLED.addLeds<CHIPSET, DATA_PIN, CLOCK_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
#endif
  FastLED.setBrightness(  maxBright );

//  FastLED.setDither( 0 );

  pinMode(BUTTON_PIN, INPUT);
  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);
  pinMode(BUTTON_LED_PIN, OUTPUT);
  digitalWrite(BUTTON_LED_PIN, HIGH);

  pinMode(BPM_BUTTON_PIN, INPUT_PULLUP);

  set_max_power_in_volts_and_milliamps(5, 500);

#ifdef DEBUG
  Serial.begin(115200) ;
  DEBUG_PRINT( F("Starting up. Numroutines = ")) ;
  DEBUG_PRINTLN( NUMROUTINES ) ;

#endif

  /* Start the scheduler */
  runner.init();
  runner.addTask(taskLedModeSelect);
  taskLedModeSelect.enable() ;

  runner.addTask(taskCheckButtonPress);
  taskCheckButtonPress.enable() ;

  runner.addTask(taskGetDMPData);
  taskGetDMPData.enable() ;

#ifdef DEBUG_WITH_TASK
  runner.addTask(taskPrintDebugging);
  taskPrintDebugging.enable() ;
#endif

  tapTempo.setMaxBPM(DEFAULT_BPM);
  inputString.reserve(200);
}  // end setup()




void loop() {
  runner.execute();
}




void ledModeSelect() {

  if ( strcmp(routines[ledMode], "p_rb") == 0  ) {
    ldr.FillLEDsFromPaletteColors(0) ;

#ifdef RT_P_RB_STRIPE
  } else if ( strcmp(routines[ledMode], "p_rb_stripe") == 0 ) {
    ldr.FillLEDsFromPaletteColors(1) ;
#endif

#ifdef RT_P_OCEAN
  } else if ( strcmp(routines[ledMode], "p_ocean") == 0 ) {
    ldr.FillLEDsFromPaletteColors(2) ;
#endif

#ifdef RT_P_HEAT
  } else if ( strcmp(routines[ledMode], "p_heat") == 0 ) {
    ldr.FillLEDsFromPaletteColors(3) ;
#endif

#ifdef RT_P_LAVA
  } else if ( strcmp(routines[ledMode], "p_lava") == 0 ) {
    ldr.FillLEDsFromPaletteColors(4) ;
#endif

#ifdef RT_P_PARTY
  } else if ( strcmp(routines[ledMode], "p_party") == 0 ) {
    ldr.FillLEDsFromPaletteColors(5) ;
#endif

#ifdef RT_P_CLOUD
  } else if ( strcmp(routines[ledMode], "p_cloud") == 0 ) {
    ldr.FillLEDsFromPaletteColors(6) ;
#endif

#ifdef RT_P_FOREST
  } else if ( strcmp(routines[ledMode], "p_forest") == 0 ) {
    ldr.FillLEDsFromPaletteColors(7) ;
#endif

#ifdef RT_TWIRL1
  } else if ( strcmp(routines[ledMode], "twirl1") == 0 ) {
    ldr.twirlers( 1, false ) ;
    taskLedModeSelect.setInterval( TASK_IMMEDIATE ) ;
#endif

#ifdef RT_TWIRL2
  } else if ( strcmp(routines[ledMode], "twirl2") == 0 ) {
    ldr.twirlers( 2, false ) ;
#endif

#ifdef RT_TWIRL4
  } else if ( strcmp(routines[ledMode], "twirl4") == 0 ) {
    ldr.twirlers( 4, false ) ;
#endif

#ifdef RT_TWIRL6
  } else if ( strcmp(routines[ledMode], "twirl6") == 0 ) {
    ldr.twirlers( 6, false ) ;
#endif

#ifdef RT_TWIRL2_O
  } else if ( strcmp(routines[ledMode], "twirl2o") == 0 ) {
    ldr.twirlers( 2, true ) ;
#endif

#ifdef RT_TWIRL4_O
  } else if ( strcmp(routines[ledMode], "twirl4o") == 0 ) {
    ldr.twirlers( 4, true ) ;
#endif

#ifdef RT_TWIRL6_O
  } else if ( strcmp(routines[ledMode], "twirl6o") == 0 ) {
    ldr.twirlers( 6, true ) ;
#endif

#ifdef RT_FADE_GLITTER
  } else if ( strcmp(routines[ledMode], "fglitter") == 0 ) {
    ldr.fadeGlitter() ;
    //taskLedModeSelect.setInterval( map( constrain( activityLevel(), 0, 4000), 0, 4000, 20, 5 ) * TASK_RES_MULTIPLIER ) ;
#ifdef USING_MPU
    taskLedModeSelect.setInterval( map( constrain( activityLevel(), 0, 2500), 0, 2500, 40, 2 ) * TASK_RES_MULTIPLIER ) ;
#else
    taskLedModeSelect.setInterval( 20 * TASK_RES_MULTIPLIER ) ;
#endif
#endif

#ifdef RT_DISCO_GLITTER
  } else if ( strcmp(routines[ledMode], "dglitter") == 0 ) {
    ldr.discoGlitter() ;
#ifdef USING_MPU
    taskLedModeSelect.setInterval( map( constrain( activityLevel(), 0, 2500), 0, 2500, 40, 2 ) * TASK_RES_MULTIPLIER ) ;
#else
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif
#endif

#ifdef RT_GLED
    // Gravity LED
  } else if ( strcmp(routines[ledMode], "gled") == 0 ) {
    ldr.gLed() ;
    taskLedModeSelect.setInterval( 5 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_BLACK
  } else if ( strcmp(routines[ledMode], "black") == 0 ) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    taskLedModeSelect.setInterval( 500 * TASK_RES_MULTIPLIER ) ;  // long because nothing is going on anyways.
#endif

#ifdef RT_RACERS
  } else if ( strcmp(routines[ledMode], "racers") == 0 ) {
    ldr.racingLeds() ;
    taskLedModeSelect.setInterval( 8 * TASK_RES_MULTIPLIER) ;
#endif

#ifdef RT_WAVE
  } else if ( strcmp(routines[ledMode], "wave") == 0 ) {
    ldr.waveYourArms() ;
    taskLedModeSelect.setInterval( 15 * TASK_RES_MULTIPLIER) ;
#endif

#ifdef RT_SHAKE_IT
  } else if ( strcmp(routines[ledMode], "shakeit") == 0 ) {
    ldr.shakeIt() ;
    taskLedModeSelect.setInterval( 8 * 1000 ) ;
#endif

#ifdef RT_STROBE1
  } else if ( strcmp(routines[ledMode], "strobe1") == 0 ) {
    ldr.strobe1() ;
    taskLedModeSelect.setInterval( 5 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_STROBE2
  } else if ( strcmp(routines[ledMode], "strobe2") == 0 ) {
    ldr.strobe2() ;
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_HEARTBEAT
  } else if ( strcmp(routines[ledMode], "heartbeat") == 0 ) {
    ldr.heartbeat() ;
    taskLedModeSelect.setInterval( 5 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_VUMETER
  } else if ( strcmp(routines[ledMode], "vumeter") == 0 ) {
    ldr.vuMeter() ;
    taskLedModeSelect.setInterval( 8 * TASK_RES_MULTIPLIER) ;
#endif

#ifdef RT_FASTLOOP
  } else if ( strcmp(routines[ledMode], "fastloop") == 0 ) {
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER) ;
    ldr.fastLoop( false ) ;
#endif

#ifdef RT_FASTLOOP2
  } else if ( strcmp(routines[ledMode], "fastloop2") == 0 ) {
    ldr.fastLoop( true ) ;
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER) ;
#endif

#ifdef RT_PENDULUM
  } else if ( strcmp(routines[ledMode], "pendulum") == 0 ) {
    ldr.pendulum() ;
    taskLedModeSelect.setInterval( 1500 ) ; // needs a fast refresh rate
#endif

#ifdef RT_BOUNCEBLEND
  } else if ( strcmp(routines[ledMode], "bounceblend") == 0 ) {
    ldr.bounceBlend() ;
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_JUGGLE_PAL
  } else if ( strcmp(routines[ledMode], "jugglepal") == 0 ) {
    ldr.jugglePal() ;
    taskLedModeSelect.setInterval( 150 ) ; // fast refresh rate needed to not skip any LEDs
#endif

#ifdef RT_NOISE_LAVA
  } else if ( strcmp(routines[ledMode], "noise_lava") == 0 ) {
    ldr.fillnoise8( 0, beatsin8( tapTempo.getBPM(), 1, 25), 30, 1); // pallette, speed, scale, loop
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_NOISE_PARTY
  } else if ( strcmp(routines[ledMode], "noise_party") == 0 ) {
    ldr.fillnoise8( 1, beatsin8( tapTempo.getBPM(), 1, 25), 30, 1); // pallette, speed, scale, loop
    //    taskLedModeSelect.setInterval( beatsin16( tapTempo.getBPM(), 2000, 50000) ) ;
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_NOISE_OCEAN
  } else if ( strcmp(routines[ledMode], "noise_ocean") == 0 ) {
    ldr.fillnoise8( 2, beatsin8( tapTempo.getBPM(), 1, 25), 30, 1); // pallette, speed, scale, loop
    //    taskLedModeSelect.setInterval( beatsin16( tapTempo.getBPM(), 2000, 50000) ) ;
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif


#ifdef RT_QUAD_STROBE
  } else if ( strcmp(routines[ledMode], "quadstrobe") == 0 ) {
    ldr.quadStrobe();
    taskLedModeSelect.setInterval( (60000 / (tapTempo.getBPM() * 4)) * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_PULSE_3
  } else if ( strcmp(routines[ledMode], "pulse3") == 0 ) {
    ldr.pulse3();
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_PULSE_5_1
  } else if ( strcmp(routines[ledMode], "pulse5_1") == 0 ) {
    ldr.pulse5(1, true);
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_PULSE_5_2
  } else if ( strcmp(routines[ledMode], "pulse5_2") == 0 ) {
    ldr.pulse5(2, true);
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_PULSE_5_3
  } else if ( strcmp(routines[ledMode], "pulse5_3") == 0 ) {
    ldr.pulse5(3, true);
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_THREE_SIN_PAL
  } else if ( strcmp(routines[ledMode], "tsp") == 0 ) {
    ldr.threeSinPal() ;
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_COLOR_GLOW
  } else if ( strcmp(routines[ledMode], "color_glow") == 0 ) {
    ldr.colorGlow() ;
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif

#ifdef RT_FAN_WIPE
  } else if ( strcmp(routines[ledMode], "fan_wipe") == 0 ) {
    ldr.fanWipe() ;
    taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
#endif
  }

}



/*
SerialEvent occurs whenever a new data comes in the hardware serial RX. This
routine is run between each time loop() runs, so using delay inside loop can
delay response. Multiple bytes of data may be available.
*/
void serialEvent() {
while (Serial.available()) {
  // get the new byte:
  char inChar = (char)Serial.read();
  // add it to the inputString:
  inputString += inChar;
  // if the incoming character is a newline, set a flag so the main loop can
  // do something about it:
  if (inChar == '\n') {
    stringComplete = true;
  }
}
}


#define LONG_PRESS_MIN_TIME 500  // minimum time for a long press
#define SHORT_PRESS_MIN_TIME 70   // minimum time for a short press - debounce

void checkButtonPress() {
  static unsigned long buttonTimer = 0;
  static boolean buttonActive = false;

  if (digitalRead(BUTTON_PIN) == HIGH) {
    // Start the timer
    if (buttonActive == false) {
      buttonActive = true;
      buttonTimer = millis();
    }

    // If timer has passed longPressTime, set longPressActive to true
    if ((millis() - buttonTimer > LONG_PRESS_MIN_TIME) && (longPressActive == false)) {
      longPressActive = true;
    }

#ifndef USING_MPU
    if( longPressActive == true ) {
//          cycleBrightness() ;
    }
#endif


  } else {
    // Reset when button is no longer pressed
    if (buttonActive == true) {
      buttonActive = false;
      if (longPressActive == true) {
        longPressActive = false;
//        taskLedModeSelect.enableIfNot() ;
      } else {
        if ( millis() - buttonTimer > SHORT_PRESS_MIN_TIME ) {
          ledMode++;
          if (ledMode == NUMROUTINES ) {
            ledMode = 0;
          }

          DEBUG_PRINT(F("ledMode = ")) ;
          DEBUG_PRINT( routines[ledMode] ) ;
          DEBUG_PRINT(F(" mode ")) ;
          DEBUG_PRINTLN( ledMode ) ;

//          FastLED.setBrightness( maxBright ) ; // reset it to 'default'
        }
      }
    }
  }

  if (digitalRead(BPM_BUTTON_PIN) == LOW) {
    tapTempo.update(true);
//    DEBUG_PRINTLN( _tapTempo.getBPM() ) ;
  } else {
    tapTempo.update(false);
  }

  serialEvent() ;
  if (stringComplete) {
    if( inputString.charAt(0) == 'p' ) {
      inputString.remove(0,1);
      uint8_t input = inputString.toInt();
      if( input < NUMROUTINES ) {
        ledMode = inputString.toInt();
      }
      DEBUG_PRINT("LedMode: ");
      DEBUG_PRINTLN( ledMode ) ;
    }
    if( inputString.charAt(0) == 'b' ) {
      inputString.remove(0,1);
      tapTempo.setMaxBPM(inputString.toInt());
      tapTempo.setMinBPM(inputString.toInt());
      DEBUG_PRINT("BPM: ");
      DEBUG_PRINTLN( inputString.toInt() ) ;
    }
    if( inputString.charAt(0) == 'm' ) {
      inputString.remove(0,1);
      maxBright = inputString.toInt() ;
      ldr.setMaxBright( maxBright );
      DEBUG_PRINT("MaxBright: ");
      DEBUG_PRINTLN( maxBright ) ;
    }

    // clear the string:
    inputString = "";
    stringComplete = false;
  }

}

// Only use if no MPU present:
#ifndef USING_MPU

void cycleBrightness() {
  static uint8_t currentBright = DEFAULT_BRIGHTNESS ;

#ifdef FAN
  uint8_t deviceMaxBright = 75;
  ldr.setMaxBright( maxBright );
#else
  uint8_t deviceMaxBright = 255;
  ldr.setMaxBright( maxBright );
#endif


  if ( taskCheckButtonPress.getRunCounter() % 100 ) {
    maxBright = lerp8by8( 3, deviceMaxBright, quadwave8( currentBright )) ;
    ldr.setMaxBright( maxBright );
    currentBright++ ;
    DEBUG_PRINTLN( maxBright ) ;
  }

//  taskLedModeSelect.disable() ;
//  fill_solid(leds, NUM_LEDS, CRGB::White );
//  FastLED.show() ;

}
#endif



// for debugging purposes
int freeRam ()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}