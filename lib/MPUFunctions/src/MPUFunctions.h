#ifndef MPUFunctions_H
#define MPUFunctions_H

#include "Arduino.h"
#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>

class MPUFunctions
{
  public:
    MPUFunctions();
    void dmpDataReady();
    void mGetDMPData();
    void getYPRAccel();
    void printDebugging();
    int activityLevel();
    bool isTilted();
    bool isMpuUp();
    bool isMpuDown();
    bool isYawReliable();

    int aaRealX = 0 ;
    int aaRealY = 0 ;
    int aaRealZ = 0 ;
    int yprX = 0 ;
    int yprY = 0 ;
    int yprZ = 0 ;
    bool isVertical = true;
    int16_t maxAccel = 0 ;
};

#endif
