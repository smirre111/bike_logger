#include "sensorprocessing.h"


extern SemaphoreHandle_t xMutex;
extern uint16_t millis_last;
extern uint16_t millis_cur;

#ifdef ADAF

#else

#endif

#ifdef USE_ADAFRUIT_VL6810_LIB
extern Adafruit_VL6180X vl;
uint8_t range;
#else
extern VL6180X vl;
extern uint16_t range;
#endif

extern Adafruit_MPU6050 mpu;
extern uint8_t status;

#ifndef USE_ADAFRUIT_VL6810_LIB
bool rangeDataReady()
{
  return ((vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO) & 0x04) != 0);
}

uint8_t readRangeNonBlocking()
{
  uint8_t range = vl.readReg(VL6180X::RESULT__RANGE_VAL);
  vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x01);

  return range;
}

uint8_t readRangeNonBlockingMillimeters()
{
  return readRangeNonBlocking() * vl.getScaling();
}

// bool rangeDataReady()
// {
//   uint8_t stt = vl.readReg(VL6180X::RESULT__RANGE_STATUS);
//   uint16_t status = vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO);
//   uint16_t ready = (status & 0x0004);
//   //return ((vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO) & 0x04) != 0);
//   while(ready == 0) {
//     Serial.print("Status :");
//     Serial.println(status);
//     Serial.print("Status :");
//     Serial.println(stt);
//     Serial.print("Range status: ");
//     Serial.print(status & 0x0007);
//     Serial.print(" ALS status: ");
//     Serial.print(status & 0x0038);
//     Serial.print(" Error status: ");
//     Serial.println(status & 0x00C0);

//     stt = vl.readReg(VL6180X::RESULT__RANGE_STATUS);
//     status = vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO);
//     ready = (status & 0x04);
//   }
//   return true;
// }

// uint8_t readRangeNonBlocking()
// {
//   Serial.println("Reading range");
//   uint8_t range = vl.readReg(VL6180X::RESULT__RANGE_VAL);
//   //vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x01);
//   vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x07);

//   return range;
// }

// uint8_t readRangeNonBlockingMillimeters()
// {
//   return readRangeNonBlocking() * vl.getScaling();
// }

#endif


void processSensor(void *pvParameters)
{

  logger_t curSensor;

  while (1)
  {
    if (1)
    {
      Serial.println("Sensor: taking mutex");
    }
    xSemaphoreTake(xMutex, portMAX_DELAY);
    for (int counter = 0; counter < SAMPLES; counter++)
    {
      if (0)
      {
        Serial.print("Loop iteration: ");
        Serial.println(counter);
        Serial.print("sensor() running on core ");
        Serial.println(xPortGetCoreID());
        Serial.println("In processSensor");
      }

#ifdef USE_ADAFRUIT_VL6810_LIB
      range = vl.readRange();
      status = vl.readRangeStatus();
      millis_cur = millis();
      //Serial.print("Sensor interval: ");
      Serial.println(millis_cur - millis_last);
      millis_last = millis_cur;
      if (status == VL6180X_ERROR_NONE)
      {
        Serial.print("Range: ");
        Serial.println(range);
      }

#else
      //range = vl.readRangeContinuousMillimeters();
      Serial.println("Waiting for range data ready");
      if (rangeDataReady())
      {
        Serial.println("Range data ready");
        range = readRangeNonBlockingMillimeters();
      }

      millis_cur = millis();
      //Serial.print("Sensor interval: ");
      Serial.print("Duration: ");
      Serial.println(millis_cur - millis_last);
      millis_last = millis_cur;
      Serial.print("Range: ");
      Serial.println(range);
      if (vl.timeoutOccurred())
      {
        Serial.println(" TIMEOUT");
      }
#endif
      /* Get new sensor events with the readings */
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);
      // a.acceleration.x = 0;
      // a.acceleration.y = 0;
      // a.acceleration.z = 0;
      // /* Print out the values */
      // Serial.print("Acceleration X: ");
      // Serial.print(a.acceleration.x);
      // Serial.print(", Y: ");
      // Serial.print(a.acceleration.y);
      // Serial.print(", Z: ");
      // Serial.print(a.acceleration.z);
      // Serial.println(" m/s^2");

      // Serial.print("Rotation X: ");
      // Serial.print(g.gyro.x);
      // Serial.print(", Y: ");
      // Serial.print(g.gyro.y);
      // Serial.print(", Z: ");
      // Serial.print(g.gyro.z);
      // Serial.println(" rad/s");

      // Serial.print("Temperature: ");
      // Serial.print(temp.temperature);
      // Serial.println(" degC");
#ifdef USE_ADAFRUIT_VL6810_LIB
      curSensor.range = float(range);
#else
      curSensor.range = float(range);
#endif
      curSensor.x = a.acceleration.x;
      curSensor.y = a.acceleration.y;
      curSensor.z = a.acceleration.z;

      logData[counter] = curSensor;
    }

    for (int cnt = 0; cnt < SAMPLES; cnt++)
    {
      logDataDbl[cnt] = logData[cnt];
    }
    Serial.println("Sensor: releasing mutex");
    xSemaphoreGive(xMutex);
    vTaskDelay(10);
  }
}



void setupVl(VL6180X &vl) {

    // Serial.print("Wire statusInit: ");
  // Serial.println(vl.last_status);
  vl.init();
  // Serial.print("Wire statusIn: ");
  // Serial.println(vl.last_status);
  vl.configureDefault();
  // Serial.print("Wire statusdef: ");
  // Serial.println(vl.last_status);

  // Reduce range max convergence time and ALS integration
  // time to 30 ms and 50 ms, respectively, to allow 10 Hz
  // operation (as suggested by Table 6 ("Interleaved mode
  // limits (10 Hz operation)") in the datasheet).
  vl.writeReg(VL6180X::SYSRANGE__MAX_CONVERGENCE_TIME, 30);
  vl.writeReg16Bit(VL6180X::SYSALS__INTEGRATION_PERIOD, 50);
  vl.writeReg(VL6180X::SYSALS__INTERMEASUREMENT_PERIOD, 100);
  vl.writeReg(VL6180X::SYSRANGE__INTERMEASUREMENT_PERIOD, 10);
  vl.setTimeout(500);
  // Serial.print("Wire statusTo: ");
  // Serial.println(vl.last_status);

  // stop continuous mode if already active
  vl.stopContinuous();
  // in case stopContinuous() triggered a single-shot
  // measurement, wait for it to complete
  delay(300);
  vl.writeReg(VL6180X::SYSTEM__MODE_GPIO1, 0x10);
  vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x03);
  // Serial.print("Wire status1: ");
  // Serial.println(vl.last_status);
  // enable interrupt output on GPIO1
  // vl.writeReg(VL6180X::SYSTEM__MODE_GPIO1, 0x30);
  // // clear any existing interrupts

  uint8_t intr = vl.readReg(VL6180X::SYSTEM__INTERRUPT_CONFIG_GPIO);
  intr |= 0x4;
  vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CONFIG_GPIO, intr);
  // Serial.print("Wire status2: ");
  // Serial.println(vl.last_status);

  vl.writeReg16Bit(VL6180X::READOUT__AVERAGING_SAMPLE_PERIOD, 0x10A);
  // Serial.print("Wire status3: ");
  // Serial.println(vl.last_status);

  // vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CONFIG_GPIO, 0x24);
  // vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x07);
  // start interleaved continuous mode with period of 100 ms
  //vl.startInterleavedContinuous(100);
  // vl.writeReg(VL6180X::INTERLEAVED_MODE__ENABLE, 0);

  // vl.writeReg(VL6180X::SYSRANGE__START, 0x01);
  // vl.writeReg(VL6180X::SYSALS__START, 0x01);
  // delay(300);
  // vl.writeReg(VL6180X::SYSRANGE__START, 0x3);
  uint8_t rce = vl.readReg(VL6180X::SYSRANGE__RANGE_CHECK_ENABLES);
  rce &= 0xFE;
  vl.writeReg(VL6180X::SYSRANGE__RANGE_CHECK_ENABLES, rce);
  // Serial.print("Wire status4: ");
  // Serial.println(vl.last_status);

  Serial.println("Waiting for device...");
  vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x07);
  // Serial.print("Wire status5: ");
  // Serial.println(vl.last_status);
  while (!(vl.readReg(VL6180X::RESULT__RANGE_STATUS) & 0x01));
  //vl.startRangeContinuous(100);

  vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x07);
  // Serial.print("Wire status6: ");
  // Serial.println(vl.last_status);
  Serial.println("Starting range measurement");
  vl.writeReg(VL6180X::SYSRANGE__START, 0x3);
}





void setupMpu(Adafruit_MPU6050 &mpu) {

  if (!mpu.begin())
  {
    Serial.println("Failed to find MPU6050 chip");
    while (1)
    {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Accelerometer range set to: ");

  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Gyro range set to: ");

  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.print("Filter bandwidth set to: ");

}