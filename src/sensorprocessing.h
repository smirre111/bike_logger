#pragma once 

#include "Arduino.h"
#include "common.h"

#ifdef USE_ADAFRUIT_VL6810_LIB
#include "Adafruit_VL6180X.h"
#else
#include <VL6180X.h>
#endif
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>





struct logger_t
{
  float range;
  float x;
  float y;
  float z;
};


extern logger_t logData[SAMPLES];
extern logger_t logDataDbl[SAMPLES];



bool rangeDataReady();


uint8_t readRangeNonBlockingMillimeters();


void processSensor(void *pvParameters);

void setupVl(VL6180X &vl);

void setupMpu(Adafruit_MPU6050 &mpu);