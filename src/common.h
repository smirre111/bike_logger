#define SAMPLES 128
//#define USE_ADAFRUIT_VL6810_LIB
//#define START_WEBSERVER
#define START_WIFI
#define DISTANCE
#define ACCEL

#ifdef ESP8266
#define SD_CS_pin           D8         // The pin on Wemos D1 Mini for SD Card Chip-Select
#else
#define SD_CS_pin           5        // Use pin 5 on MH-T Live ESP32 version of Wemos D1 Mini for SD Card Chip-Select
#endif         

#define ServerVersion "1.0"