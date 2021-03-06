#include <FastLED.h>
#include <TaskScheduler.h>
#include <ArduinoTapTempo.h>
#include <LEDRoutines.h>
#include <MPUFunctions.h>


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

#include <FastLED.h>
#include <TaskScheduler.h>
#include <ArduinoTapTempo.h>
#include <easing.h>

#ifdef USING_MPU
#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>
#endif

// Uncomment for debug output to Serial. Comment to make small(er) code :)
#define DEBUG

// Uncommment to get MPU debbuging:
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

#ifdef _TASK_MICRO_RES
#define TASK_RES_MULTIPLIER 1000
#endif

#define BRIGHTFACTOR 0.2

//black green white red

#ifdef NEO_PIXEL
#define CHIPSET     WS2812B
#define COLOR_ORDER GRB  // Try mixing up the letters (RGB, GBR, BRG, etc) for a whole new world of color combinations
#endif


#if defined(APA_102) || defined(APA_102_SLOW)
#include <SPI.h>
#define CHIPSET     APA102
#define COLOR_ORDER BGR
#endif

// Sanity checks:
#ifndef NUM_LEDS
#error "Error: NUM_LEDS not defined"
#endif

#ifndef DEFAULT_BRIGHTNESS
#error "Error: DEFAULT_BRIGHTNESS not defined"
#endif

#ifndef DEFAULT_BPM
#error "Error: DEFAULT_BPM not defined"
#endif

//#define NUM_LEDS 139
CRGB leds[NUM_LEDS];
uint8_t currentBrightness = DEFAULT_BRIGHTNESS ;

// BPM and button stuff
boolean longPressActive = false;
ArduinoTapTempo tapTempo;

// Serial input to change patterns, speed, etc
String inputString = "";         // a String to hold incoming data
boolean stringComplete = false;  // whether the string is complete

// Which mode do we start with
#ifdef DEFAULT_LED_MODE
byte ledMode = DEFAULT_LED_MODE;
#else
byte ledMode = 0;
#endif


MPUFunctions mpuf = MPUFunctions() ;
uint8_t numLeds = NUM_LEDS ;
LEDRoutines ldr;


/* Scheduler stuff */

#define LEDMODE_SELECT_DEFAULT_INTERVAL  50000   // default scheduling time for LEDMODESELECT, in microseconds
void ledModeSelect() ;                          // prototype method
Scheduler runner;
Task taskLedModeSelect( LEDMODE_SELECT_DEFAULT_INTERVAL, TASK_FOREVER, &ledModeSelect);

#ifdef BUTTON_PIN
#define TASK_CHECK_BUTTON_PRESS_INTERVAL  10*TASK_RES_MULTIPLIER   // in milliseconds
void checkButtonPress() ;                       // prototype method
Task taskCheckButtonPress( TASK_CHECK_BUTTON_PRESS_INTERVAL, TASK_FOREVER, &checkButtonPress);
#endif

#ifdef AUTOADVANCE
void autoAdvanceLedMode() ;                       // prototype method
Task taskAutoAdvanceLedMode( 30 * TASK_SECOND, TASK_FOREVER, &autoAdvanceLedMode);
#endif

// ==================================================================== //
// ===               MPU6050 variable declarations                ===== //
// ==================================================================== //


// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif

#ifdef USING_MPU
MPU6050 mpu;

#define INTERRUPT_PIN 15  // use pin 2 on Arduino Uno & most boards

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

int aaRealX = 0 ;
int aaRealY = 0 ;
int aaRealZ = 0 ;
int yprX = 0 ;
int yprY = 0 ;
int yprZ = 0 ;

void getDMPData() ; // prototype method
Task taskGetDMPData( 3 * TASK_RES_MULTIPLIER, TASK_FOREVER, &getDMPData);
#endif


// #ifdef DEBUG_WITH_TASK
// void printDebugging() ; // prototype method
// Task taskPrintDebugging( 100000, TASK_FOREVER, &printDebugging);
// #endif


// ==================================================================== //
// ============================ Begin Setup =========================== //
// ==================================================================== //


void setup() {
  delay( 1000 ); // power-up safety delay

  #ifdef NEO_PIXEL
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  #endif

  #ifdef APA_102
  FastLED.addLeds<CHIPSET, MY_DATA_PIN, MY_CLOCK_PIN, COLOR_ORDER, DATA_RATE_MHZ(12)>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  #endif

  #ifdef APA_102_SLOW
  // Some APA102's require a very low data rate or they start flickering. Shitty quality LEDs? Wiring? Level shifter?? TODO: figure it out!
  FastLED.addLeds<CHIPSET, MY_DATA_PIN, MY_CLOCK_PIN, COLOR_ORDER, DATA_RATE_MHZ(2)>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  #endif

  ldr.setLeds( leds, numLeds, &tapTempo, &taskLedModeSelect, &currentBrightness );

  FastLED.setBrightness( currentBrightness );

  #if defined(BUTTON_PIN)
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  #endif

  // On these boards one of the button pins is connected to these, so we pull it low so when the button is pressed, the input pin goes low too.
  #if defined(BUTTON_GND_PIN)
  pinMode(BUTTON_GND_PIN, OUTPUT);
  digitalWrite(BUTTON_GND_PIN, LOW);
  #endif

  #if defined(BUTTON_LED_PIN)
  pinMode(BUTTON_LED_PIN, OUTPUT);
  digitalWrite(BUTTON_LED_PIN, HIGH);
  #endif

  #if defined(BPM_BUTTON_PIN)
  pinMode(BPM_BUTTON_PIN, INPUT_PULLUP);
  #endif

  // Not sure this actually works
  //FastLED.setMaxPowerInVoltsAndMilliamps(5,475);

  #ifdef ESP8266
    yield(); // not sure if needed to placate ESP watchdog
  #endif

  /* Start the scheduler */
  runner.init();
  runner.addTask(taskLedModeSelect);
  taskLedModeSelect.enable() ;

#ifdef BUTTON_PIN
  runner.addTask(taskCheckButtonPress);
  taskCheckButtonPress.enable() ;
#endif

#ifdef AUTOADVANCE
  runner.addTask(taskAutoAdvanceLedMode);
  taskAutoAdvanceLedMode.enable() ;
#endif


// #ifdef ESP8266
// WiFi.forceSleepBegin();
// #endif

  // ==================================================================== //
  // ============================ MPU Stuff ============================= //
  // ==================================================================== //

#ifdef USING_MPU
  // join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  Wire.begin();
  Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
  Fastwire::setup(400, true);
#endif

  DEBUG_PRINTLN( F("Wire library begin done.")) ;

  mpu.initialize();
  DEBUG_PRINTLN( F("MPU initialize done.")) ;
  pinMode(INTERRUPT_PIN, INPUT);
  devStatus = mpu.dmpInitialize();
  DEBUG_PRINTLN( F("DMP initialize done.")) ;

  mpu.setXAccelOffset(X_ACCEL_OFFSET);
  mpu.setYAccelOffset(Y_ACCEL_OFFSET);
  mpu.setZAccelOffset(Z_ACCEL_OFFSET);
  mpu.setXGyroOffset(X_GYRO_OFFSET);
  mpu.setYGyroOffset(Y_GYRO_OFFSET);
  mpu.setZGyroOffset(Z_GYRO_OFFSET);

  DEBUG_PRINTLN( F("MPU calibrate done")) ;

  if (devStatus == 0) {
    mpu.setDMPEnabled(true);
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
    mpuIntStatus = mpu.getIntStatus();
    dmpReady = true;
    packetSize = mpu.dmpGetFIFOPacketSize();

  } else {
    // ERROR!
    // 1 = initial memory load failed
    // 2 = DMP configuration updates failed
    // (if it's going to break, usually the code will be 1)
    DEBUG_PRINT(F("DMP Initialization failed (code "));
    DEBUG_PRINT(devStatus);
    DEBUG_PRINTLN(F(")"));
  }

  DEBUG_PRINTLN( F("DMP enable done")) ;

  runner.addTask(taskGetDMPData);
  taskGetDMPData.enable() ;
#endif


#ifdef DEBUG_WITH_TASK
  runner.addTask(taskPrintDebugging);
  taskPrintDebugging.enable() ;
#endif

  tapTempo.setBPM(DEFAULT_BPM);
  inputString.reserve(200);
}  // end setup()


// ==================================================================== //
// ============================ Main Loop() =========================== //
// ==================================================================== //


void loop() {
  #ifdef ESP8266
    yield() ; // Pat the ESP watchdog
  #endif
  runner.execute();
}



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
 #ifdef RT_FIRE2012
   "fire2012",
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
 #ifdef RT_DROPLETS
   "droplets",
 #endif
 #ifdef RT_DROPLETS2
   "droplets2",
 #endif
 #ifdef RT_BOUNCYBALLS
   "bouncyballs",
 #endif
 #ifdef RT_CIRC_LOADER
   "circloader",
 #endif
 #ifdef RT_RIPPLE
   "ripple",
 #endif
 #ifdef RT_RANDOMWALK
   "randomwalk",
 #endif
 #ifdef RT_FASTLOOP3
   "fastloop3",
 #endif
 #ifdef RT_POVPATTERNS
   "povpatterns",
 #endif
 #ifdef RT_BLACK
   "black",
 #endif

 };

 #define NUMROUTINES (sizeof(routines)/sizeof(char *)) //array size


 void ledModeSelect() {
   #ifdef ESP8266
     yield();
   #endif

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
     taskLedModeSelect.setInterval( TASK_IMMEDIATE ) ;
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

 #ifdef RT_FIRE2012
   } else if ( strcmp(routines[ledMode], "fire2012") == 0 ) {
     ldr.Fire2012() ;
     taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
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
     if( tapTempo.getBPM() > 50 ) {
       ldr.fillnoise8( 0, beatsin8( tapTempo.getBPM(), 1, 25), 30, 1); // pallette, speed, scale, loop
     } else {
       ldr.fillnoise8( 0, 1, 30, 1); // pallette, speed, scale, loop
     }
     taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
 #endif

 #ifdef RT_NOISE_PARTY
   } else if ( strcmp(routines[ledMode], "noise_party") == 0 ) {
     if( tapTempo.getBPM() > 50 ) {
       ldr.fillnoise8( 1, beatsin8( tapTempo.getBPM(), 1, 25), 30, 1); // pallette, speed, scale, loop
     } else {
       ldr.fillnoise8( 1, 1, 30, 1); // pallette, speed, scale, loop
     }
     taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
 #endif

 #ifdef RT_NOISE_OCEAN
   } else if ( strcmp(routines[ledMode], "noise_ocean") == 0 ) {
     ldr.fillnoise8( 2, beatsin8( tapTempo.getBPM(), 1, 25), 30, 1); // pallette, speed, scale, loop
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

 #ifdef RT_DROPLETS
 } else if ( strcmp(routines[ledMode], "droplets") == 0 ) {
     ldr.droplets() ;
     taskLedModeSelect.setInterval( 30 * TASK_RES_MULTIPLIER ) ;
 #endif

 #ifdef RT_BOUNCYBALLS
 } else if ( strcmp(routines[ledMode], "bouncyballs") == 0 ) {
     ldr.bouncyBalls() ;
     taskLedModeSelect.setInterval( 30 * TASK_RES_MULTIPLIER ) ;
 #endif

 #ifdef RT_DROPLETS2
 } else if ( strcmp(routines[ledMode], "droplets2") == 0 ) {
     ldr.droplets2() ;
     taskLedModeSelect.setInterval( 10 * TASK_RES_MULTIPLIER ) ;
 #endif

 #ifdef RT_CIRC_LOADER
 } else if ( strcmp(routines[ledMode], "circloader") == 0 ) {
     ldr.circularLoader() ;
     taskLedModeSelect.setInterval( 50 * TASK_RES_MULTIPLIER ) ;
 #endif

 #ifdef RT_RIPPLE
 } else if ( strcmp(routines[ledMode], "ripple") == 0 ) {
     ldr.ripple() ;
     taskLedModeSelect.setInterval( 100 * TASK_RES_MULTIPLIER ) ;
 #endif

 #ifdef RT_RANDOMWALK
 } else if ( strcmp(routines[ledMode], "randomwalk") == 0 ) {
     ldr.randomWalk() ;
     taskLedModeSelect.setInterval( 5 * TASK_RES_MULTIPLIER ) ;
 #endif

 #ifdef RT_FASTLOOP3
 } else if ( strcmp(routines[ledMode], "fastloop3") == 0 ) {
     ldr.fastLoop3() ;
     taskLedModeSelect.setInterval( 15 * TASK_RES_MULTIPLIER ) ;
 #endif


 #ifdef RT_POVPATTERNS
 } else if ( strcmp(routines[ledMode], "povpatterns") == 0 ) {
   #define width(array) sizeof(array) / sizeof(array[0])
   DEBUG_PRINTLN( "." ) ;

     ldr.povPatterns(30, Image, width(Image));
     taskLedModeSelect.setInterval( 30000 ) ; // microseconds ; fast needed for POV patterns
 #endif

 #ifdef RT_BLACK
   } else if ( strcmp(routines[ledMode], "black") == 0 ) {
     ldr.fill_solid(leds, NUM_LEDS, CRGB::Black);
     FastLED.show();
     taskLedModeSelect.setInterval( 500 * TASK_RES_MULTIPLIER ) ;  // long because nothing is going on anyways.
 #endif

   }
 }
