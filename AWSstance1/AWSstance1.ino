#include <M5Core2.h>

// Include AWS IoT Certificates
#include "aws_iot_certs.h"

#include "WiFi.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
// Import the Arduino Json Library
// Used for sending payload information to AWS IoT Core. 
#include <ArduinoJson.h>
// Intiallize the WifiClientSecure
WiFiClientSecure net = WiFiClientSecure();
// Setup MQTT Client with 512 packet size
MQTTClient client = MQTTClient(512);

// Defining our AWS IoT Configuration
#define DEVICE_NAME "SPIROMETER"
#define AWS_IOT_ENDPOINT "a5p8uhv31rv4o-ats.iot.us-east-1.amazonaws.com"
#define AWS_IOT_TOPIC "$aws/things/SPIROMETER/shadow/update"
#define AWS_MAX_RECONNECT_TRIES 10
// Define Publish and Describe Topics
// Make sure to add your topics names
#define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
//esp32/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"

#define WIFI_NETWORK  "Test"
#define WIFI_PASS     "135792468"
#define WIFI_TIMEOUT_MS 20000


//---------------------------------------------------------
// DHT22 Sensor dummy Function
void initializeDHT22Sensor() {
  // Initialize DHT22 Sensor
  //DHT dht(DHTPIN, DHTTYPE);
  //pinMode(DHTPIN, INPUT);
  //dht.begin();  
  
  // Get the Temperature
  float temperature = 34.67;
  Serial.println(temperature);
  
  // Get Humidity Data
  float humidity = 68.95;
  Serial.println(humidity);
  
  //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  //display.clearDisplay();
  //display.setTextSize(1);
  //display.setTextColor(WHITE);
  //display.setCursor(0,8);
  Serial.println("Temp/Humidity");
  Serial.print(temperature);
  Serial.print(" / ");
  Serial.print(humidity);
  //display.display();
StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}
// End of Function
//------------------------------------------------------


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
    //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    //display.clearDisplay();
    //display.setTextSize(1);
    //display.setTextColor(WHITE);
    //display.setCursor(0,10);
    Serial.println("Connected to AWS!");
    //display.display();
    Serial.println("AWS Timed Out!");
    return;
  } else {
    //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    //display.clearDisplay();
    //display.setTextSize(1);
    //display.setTextColor(WHITE);
    //display.setCursor(0,10);
    Serial.println("Connected to AWS!");
    //display.display();
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
    //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    //display.clearDisplay();
    //display.setTextSize(1);
    //display.setTextColor(WHITE);
    //display.setCursor(0,10);
    //display.write(3);
    Serial.println(" Connected!");
    Serial.println(WiFi.localIP());
    //display.display();
  }
  
}
// End Function
//---------------------------------------
void setup(){
  M5.begin();
  Serial.begin(9600);
  connectToWifi();
  delay(2000);
  connectToAWSIoT();
}
void loop() {
  initializeDHT22Sensor();
  client.loop();
  delay(9000);
}
