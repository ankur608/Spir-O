#include "aws_iot_certs.h"

#include "WiFi.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
WiFiClientSecure net = WiFiClientSecure();
// Setup MQTT Client with 512 packet size
MQTTClient client = MQTTClient(512);

// Defining our AWS IoT Configuration // 
#define DEVICE_NAME "SPIROMETER"
#define AWS_IOT_ENDPOINT "a5p8uhv31rv4o-ats.iot.us-east-1.amazonaws.com"
#define AWS_IOT_TOPIC "$aws/things/SPIROMETER/shadow/update"
#define AWS_MAX_RECONNECT_TRIES 10
// Define Publish and Describe Topics
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

#define WIFI_NETWORK  "Test"
#define WIFI_PASS     "135792468"
#define WIFI_TIMEOUT_MS 20000
////////////////////////////////////////
#include <M5Core2.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <FlowMeter.h> 
#include <WiFi.h>
#include "time.h"
#include "FastLED.h"
#include <SparkFunBME280.h>
#include <SparkFunCCS811.h>

#define CCS811_ADDR 0x5B //Default I2C Address
#define PIN_NOT_WAKE 5
#define LEDS_PIN 25
#define LEDS_NUM 10
CRGB ledsBuff[LEDS_NUM];

CCS811 myCCS811(CCS811_ADDR);
BME280 myBME280;
FlowMeter *Meter;

boolean pftRun = false;
boolean inspFlag = true;
int humid_baseline, humid_corr;
float psiPressure;
float psiMult = 0.000145;
const unsigned long period = 1000;
float EOTI = 0.025; //End of Test Indication(0.025 litres/second {+/-} 200ml/sec)
float MFR = 0.69;   //Modest Flow Rate(0.69 litres/second)
float LFR = 0.5;    //Low Flow Rate(0.5 litres/second)
float FEV1, PEF, FVC;

uint8_t val = 0;
extern const unsigned char previewR[120264];
extern const unsigned char bibiSig[8820];

//const char* ssid       = "Test";
//const char* password   = "135792468";
//const char* ntpServer = "in.pool.ntp.org";
//const long  gmtOffset_sec = 19800;
//const int   daylightOffset_sec = 0;//3600

#define CONFIG_I2S_BCK_PIN 12
#define CONFIG_I2S_LRCK_PIN 0
#define CONFIG_I2S_DATA_PIN 2
#define CONFIG_I2S_DATA_IN_PIN 34

#define Speak_I2S_NUMBER I2S_NUM_0

#define MODE_MIC 0
#define MODE_SPK 1
#define DATA_SIZE 1024

bool InitI2SSpeakOrMic(int mode)
{
    esp_err_t err = ESP_OK;

    i2s_driver_uninstall(Speak_I2S_NUMBER);
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 2,
        .dma_buf_len = 128,
    };
    if (mode == MODE_MIC)
    {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    }
    else
    {
        i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_config.use_apll = false;
        i2s_config.tx_desc_auto_clear = true;
    }
    err += i2s_driver_install(Speak_I2S_NUMBER, &i2s_config, 0, NULL);
    i2s_pin_config_t tx_pin_config;

    tx_pin_config.bck_io_num = CONFIG_I2S_BCK_PIN;
    tx_pin_config.ws_io_num = CONFIG_I2S_LRCK_PIN;
    tx_pin_config.data_out_num = CONFIG_I2S_DATA_PIN;
    tx_pin_config.data_in_num = CONFIG_I2S_DATA_IN_PIN;
    err += i2s_set_pin(Speak_I2S_NUMBER, &tx_pin_config);
    err += i2s_set_clk(Speak_I2S_NUMBER, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

    return true;
}

void MeterISR() {
    // let our flow meter count the pulses
    Meter->count();
}


void setup() {
  M5.begin();
  SpeakInit();
  M5.Lcd.clear();
  M5.Lcd.setBrightness(200);
  M5.Lcd.drawJpgFile(SD, "/sl1.jpg");
  for(int x=0; x<=1; x++)
  {
   Beep();
   delay(140); 
  }
  delay(5000);
  //delay(1000);
  Serial.begin(115200);
  Wire.begin();
  M5.Lcd.drawJpgFile(SD, "/sl2.jpg");
  delay(7000);
  M5.Lcd.drawJpgFile(SD, "/sl3.jpg");
  delay(8000);
  M5.Lcd.drawJpgFile(SD, "/sl4.jpg");
  delay(12000);
  //delay(1000);  //Excluding.
  Meter = new FlowMeter(digitalPinToInterrupt(14), UncalibratedSensor, MeterISR, FALLING);
  myCCS811.begin();
  delay(100);
  myBME280.begin();
  //Initialize BME280
  myBME280.settings.commInterface = I2C_MODE;
  myBME280.settings.I2CAddress = 0x77;
  myBME280.settings.runMode = 3; //Normal mode
  myBME280.settings.tStandby = 0;
  myBME280.settings.filter = 4;
  myBME280.settings.tempOverSample = 5;
  myBME280.settings.pressOverSample = 5;
  myBME280.settings.humidOverSample = 5;
  delay(10);  //Make sure sensor had enough time to turn on. BME280 requires 2ms to start up.
  M5.Lcd.clearDisplay();
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.setTextColor(BLACK, WHITE);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(2);  
    
  devAuth();
  delay(1000);
  M5.Lcd.setCursor(87, 190);
  M5.Lcd.print("Scan code..");

  FastLED.addLeds<SK6812, LEDS_PIN>(ledsBuff, LEDS_NUM);
    for (int i = 0; i < LEDS_NUM; i++)
    {
        ledsBuff[i].setRGB(0, 0, 20);
    }
      FastLED.show();
      delay(9000);
      M5.Lcd.clearDisplay();
      splash_homescreen();
      connectToWifi();
      delay(2000);
      connectToAWSIoT();
}


void loop() {
  M5.update();
  if(M5.BtnA.wasPressed()) {
    M5.Lcd.clearDisplay();
    splash_PFT1();
    getSensor();
    delay(8000);
    humid_baseline = myBME280.readFloatHumidity();
    for(int m=0; m<=4; m++)
    {     Beep();  delay(600);     }
    splash_screenTestPFT();
    //Meter->reset();
      runPFTAdv();
      FEV1 = Meter->getCurrentVolume();
      PEF = Meter->getCurrentFlowrate()/60.0f;
//      Serial.print("FEV(1) : ");
//      Serial.print(FEV1);
//      Serial.print(" litres, \tPEF : ");
//      Serial.print(PEF);
//      Serial.println(" litres/sec");
      
      for (int count=1; count<=5; count++)
      {
        runPFTAdv();
      }
      FVC = Meter->getTotalVolume();
      getSensor();
      humid_corr = myBME280.readFloatHumidity();
//      Serial.print("FVC : ");
//      Serial.print(FVC);
//      Serial.println(" litres");

      if (humid_corr - humid_baseline > 5) //to be calibrated for hpa(unit). 
      {
        Serial.println("Expiration Maneuver");
        inspFlag = false;
      }
      else 
      {
        Serial.println("Inspiration Maneuver");
        inspFlag = true;
      }
      
    for (int n=0; n<9; n++)
    {    Beep();     }    
    delay(100);
    splash_PFT2();
    val = 0;
    delay(15000);
    splash_homescreen();
  }
  
  if(M5.BtnB.wasPressed()) {
    M5.Lcd.clearDisplay();
    splash_VOC1();
    delay(5000);
    splash_screenTestVOC();
    //delay(6000);
    getSensor();
    delay(500);  // Modified from 2 second to 0.5 second
    splash_VOC2();
    val = 0;
    delay(15000);
    splash_homescreen();
  }
  
  if(M5.BtnC.wasPressed()) {
    M5.Lcd.clearDisplay();
    splash_exit();
    getSensor();
    enablePacketCapture();
    for (int c=0; c<3; c++)
    {
    client.loop();
    delay(5000);
    }
    
  }
  
}


////////////////////////////////////////////////////////////////////////////////////////////////////////

void devAuth()
{
  M5.Lcd.qrcode("https://console.aws.amazon.com/iot/home?region=us-east-1#/dashboard",65,0,190,6);
}

void splash_homescreen()//OK.
{
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(ORANGE , BLACK);
  M5.Lcd.setTextFont(2); //Added.
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(60, 8);
  M5.Lcd.print("SpirO Test");

  M5.Lcd.setTextColor(GREENYELLOW , BLACK);
  M5.Lcd.setTextSize(2);  
  M5.Lcd.setCursor(10, 100);
  M5.Lcd.print("Press Button A for PFT ");
  M5.Lcd.setCursor(10, 140);
  M5.Lcd.print("Press Button B for VOC ");
  M5.Lcd.setCursor(10, 180);
  M5.Lcd.print("Press Button C for EXIT ");
}

void splash_screenTestPFT()//OK.
{
  M5.Lcd.fillScreen(GREENYELLOW);
  M5.Lcd.setTextColor(BLACK , GREENYELLOW);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(43, 110);
  M5.Lcd.print("Test in Progress...");
  for (int i=0; i<=100; i++)
  {
  M5.Lcd.progressBar(40, 150, 240, 20, val);    //Display a progress bar with a width of 240 and a 20% progress at (0, 0)
  val++;
  delay(100);
  }
}

void splash_screenTestVOC()//OK.
{
  //M5.Lcd.clearDisplay();
  M5.Lcd.fillScreen(ORANGE);
  M5.Lcd.setTextColor(BLACK , ORANGE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(43, 110);
  M5.Lcd.print("Test in Progress...");
  for (int i=0; i<=100; i++)
  {
  M5.Lcd.progressBar(40, 150, 240, 20, val);    //Display a progress bar with a width of 240 and a 20% progress at (0, 0)
  val++;
  delay(50);
  }
}

void splash_PFT1()
{
  M5.Lcd.fillScreen(GREENYELLOW);
  M5.Lcd.setTextColor(NAVY, GREENYELLOW);
  M5.Lcd.setTextFont(2); //Added.
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(28, 0);
  M5.Lcd.print("PFT-Diagnosis");

  M5.Lcd.setTextColor(BLACK, GREENYELLOW);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(20, 80);
  M5.Lcd.print("- CALM DOWN AND TAKE DEEP BREATHS");
  M5.Lcd.setCursor(20, 120);
  M5.Lcd.print("- PUT YOUR LIPS OVER MOUTHPIECE");
  M5.Lcd.setCursor(20, 160);
  M5.Lcd.print("- INHALE WITH FULL CAPACITY");
  M5.Lcd.setCursor(20, 200);                                                                                               
  M5.Lcd.print("- EXHALE WITH FULL CAPACITY");
}

void splash_PFT2() //OK.
{
  M5.Lcd.fillScreen(GREENYELLOW);
  M5.Lcd.setTextColor(BLACK, GREENYELLOW);
  M5.Lcd.setTextSize(2);  
  M5.Lcd.setCursor(20, 40);
  M5.Lcd.print("FVC :  ");
  M5.Lcd.setCursor(20, 70);
  M5.Lcd.print("FEV(1) :  ");
  M5.Lcd.setCursor(20, 100);
  M5.Lcd.print("PEFR :  ");
  M5.Lcd.setCursor(20, 150);
  M5.Lcd.print("FEV1/FVC :  ");
  M5.Lcd.setTextColor(RED, GREENYELLOW);
  M5.Lcd.setCursor(150, 40);
  M5.Lcd.print(FVC);
  M5.Lcd.print(" lts.");
  M5.Lcd.setCursor(150, 70);
  M5.Lcd.print(FEV1);
  M5.Lcd.print(" lts.");
  M5.Lcd.setCursor(150, 100);
  M5.Lcd.print(PEF);
  M5.Lcd.print(" lt/s");
  M5.Lcd.setCursor(190, 150);
  M5.Lcd.print((FEV1/FVC)*100);
  M5.Lcd.print(" %");
  //M5.Lcd.setCursor(20, 160);
  //M5.Lcd.print("Vt :  ");
  //M5.Lcd.setCursor(20, 190);
  //M5.Lcd.print("TLC :  ");
  //DingDong();
  M5.Lcd.setCursor(80, 190);
  if (inspFlag == false)
  {
    M5.Lcd.print("EXPIRATION");
  }
  else
  {
    M5.Lcd.print("INSPIRATION");
  }
  delay(100);
}

void splash_VOC1()
{
  M5.Lcd.fillScreen(ORANGE);
  M5.Lcd.setTextColor(WHITE, ORANGE);
  M5.Lcd.setTextFont(2); //Added.
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(20, 0);
  M5.Lcd.print("xVOC-Diagnosis");

  M5.Lcd.setTextColor(BLACK, ORANGE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 120);
  M5.Lcd.print(" BREATHE LIGHTLY FOR  FEW SECONDS THROUGH MOUTHPIECE");
}

void splash_VOC2() //OK.
{
  M5.Lcd.fillScreen(ORANGE);
  M5.Lcd.setTextColor(BLACK, ORANGE);
  M5.Lcd.setTextFont(2); //Added.
  M5.Lcd.setTextSize(2);  
  M5.Lcd.setCursor(20, 40);
  M5.Lcd.print("Air-Temp......");
  M5.Lcd.setCursor(20, 70);
  M5.Lcd.print("Air-Humi......");
  M5.Lcd.setCursor(20, 100);
  M5.Lcd.print("eCO2..........");
  M5.Lcd.setCursor(20, 130);
  M5.Lcd.print("TVOC..........");
  M5.Lcd.setCursor(20, 160);
  M5.Lcd.print("Pressure......");
  M5.Lcd.setTextColor(RED, ORANGE);
  M5.Lcd.setCursor(180, 40);
  M5.Lcd.print(myBME280.readTempC(), 2);
  M5.Lcd.print(" C");
  M5.Lcd.setCursor(180, 70);
  M5.Lcd.print(myBME280.readFloatHumidity(), 2);
  M5.Lcd.print(" %");
  M5.Lcd.setCursor(180, 100);
  M5.Lcd.print(myCCS811.getCO2());
  M5.Lcd.print(" ppm");
  M5.Lcd.setCursor(180, 130);
  M5.Lcd.print(myCCS811.getTVOC());
  M5.Lcd.print(" ppb");
  M5.Lcd.setCursor(180, 160);
  M5.Lcd.print(psiPressure);
  M5.Lcd.print(" psi");
  getSensor();
  for(int x=0; x<=3; x++)
  {
   Beep();
   delay(200); 
  }
  delay(100);
}

void splash_exit() //OK.
{
  M5.Lcd.fillScreen(NAVY);
  M5.Lcd.setTextColor(WHITE, NAVY);
  M5.Lcd.setTextFont(2); //Added.
  M5.Lcd.setTextSize(2);  
  M5.Lcd.setCursor(40, 80);
  M5.Lcd.print(" BYE! & Take Care. ");
  DingDong();
  M5.Lcd.setCursor(0, 120);
  M5.Lcd.print(" Syncing data to AWS...");
  //initTime();
  //printLocalTime();
  //M5.Lcd.setCursor(160, 180);
  //M5.Lcd.print("4:27");
  //M5.Lcd.print(&timeinfo, "%H:%M");
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void getSensor()
{
//Check to see if data is available
  if (myCCS811.dataAvailable())
  {
    //Calling this function updates the global tVOC and eCO2 variables
    myCCS811.readAlgorithmResults();
    //printInfoSerial fetches the values of tVOC and eCO2
    printInfoSerial();

    float BMEtempC = myBME280.readTempC();
    float BMEhumid = myBME280.readFloatHumidity();
//    Serial.print("Applying new values (deg C, %): ");
//    Serial.print(BMEtempC);
//    Serial.print(",");
//    Serial.println(BMEhumid);
//    Serial.println();

    //This sends the temperature data to the CCS811
    myCCS811.setEnvironmentalData(BMEhumid, BMEtempC);
  }
//  else if (myCCS811.checkForStatusError())
//  {
//    //If the CCS811 found an internal error, print it.
//    printSensorError();
//  }  
}


//---------------------------------------------------------------
void printInfoSerial()
{
  //getCO2() gets the previously read data from the library
  Serial.println("CCS811 data:");
  Serial.print(" CO2 concentration : ");
  Serial.print(myCCS811.getCO2());
  Serial.println(" ppm");

  //getTVOC() gets the previously read data from the library
  Serial.print(" TVOC concentration : ");
  Serial.print(myCCS811.getTVOC());
  Serial.println(" ppb");

  Serial.println("BME280 data:");
  Serial.print(" Temperature: ");
  Serial.print(myBME280.readTempC(), 2);
  Serial.println(" degrees C");

  Serial.print(" %RH: ");
  Serial.print(myBME280.readFloatHumidity(), 2);
  Serial.println(" %");

  Serial.print(" Pressure: ");
  Serial.print(myBME280.readFloatPressure(), 2);
  Serial.println(" Pa");
  psiPressure = myBME280.readFloatPressure()*psiMult;
  Serial.print(psiPressure);
  Serial.println(" psi");
  Serial.println();
}
///////////////////////////////////////////////////////////////////////////////////
//printDriverError decodes the CCS811Core::status type and prints the
//type of error to the serial terminal.
//
//Save the return value of any function of type CCS811Core::status, then pass
//to this function to see what the output was.
/*
void printDriverError( CCS811Core::status errorCode )
{
  switch ( errorCode )
  {
    case CCS811Core::SENSOR_SUCCESS:
      Serial.print("SUCCESS");
      break;
    case CCS811Core::SENSOR_ID_ERROR:
      Serial.print("ID_ERROR");
      break;
    case CCS811Core::SENSOR_I2C_ERROR:
      Serial.print("I2C_ERROR");
      break;
    case CCS811Core::SENSOR_INTERNAL_ERROR:
      Serial.print("INTERNAL_ERROR");
      break;
    case CCS811Core::SENSOR_GENERIC_ERROR:
      Serial.print("GENERIC_ERROR");
      break;
    default:
      Serial.print("Unspecified error.");
  }
}

//printSensorError gets, clears, then prints the errors
//saved within the error register.
void printSensorError()
{
  uint8_t error = myCCS811.getErrorRegister();

  if ( error == 0xFF ) //comm error
  {
    Serial.println("Failed to get ERROR_ID register.");
  }
  else
  {
    Serial.print("Error: ");
    if (error & 1 << 5) Serial.print("HeaterSupply");
    if (error & 1 << 4) Serial.print("HeaterFault");
    if (error & 1 << 3) Serial.print("MaxResistance");
    if (error & 1 << 2) Serial.print("MeasModeInvalid");
    if (error & 1 << 1) Serial.print("ReadRegInvalid");
    if (error & 1 << 0) Serial.print("MsgInvalid");
    Serial.println();
  }
}
*/
///////////////////////////////////////////////////////
void SpeakInit(void)
{
  M5.Axp.SetSpkEnable(true);
  InitI2SSpeakOrMic(MODE_SPK);
}

void DingDong(void)
{
  size_t bytes_written = 0;
  i2s_write(Speak_I2S_NUMBER, previewR, 120264, &bytes_written, portMAX_DELAY);
}

void Beep(void)
{
  size_t bytes_written = 0;
  i2s_write(Speak_I2S_NUMBER, bibiSig, 8820, &bytes_written, portMAX_DELAY);
}
/*
void runPFT()
{
  delay(period);
  Meter->tick(period);
  Serial.println("Currently " + String(Meter->getCurrentFlowrate()) + " l/min, " + String(Meter->getTotalVolume())+ " l total.");
}
*/
void runPFTAdv()
{
  delay(period);
  Meter->tick(period);
  if((Meter->getCurrentFlowrate()/60.0f) > EOTI)
    {
      pftRun = true;
      Serial.println("Test in Progress...");
//      Serial.print(Meter->getTotalDuration()/1000);
//      Serial.println("sec");
//      Serial.print(Meter->getCurrentFrequency());
//      Serial.println("cycles");
//      Serial.print((Meter->getCurrentFlowrate()/60.0f));
//      Serial.println("litre/sec");
//      Serial.print(Meter->getCurrentVolume());
//      Serial.println("lt");
//      Serial.print(Meter->getTotalVolume());
//      Serial.println("litres");    
//      Serial.println(); 

//      if ( ((Meter->getTotalDuration()/1000) / (Meter->getTotalDuration()/1000)) == 1 )
//      {
//      Serial.println("1 second stats: ");
//      Serial.print((Meter->getCurrentFlowrate()/60.0f));
//      Serial.print("litre/sec,    ");
//      Serial.print(Meter->getCurrentVolume());
//      Serial.print("lt,   ");
//      Serial.print(Meter->getTotalVolume());
//      Serial.println("litres");    
//      }
    }
    else
    {
      pftRun = false;
      Serial.println("No test..."); 
    }
}

///////////////////////////////////////////////////////////////
/*
void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void initTime()
{
  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" CONNECTED");
  
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
*/
//////////////////////////////////////////////////////////////

// AWS Connection Function
void connectToAWSIoT() {
  // Adding the IoT Certificates for our AWS IoT Thing
  net.setCACert(AWS_ROOT_CA_CERT);
  net.setCertificate(AWS_CLIENT_CERT);
  net.setPrivateKey(AWS_PRIVATE_KEY);
// Associating the Endpoint and Port Number
  client.begin(AWS_IOT_ENDPOINT, 8883, net);
// Setting up Retry Count
  int retries = 0;
  Serial.print("Connecting to AWS IOT");
// Attempting Connection in While Loop
  while (!client.connect(DEVICE_NAME) && retries < AWS_MAX_RECONNECT_TRIES) {
    Serial.print(".");
    delay(100);
    retries++;
  }
// Setup If Else Statement to handle connections and failures. 
  if(!client.connected()){
    Serial.println("Connected to AWS!");
    Serial.println("AWS Timed Out!");
    return;
  } else {
    Serial.println("Connected to AWS!");
    Serial.println("Connected to AWS IoT Thing!");
    // Subscribe to the AWS IoT Device 
    client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  }
}
// End of Function
//------------------------------------------------------
// Connection to Wifi Function
void connectToWifi() {
  Serial.print("Connecting to Wifi");
  // In order to connect to an existing network we need to utilize station mode. 
  WiFi.mode(WIFI_STA);
  
  // Connect to the Network Using SSID and Pass. 
  WiFi.begin(WIFI_NETWORK, WIFI_PASS);
// Store the time it takes for Wifi to connect. 
  unsigned long startAttemptTime = millis();
// The While loop utilize the Wifi Status to check if its connect as well as makes sure that the timeout was not reached. 
  while(WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS){   
    Serial.print(".");
    
    // Deply so this while loop does not run so fasts. 
    delay(100);
  }
if(WiFi.status() != WL_CONNECTED){
    Serial.println("Failed to Connect to Wifi");
    esp_deep_sleep_start();
  } else {
    Serial.println(" Connected!");
    Serial.println(WiFi.localIP());
  }
  
}
// End Function
//////////////////////////////////////////
void enablePacketCapture()
{
  float temperature = myBME280.readTempC();
  Serial.println(temperature); 
  float humidity = myBME280.readFloatHumidity();
  Serial.println(humidity);

  Serial.println("Temp/Humidity");
  Serial.print(temperature);
  Serial.print(" / ");
  Serial.println(humidity);
  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}
// End of Function
