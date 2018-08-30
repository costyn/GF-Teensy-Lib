#ifndef LEDRoutines_H
#define LEDRoutines_H

#include <Arduino.h>
#include <FastLED.h>
#include <ArduinoTapTempo.h>
#include <TaskScheduler.h>


class LEDRoutines
{
  public:
    LEDRoutines(CRGB* leds, uint8_t numLeds, ArduinoTapTempo* tapTempo, Task* taskLedModeSelect) ;
    void FillLEDsFromPaletteColors(uint8_t paletteIndex ) ;
    void fadeGlitter() ;
    void discoGlitter() ;
    void strobe1() ;
    void strobe2() ;
    void Fire2012() ;
    void racingLeds() ;
    void waveYourArms() ;
    void shakeIt() ;
    void whiteStripe() ;
    void gLedOrig() ;
    void gLed() ;
    void twirlers(uint8_t numTwirlers, bool opposing ) ;
    void heartbeat() ;
    void fastLoop(bool reverse) ;
    void fillnoise8(uint8_t currentPalette, uint8_t speed, uint8_t scale, boolean colorLoop ) ;
    void pendulum() ;
    void bounceBlend() ;
    void jugglePal() ;
    void quadStrobe() ;
    void pulse3() ;
    void pulse5( uint8_t numPulses, boolean leadingDot) ;
    void threeSinPal() ;
    void colorGlow() ;
    void fanWipe() ;

    // Helpers:
    void fillGradientRing( int startLed, CHSV startColor, int endLed, CHSV endColor ) ;
    void fillSolidRing( int startLed, int endLed, CHSV color ) ;
    int lowestPoint() ;
    int mod(int x, int m) ;
    void fadeall(uint8_t fade_all_speed) ;
    void brightall(uint8_t bright_all_speed) ;
    void addGlitter( fract8 chanceOfGlitter) ;
    void checkButtonPress() ;
    void cycleBrightness() ;
    void serialEvent() ;
    void setMaxBright( uint8_t maxBright );

    CRGB* _leds ;
};

#endif
