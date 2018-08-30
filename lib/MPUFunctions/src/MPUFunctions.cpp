
#include "Arduino.h"
#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>


// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif


// ================================================================
// ================================================================
// =====             MPU 6050 CODE STUFF                      =====
// ================================================================
// ================================================================

class MPUFunctions
{
   // Variable declarations

  MPU6050 mpu;
  int aaRealX = 0 ;
  int aaRealY = 0 ;
  int aaRealZ = 0 ;
  int yprX = 0 ;
  int yprY = 0 ;
  int yprZ = 0 ;
  bool isVertical = true ;
  int16_t maxAccel = 0 ;


  #define INTERRUPT_PIN 2  // use pin 2 on Arduino Uno & most boards

  // MPU control/status vars
  bool dmpReady = false;  // set true if DMP init was successful
  uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
  uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
  uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
  uint16_t fifoCount;     // count of all bytes currently in FIFO
  uint8_t fifoBuffer[64]; // FIFO storage buffer

  /*
    void dmpDataReady() ;
    void addGlitter( fract8 chanceOfGlitter) ;
    int mod(int x, int m) ;
    int activityLevel();
    bool isMpuUp();
    bool isMpuDown();
    void fillGradientRing( int startLed, CHSV startColor, int endLed, CHSV endColor ) ;
    void fillSolidRing( int startLed, int endLed, CHSV color ) ;
    void syncToBPM() ;
  */

  volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high


  // Constructor
  MPUFunctions() {
     // join I2C bus (I2Cdev library doesn't do this automatically)
   #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
     Wire.begin();
     Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
   #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
     Fastwire::setup(400, true);
   #endif

     mpu.initialize();
     devStatus = mpu.dmpInitialize();

     // Strip MPU
     // Your offsets:  -1527 -1127 2339  51  4 -7

     mpu.setXAccelOffset(-1527 );
     mpu.setYAccelOffset(-1127);
     mpu.setZAccelOffset(2339);
     mpu.setXGyroOffset(51);
     mpu.setYGyroOffset(4);
     mpu.setZGyroOffset(-7);


     if (devStatus == 0) {
       mpu.setDMPEnabled(true);
//       attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
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
  }

  void mGetDMPData() {

    if ( fifoCount > packetSize ) {
      DEBUG_PRINTLN(F("yo!"));
      mpu.resetFIFO();
    }

    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();
    fifoCount = mpu.getFIFOCount();

    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
      mpu.resetFIFO();
      DEBUG_PRINTLN(F("FIFO overflow!"));

    } else if (mpuIntStatus & 0x02) {
      while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();
      mpu.getFIFOBytes(fifoBuffer, packetSize);
      fifoCount -= packetSize;
    }

    getYPRAccel() ;
  }

  // display Euler angles in degrees
  void getYPRAccel() {
    // orientation/motion vars
    Quaternion quat;        // [w, x, y, z]         quaternion container
    VectorInt16 aa;         // [x, y, z]            accel sensor measurements
    VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
    VectorFloat gravity;    // [x, y, z]            gravity vector
    float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

    mpu.dmpGetQuaternion(&quat, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &quat);
    mpu.dmpGetYawPitchRoll(ypr, &quat, &gravity);
    mpu.dmpGetAccel(&aa, fifoBuffer);
    mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);

    yprX = (ypr[0] * 180 / M_PI) + 180;
    yprY = (ypr[1] * 180 / M_PI) + 90;
    yprZ = (ypr[2] * 180 / M_PI) + 90;

    aaRealX = aaReal.x ;
    aaRealY = aaReal.y ;
    aaRealZ = aaReal.z ;

    isVertical   = ( abs( ypr[1] * 180 / M_PI ) + abs( ypr[2] * 180 / M_PI ) > 65.0 ) ;

    int16_t maxXY    = max( aaReal.x, aaReal.y) ;
    maxAccel = max( maxXY, aaReal.z) ;

  }


  int activityLevel() {
    return round( (abs( aaRealX )  + abs( aaRealY )  + abs( aaRealZ )) / 3 );
  }


  bool isTilted() {
    #define TILTED_AT_DEGREES 10
    return ( 90 - TILTED_AT_DEGREES > max(yprY, yprZ) or yprY > 90 + TILTED_AT_DEGREES ) ;
  }

  // check if MPU is pitched up
  bool isMpuUp() {
    return yprZ > 90 ;
  }

  // check if MPU is pitched down
  bool isMpuDown() {
    return yprZ < 90 ;
  }


  bool isYawReliable() {
  #define MAXANGLE 45
    // Check if yprY or yprZ are tilted more than MAXANGLE. 90 = level
    // yaw is not reliable below MAXANGLE
    return ( MAXANGLE < min(yprY, yprZ) and max(yprY, yprZ) < 90 + MAXANGLE ) ;
  }

  void printDebugging() {
      DEBUG_PRINT( taskLedModeSelect.getRunCounter() );
      DEBUG_PRINT( "\t" );
      DEBUG_PRINT( taskLedModeSelect.getInterval() );
      DEBUG_PRINT( "\t" );
      DEBUG_PRINT( taskLedModeSelect.getStartDelay() );

  //   DEBUG_PRINT(yprX);
  //   DEBUG_PRINT(F("\t"));
  //   DEBUG_PRINT(yprY);
  //   DEBUG_PRINT(F("\t"));
  //   DEBUG_PRINT(yprZ);
  //   DEBUG_PRINT(F("\t"));
  //   DEBUG_PRINT(aaRealX);
  //   DEBUG_PRINT(F("\t"));
  //   DEBUG_PRINT(aaRealY);
  //   DEBUG_PRINT(F("\t"));
  //   DEBUG_PRINT(aaRealZ);
  //   DEBUG_PRINT(F("\t"));
  //   DEBUG_PRINT(activityLevel());
  //   DEBUG_PRINT(F("\t"));
  //   DEBUG_PRINT(isYawReliable());
  //   DEBUG_PRINT(F("\t"));
  //   DEBUG_PRINT( taskGetDMPData.getRunCounter() ) ;
  //   DEBUG_PRINT("\t");
  //   DEBUG_PRINT( freeRam() ) ;
  //
     DEBUG_PRINTLN() ;
    }

    void dmpDataReady() {
      mpuInterrupt = true;
      //  DEBUG_PRINTLN(F(":"));
    }
};
