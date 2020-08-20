/*
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       -
 *    D3       SS
 *    CMD      MOSI
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      SCK
 *    VSS      GND
 *    D0       MISO
 *    D1       -
 */

#include "Arduino.h"
#include "common.h"

#ifdef START_WEBSERVER
#include "ws_functions.h"
#endif




#ifdef START_WIFI
#ifdef ESP8266
#include <ESP8266WiFi.h>      // Built-in
#include <ESP8266WiFiMulti.h> // Built-in
#include <ESP8266mDNS.h>
#else
#include <WiFi.h>      // Built-in
#include <WiFiMulti.h> // Built-in
#include <ESPmDNS.h>
#endif
#endif

//#include <WiFiUdp.h>
#include "AsyncUDP.h"

#include "FS.h"
#include <SPIFFS.h>
#include "Network.h"
#include "Sys_Variables.h"
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFSEditor.h>
#include "sd_fs_functions.h"

#ifdef START_WIFI
#ifdef ESP8266
typedef ESP8266WiFiMulti WiFiMulti_t;
#else
typedef WiFiMulti WiFiMulti_t;
#endif
WiFiMulti_t wifiMulti;
#endif



#ifdef START_WEBSERVER
WebServer_t server(80);
#endif

#include "ESPAsyncWebServer.h"
AsyncWebServer server(80);





String webpage = "";
bool SD_present = false;

#include "sensorprocessing.h"

Adafruit_MPU6050 mpu;

#ifdef USE_ADAFRUIT_VL6810_LIB
Adafruit_VL6180X vl = Adafruit_VL6180X();
#else
VL6180X vl;
#endif

#ifdef DISTANCE
TwoWire *theWire = NULL;
TwoWire *_i2c;
#endif
File logFile;

TaskHandle_t hServer;
TaskHandle_t hSensor;
TaskHandle_t hLoop;

#ifdef USE_ADAFRUIT_VL6810_LIB
uint8_t range;
#else
uint16_t range;
#endif
uint8_t status;

logger_t logData[SAMPLES];
logger_t logDataDbl[SAMPLES];

const size_t SZ = sizeof(logger_t) * SAMPLES;
uint8_t buf[SZ];

File myFileIn;
File myFileOut;

TickType_t testTicks;

SemaphoreHandle_t xMutex;
File UploadFile;
unsigned int localPort = 2390; // local port to listen on

char packetBuffer[255];                   //buffer to hold incoming packet
uint8_t ReplyBuffer[50] = "acknowledged"; // a string to send back

//WiFiUDP udp;
AsyncUDP udp;
IPAddress remote_IP(192, 168, 178, 30);

uint16_t millis_last;
uint16_t millis_cur;

void setupSd(fs::SDFS &FS)
{

#ifdef ESP32
  // Note: SD_Card readers on the ESP32 will NOT work unless there is a pull-up on MISO, either do this or wire one on (1K to 4K7)
  Serial.println(MISO);
  pinMode(19, INPUT_PULLUP);
#endif

  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(SD_CS_pin))
  { // see if the card is present and can be initialised. Wemos SD-Card CS uses D8
    Serial.println(F("Card failed or not present, no SD Card data logging possible..."));
    SD_present = false;
  }
  else
  {
    Serial.println(F("Card initialised... file access enabled..."));
    SD_present = true;
  }

  if (!SD.begin())
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  listDir(SD, "/", 0);
  deleteFile(SD, "/logFile.txt");
  deleteFile(SD, "/logFile1.txt");


  File myfile = SD.open("/logFile.txt", FILE_WRITE);
  const char *str = "Hello world";
  myfile.write((uint8_t *)str, 12);
  myfile.close();
}

#ifdef START_WEBSERVER
void setupWebserver(WebServer_t &server)
{
  // Setting up WebServer ************************************************************
  // //*****************************************************************************
  // // Note: Using the ESP32 and SD_Card readers requires a 1K to 4K7 pull-up to 3v3 on the MISO line, otherwise they do-not function.
  // //----------------------------------------------------------------------
  // ///////////////////////////// Server Commands
  server.on("/", HomePage);
  server.on("/download", File_Download);
  server.on("/upload", File_Upload);
  server.on("/fupload", HTTP_POST, [&]() { server.send(200); }, handleFileUpload);
  ///////////////////////////// End of Request commands
  server.begin();
  Serial.println("HTTP server started");
}

#endif

#ifdef START_WIFI

void setupWifi(WiFiClass &WiFi, WiFiMulti_t &wifiMulti)
{
  // Setting up WiFi ************************************************************
  //*****************************************************************************
  if (!WiFi.config(local_IP, gateway, subnet, dns))
  { //WiFi.config(ip, gateway, subnet, dns1, dns2);
    Serial.println("WiFi STATION Failed to configure Correctly");
  }
  wifiMulti.addAP(ssid_1, password_1); // add Wi-Fi networks you want to connect to, it connects strongest to weakest
  //wifiMulti.addAP(ssid_2, password_2); // Adjust the values in the Network tab
  //wifiMulti.addAP(ssid_3, password_3);
  //wifiMulti.addAP(ssid_4, password_4); // You don't need 4 entries, this is for example!

  Serial.println("Connecting ...");
  while (wifiMulti.run() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
  }

  // Setting up Udp ************************************************************
  //*****************************************************************************
  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  //udp.begin(localPort);

  // Setting up mDNS for the webserver ************************************************************
  //*****************************************************************************
  Serial.println("\nConnected to " + WiFi.SSID() + " Use IP address: " + WiFi.localIP().toString()); // Report which SSID and IP is in use
  // The logical name http://fileserver.local will also access the device if you have 'Bonjour' running or your system supports multicast dns
  if (!MDNS.begin(servername))
  { // Set your preferred server name, if you use "myserver" the address would be http://myserver.local/
    Serial.println(F("Error setting up MDNS responder!"));
    ESP.restart();
  }
  MDNS.addService("http", "tcp", 80);
}
#endif




void setup()
{
  delay(1000);

  //Starting serial
  Serial.begin(115200);

#ifdef START_WIFI
  setupWifi(WiFi, wifiMulti);
#endif
  // Setting up SD Card ************************************************************
  //*****************************************************************************
  setupSd(SD);

#ifdef START_WEBSERVER
  setupWebserver(server);
#endif
  SPIFFS.begin();
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readDHTTemperature().c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", readDHTHumidity().c_str());
  });

  server.on("/download1", HTTP_GET, [](AsyncWebServerRequest *request){
    //AsyncWebServerResponse *response = request->beginResponse(SD, "/logFile.txt", "application/octet-stream", true);
    AsyncWebServerResponse *response = request->beginResponse(SD, "/logFile.txt", "text/plain", true);
    // response->setContentType()
    // response->addHeader("Content-Disposition", "attachment; filename=logFile.txt");
    // response->addHeader("Connection", "close");
    request->send(response);
  });
  //   // Route for root / web page
  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  //   request->send_P(200, "text/html", index_html);
  // });
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.htm", String(), false, processor);
  });
  server.begin();
  // Setting up VL6180 ************************************************************
  //*****************************************************************************
#ifdef DISTANCE
#ifdef USE_ADAFRUIT_VL6810_LIB
  Serial.println("Adafruit VL6180x test!");
  if (!vl.begin())
  {
    Serial.println("Failed to find sensor");
    while (1)
      ;
  }
  Serial.println("Sensor found!");

#else
  if (!theWire)
  {
    _i2c = &Wire;
  }
  else
  {
    _i2c = theWire;
  }
  _i2c->begin();

  setupVl(vl);

#endif
#endif
// Setting up MPU-6050 ************************************************************
//*****************************************************************************
// Try to initialize!
#ifdef ACCEL
  setupMpu(mpu);
#endif
  // Spawning tasks  ************************************************************
  //*****************************************************************************

  xMutex = xSemaphoreCreateMutex();

#ifdef START_WEBSERVER
  Serial.println("Spawning processServer");
  xTaskCreatePinnedToCore(
      processServer,   /* Task function. */
      "processServer", /* String with name of task. */
      4096,            /* Stack size in bytes. */
      NULL,            /* Parameter passed as input of the task */
      3,               /* Priority of the task. */
      &hServer, 0);    /* Task handle. */
#endif

  //   Serial.println("Spawning processSensor");
  //   xTaskCreatePinnedToCore(
  //       processSensor,   /* Task function. */
  //       "processSensor", /* String with name of task. */
  //       4096,            /* Stack size in bytes. */
  //       NULL,            /* Parameter passed as input of the task */
  //       12,              /* Priority of the task. */
  //       &hSensor, 1);    /* Task handle. */
}

void loop() {

}


// void loop()
// {

// //Copy file
// #if 0
//   int byteCnt = 0;
//   xSemaphoreTake(xMutex, portMAX_DELAY);
//   Serial.println("Loop: taking mutex");

//   myFileIn = SD.open("/logFile.txt", FILE_READ);
//   myFileOut = SD.open("/logFile1.txt", FILE_WRITE);
//   if (!myFileIn || !myFileOut)
//   {
//     Serial.print("FileIDs: ");
//     Serial.print(int(myFileIn));
//     Serial.print(int(myFileOut));
//   }
//   while (myFileIn.available())
//   {
//     byteCnt++;
//     Serial.print("Copy byte: ");
//     Serial.println(byteCnt);
//     myFileOut.write(myFileIn.read());
//   }
//   myFileIn.close();
//   myFileOut.close();
//   Serial.println("Loop: releasing mutex");
//   xSemaphoreGive(xMutex);
// #else

// #ifdef SAVE_FILE
//   xSemaphoreTake(xMutex, portMAX_DELAY);
//   Serial.println("Loop: taking mutex");
//   logFile = SD.open("/logFile.txt", FILE_APPEND);
//   if (!logFile)
//   {
//     Serial.println("Failed to open file for appending");
//     return;
//   }
//   else
//   {
//     Serial.println("logFile opened");
//   }

//   //buf = (uint8_t *)(tmp);
//   memcpy(buf, &logDataDbl, SZ);
//   //buf[SZ] = uint8_t(0);
//   int l;
//   l = logFile.write(buf, SZ);
//   // Serial.print("Written number of bytes: ");
//   // Serial.println(l);
//   // Serial.print("Buffer: ");
//   // for (int i = 0; i < SZ; i++)
//   // {
//   //   Serial.println(buf[i]);
//   // }

//   logFile.close();
//   Serial.println("Loop: releasing mutex");

//   udp.broadcastTo(ReplyBuffer, 13, 1234);

//   xSemaphoreGive(xMutex);
//   vTaskDelay(10);
// #endif
// #endif

//   // send a reply, to the IP address and port that sent us the packet we received
//   // udp.beginPacket(remote_IP, 12345);
//   // udp.write(ReplyBuffer, 13);
//   // udp.endPacket();

//   //vTaskDelay(100);
//   //delay(10000);
//   // Serial.print("Loop: priority = ");
//   // Serial.println(uxTaskPriorityGet(NULL));
//   // processSensor((void *)(8));

//   // if (rangeDataReady())
//   // {
//   //   Serial.print("Range: ");
//   //   Serial.println(readRangeNonBlockingMillimeters());
//   // }

// #ifdef USE_ADAFRUIT_VL6810_LIB
//   range = vl.readRange();
//   status = vl.readRangeStatus();
//   millis_cur = millis();
//   //Serial.print("Sensor interval: ");
//   Serial.println(millis_cur - millis_last);
//   millis_last = millis_cur;
//   if (status == VL6180X_ERROR_NONE)
//   {
//     Serial.print("Range: ");
//     Serial.println(range);
//   }
//   else
//   {
//     Serial.print("Status:");
//     Serial.println(status);
//   }

// #else
//   // //range = vl.readRangeContinuousMillimeters();

// #if 0
//   Serial.println("Waiting for range data ready");
//   vl.writeReg(VL6180X::SYSRANGE__START, 0x01);
//   if (rangeDataReady())
//   {
//     Serial.println("Range data ready");
//     range = readRangeNonBlockingMillimeters();

//   }
// #endif

// #if 0
//   while (! (vl.readReg(VL6180X::RESULT__RANGE_STATUS) & 0x01));
//   vl.writeReg(VL6180X::SYSRANGE__START, 0x01);
//   // Serial.print("Wire status: ");
//   // Serial.println(vl.last_status);
//   delay(5);
//   while (! (vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO) & 0x04));
//   range = vl.readReg(VL6180X::RESULT__RANGE_VAL);
//   vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x07);

//   // while (! (vl.readReg(VL6180X::RESULT__RANGE_STATUS) & 0x01));
//   // vl.writeReg(VL6180X::SYSRANGE__START, 0x01);
//   // while (!(vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO) & 0x04)) ;
// #endif

// #if 0
//   // uint8_t stt = vl.readReg(VL6180X::RESULT__RANGE_STATUS);
//   // if (stt == 1) {
//   //   vl.writeReg(VL6180X::SYSRANGE__START, 0x3);
//   // }
//   uint8_t status = vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO);
//   uint8_t ready = (status & 0x04);
//   // uint8_t ret = vl.readReg(VL6180X::RESULT__RANGE_RETURN_CONV_TIME);
//   // uint8_t sys = vl.readReg(VL6180X::SYSRANGE__START);
//   // Serial.print("Return: ");
//   // Serial.println(ret);
//   // Serial.print("Sys: ");
//   // Serial.println(sys);
//   // Serial.print("Status :");
//   // Serial.println(status);
//   // Serial.print("Status :");
//   // Serial.println(stt);
//   // Serial.print("Range status: ");
//   // Serial.print(status & 0x0007);
//   // Serial.print(" ALS status: ");
//   // Serial.print((status >> 3) & 0x07);
//   // Serial.print(" Error status: ");
//   // Serial.println((status >> 6) & 0x03);
//   //return ((vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO) & 0x04) != 0);
//   while (ready == 0)
//   {
//     // Serial.print("Return: ");
//     // Serial.println(ret);
//     // Serial.print("Sys: ");
//     // Serial.println(sys);
//     // Serial.print("Status :");
//     // Serial.println(status);
//     // Serial.print("Status :");
//     // Serial.println(stt);
//     // Serial.print("Range status: ");
//     // Serial.print(status & 0x07);
//     // Serial.print(" ALS status: ");
//     // Serial.print((status >> 3) & 0x07);
//     // Serial.print(" Error status: ");
//     // Serial.println((status >> 6) & 0x03);
//     //delay(50);
//     // stt = vl.readReg(VL6180X::RESULT__RANGE_STATUS);
//     status = vl.readReg(VL6180X::RESULT__INTERRUPT_STATUS_GPIO);
//     ready = (status & 0x04);
//     // ret = vl.readReg(VL6180X::RESULT__RANGE_RETURN_CONV_TIME);
//     // sys = vl.readReg(VL6180X::SYSRANGE__START);
//   }
//   range = vl.readReg(VL6180X::RESULT__RANGE_VAL);
//   vl.writeReg(VL6180X::SYSTEM__INTERRUPT_CLEAR, 0x07);

//   // Serial.print("Return: ");
//   // Serial.println(ret);
//   // Serial.print("Sys: ");
//   // Serial.println(sys);
//   // Serial.print("Status :");
//   // Serial.println(status);
//   // Serial.print("Status :");
//   // Serial.println(stt);
//   // Serial.print("Range status: ");
//   // Serial.print(status & 0x07);
//   // Serial.print(" ALS status: ");
//   // Serial.print((status >> 3) & 0x07);
//   // Serial.print(" Error status: ");
//   // Serial.println((status >> 6) & 0x03);

// #endif



// #ifdef DISTANCE
//   if (rangeDataReady())
//   {
//     Serial.println("Range data ready");
//     range = readRangeNonBlockingMillimeters();
//   }

//   millis_cur = millis();
//   //Serial.print("Sensor interval: ");
//   Serial.print("Duration: ");
//   Serial.println(millis_cur - millis_last);
//   millis_last = millis_cur;
//   Serial.print("Range: ");
//   Serial.println(range);
//   if (vl.timeoutOccurred())
//   {
//     Serial.println(" TIMEOUT");
//   }
// #endif


// #endif

// #ifdef ACCEL

//   millis_cur = millis();
//   //Serial.print("Sensor interval: ");
//   Serial.print("Duration: ");
//   Serial.println(millis_cur - millis_last);
//   millis_last = millis_cur;

//   sensors_event_t a, g, temp;
//   mpu.getEvent(&a, &g, &temp);
//   // a.acceleration.x = 0;
//   // a.acceleration.y = 0;
//   // a.acceleration.z = 0;
//   /* Print out the values */
//   Serial.print("Acceleration X: ");
//   Serial.print(a.acceleration.x);
//   Serial.print(", Y: ");
//   Serial.print(a.acceleration.y);
//   Serial.print(", Z: ");
//   Serial.print(a.acceleration.z);
//   Serial.println(" m/s^2");

//   Serial.print("Rotation X: ");
//   Serial.print(g.gyro.x);
//   Serial.print(", Y: ");
//   Serial.print(g.gyro.y);
//   Serial.print(", Z: ");
//   Serial.print(g.gyro.z);
//   Serial.println(" rad/s");

//   Serial.print("Temperature: ");
//   Serial.print(temp.temperature);
//   Serial.println(" degC");

// #endif
// }


