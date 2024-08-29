#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <LiquidCrystal_I2C.h>
#include "time.h"

#define WIFI_SSID   "SSID_Name"
#define WIFI_PASSWORD "Password"

#define API_KEY "AIzaSyCxiCUZFwtgBr56HlOx3XtT9Y0Q12uVABQ"

#define USER_EMAIL "you@gmail.com"
#define USER_PASSWORD "password"

#define DATABASE_URL "https://dht11-ea471-default-rtdb.asia-southeast1.firebasedatabase.app/"


LiquidCrystal_I2C lcd(0x27, 20, 4);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String uid;

//Data transfered to cloud
String databasePath;
String tempPath;
String humPath;
String soilPath;
String highPath;
String HighPath;

// Setup for DHT11 sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);     
int x;
// Sensor value
float temperature;
float humidity;
int soilmoisture;
float High;

// Time value
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 3000;

//Setup for time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

// Init Time
void InitTime(int &c, int &d, int &e){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");  // day of week, month, day of month, year, hour, hour(12 hour format), minute, second
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  c = atoi(timeHour);
  char timeMinute[3];
  strftime(timeMinute,3, "%M", &timeinfo);
  d = atoi(timeMinute);
  char timeSecond[3];
  strftime(timeSecond,3, "%S", &timeinfo);
  e = atoi(timeSecond);
}

// Setup wifi
void initWiFi()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Sending sensor value to cloud function
void sendFloat(String path, float value)
{
  if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), value))
  {
    Serial.print("Writing value: ");
    Serial.print(value);
    Serial.print(" on the following path: ");
    Serial.println(path);
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}

// Setup for system
void setup()
{
  Serial.begin(9600);
  pinMode(0,OUTPUT);
  
  lcd.init();
  lcd.backlight();
  dht.begin();
  initWiFi();

  // Setup to connect to Firebase cloud
  config.api_key = API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;

  Firebase.begin(&config, &auth);

  // Connecting to Firebase
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "")
  {
    Serial.print('g.');
    delay(1000);
  }
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);
  databasePath = "/UsersData/" + uid;

  // Create path on cloud to save sensor value
  tempPath = databasePath + "/temperature";
  humPath = databasePath + "/humidity";
  soilPath = databasePath + "/soilmoisture";
  highPath = databasePath + "/high";
  HighPath = databasePath + "/High";

  // Get time in real-time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  pinMode(12,OUTPUT); // test tưới nước đúng giờ
}

// loop
void loop()
{ 
  // Get sensor value
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    soilmoisture=analogRead(33)/4;
    soilmoisture =100 - map(soilmoisture,0,1023,0,100);

    // Receive high of tree data from Firebase cloud to ESP32
    if(Firebase.RTDB.getFloat(&fbdo,highPath.c_str())){
      High = fbdo.floatData();
    }

    // Display value on LCD monitor
    lcd.setCursor(1,0);
    lcd.print("Temp: " + String(temperature) + (char)223 + "C");
    lcd.setCursor(1,1);
    lcd.print("Humidity: " + String(humidity) + "%");
    lcd.setCursor(1,2);
    lcd.print("Soilmoisture: " + String(soilmoisture) + "%");
    lcd.setCursor(1,3);
    lcd.print("High: "+ String(High) );
    lcd.setCursor(15,3);
    lcd.print("cm");
    // sendFloat(HighPath,High);
    // sendFloat(tempPath, temperature);
    // sendFloat(humPath, humidity);
    // sendFloat(soilPath,soilmoisture);
    //Send sensor data to Firebase cloud
    sendFloat(HighPath,15);
    sendFloat(tempPath, temperature);
    sendFloat(humPath, humidity);
    sendFloat(soilPath,soilmoisture);
  }
  // Get time in real-time
  int hour=0;
  int minute=0;
  int second = 0;
  InitTime(hour,minute, second);
  // Auto water in scheduled time
  if (hour == 4 && minute == 35 && second >0 && second < 4) //4h35
  {
    if(soilmoisture < 40){
      digitalWrite(12,HIGH);
      delay(20000);
      digitalWrite(12,LOW);
    }
    else if(soilmoisture >=40 && soilmoisture < 50){
      digitalWrite(12,HIGH);
      delay(18000);
      digitalWrite(12,LOW);
    }
    else if (soilmoisture>=50 && soilmoisture < 60){
      digitalWrite(12,HIGH);
      delay(16000);
      digitalWrite(12,LOW);
    } 
    else if (soilmoisture >=60 && soilmoisture < 65){
      digitalWrite(12,HIGH);
      delay(14000);
      digitalWrite(12,LOW);
    }
    else if (soilmoisture >=65 && soilmoisture < 70){
      digitalWrite(12,HIGH);
      delay(12000);
      digitalWrite(12,LOW);
    }
  }
    if (hour == 7 && minute == 0 && second >0 && second < 4)  //7h
  {
    if(soilmoisture < 40){
      digitalWrite(12,HIGH);
      delay(20000);
      digitalWrite(12,LOW);
    }
    else if(soilmoisture >=40 && soilmoisture < 50){
      digitalWrite(12,HIGH);
      delay(18000);
      digitalWrite(12,LOW);
    }
    else if (soilmoisture>=50 && soilmoisture < 60){
      digitalWrite(12,HIGH);
      delay(16000);
      digitalWrite(12,LOW);
    } 
    else if (soilmoisture >=60 && soilmoisture < 65){
      digitalWrite(12,HIGH);
      delay(14000);
      digitalWrite(12,LOW);
    }
    else if (soilmoisture >=65 && soilmoisture < 70){
      digitalWrite(12,HIGH);
      delay(12000);
      digitalWrite(12,LOW);
    }
  }
    if (hour == 10 && minute == 0 && second >0 && second < 4) //10h
  {
    if(soilmoisture < 40){
      digitalWrite(12,HIGH);
      delay(20000);
      digitalWrite(12,LOW);
    }
    else if(soilmoisture >=40 && soilmoisture < 50){
      digitalWrite(12,HIGH);
      delay(18000);
      digitalWrite(12,LOW);
    }
    else if (soilmoisture>=50 && soilmoisture < 60){
      digitalWrite(12,HIGH);
      delay(16000);
      digitalWrite(12,LOW);
    } 
    else if (soilmoisture >=60 && soilmoisture < 65){
      digitalWrite(12,HIGH);
      delay(14000);
      digitalWrite(12,LOW);
    }
    else if (soilmoisture >=65 && soilmoisture < 70){
      digitalWrite(12,HIGH);
      delay(12000);
      digitalWrite(12,LOW);
    }
    delay(2000);
  } 
}

