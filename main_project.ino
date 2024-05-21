
 

#include "Arduino_SensorKit.h" //library of DHT20 sensor
#include "LiquidCrystal_I2C.h" //library of LCD
#include "GP2YDustSensor.h" //library of dust sensor
#include "WiFi.h" //library of ESP32 wifi function
#include "Firebase_ESP_Client.h" //library of online Firebase Realtime Database (Firebase RTDB)
#include "Wire.h"

//function prototypes
void spinner();
void printLocalTime();

//Clock display config
#define NTP_SERVER1     "pool.ntp.org"  //1st time server address
#define NTP_SERVER2     "time.nist.gov" //2nd time server address
#define UTC_OFFSET      7               //your timezone (VN is UTC+7)
#define UTC_OFFSET_DST  0               //daylight saving time (0 in VN)

//Wifi config
// Insert your network credentials
#define WIFI_SSID "TuanAnh" //change this based on the name of your wifi
#define WIFI_PASSWORD "1208bangchu" //change to the password of your wifi

//Firebase RTDB config
// Insert Firebase project API Key
#define API_KEY "AIzaSyBCTagXyz4i1s125lpZQQD1cY1lJymb0Yg"

// Insert the RTDB URL
#define DATABASE_URL "https://iot-dacn-default-rtdb.firebaseio.com/" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth fbauth; //Firebase RTDB authentication variable
FirebaseConfig fbconfig; //Firebase RTDB config variable
bool signupOK = false; //variable to check if Firebase RTDB login success

//DHT20 config
#define Environment Environment_I2C
#define FAN_PIN 15
#define PWM_FREQUENCY 100 // PWM frequency in Hz (adjust as needed)
#define PWM_RESOLUTION 8  // PWM resolution (8-bit in this example)

//LCD config
LiquidCrystal_I2C LCD(0x21, 16, 2); // I2C address 0x27, 16 column and 2 rows

//built-in led
#define LED_BUILTIN 2

//SHARP GP2Y10 Dust Sensor config
const uint8_t DUST_SENSOR_LED_PIN = 14; //IR LED output pin
const uint8_t DUST_SENSOR_V0_PIN = A0; //v0 input pin

GP2YDustSensor dustSensor(GP2YDustSensorType::GP2Y1010AU0F, DUST_SENSOR_LED_PIN, DUST_SENSOR_V0_PIN);

//LED, fan and buzzer flags (1 - on; 0 - off)
//previous state flags
bool buzzer_flag = false;
bool fan_flag = false;
bool manual_flag = false;

int airq_thresh = 0;
int temp_thresh = 0;
int humid_thresh = 0;

unsigned long long int prevMillis = 0; //variable for timer
 
void setup() {
  //pin modes
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT); //onboard LED

  Wire.begin();
  Serial.begin(115200); //baud rate at 115200

  LCD.init(); // initialize the lcd
  LCD.backlight();
  LCD.setCursor(0, 0);
  LCD.print("System On");
  Serial.println("System On");
  delay(1000);

  LCD.clear();
  //wifi connection
  LCD.setCursor(0, 0);
  LCD.print("Connecting to ");
  LCD.setCursor(0, 1);
  LCD.print("WiFi ");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    spinner();
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //time update
  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.println("Online");
  LCD.setCursor(0, 1);
  LCD.println("Updating time...");
  configTime(UTC_OFFSET*3600, UTC_OFFSET_DST, NTP_SERVER1, NTP_SERVER2);

  //Firebase RTDB setup
  fbconfig.api_key = API_KEY;
  fbconfig.database_url = DATABASE_URL;
  /* Sign up */
  LCD.clear();
  if (Firebase.signUp(&fbconfig, &fbauth, "", "")){ //log into Firebase RTDB
    Serial.println("Firebase RTDB Login Success!");
    LCD.setCursor(0, 0);
    LCD.println("Firebase RTDB");
    LCD.setCursor(0, 1);
    LCD.println("Login Success!");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", fbconfig.signer.signupError.message.c_str());
    LCD.setCursor(0, 0);
    LCD.println("Firebase RTDB");
    LCD.setCursor(0, 1);
    LCD.println("Login Failed...");
  }
  Firebase.begin(&fbconfig, &fbauth);
  Firebase.reconnectWiFi(true);
  delay(1000);
  LCD.clear();

  Environment.begin(); //initiate the temp / humid sensor

  //dust sensor
  dustSensor.setBaseline(0.1);
  dustSensor.setCalibrationFactor(1.0/4.0); //since our Vcc is 3.3V, we have to scale down the 5V output of the sensor
  dustSensor.begin(); //initiate the dust sensor

  
  ledcSetup(0, PWM_FREQUENCY, PWM_RESOLUTION); // Configure PWM channel 0
  ledcAttachPin(FAN_PIN, 0); // Attach the fan pin to PWM channel 0

}
 
void loop() {
  //-----------------------------------------------------------------
  //sensor readings variables
  float tempurature = Environment.readTemperature();
  float humidity = Environment.readHumidity();
  float dust = dustSensor.getDustDensity();
  float avg_dust = dustSensor.getRunningAverage();

  //-----------------------------------------------------------------
  //blink onboard led every 2.5s to indicate it's working
  // if(millis() - prevMillis > 2500 || prevMillis == 0){
  //   prevMillis = millis();
  //   if(built_in_LED_flag == LOW){
  //     built_in_LED_flag = HIGH;
  //   }else{
  //     built_in_LED_flag = LOW;
  //   }
  //   digitalWrite(LED_BUILTIN, built_in_LED_flag);
  // }

  //-----------------------------------------------------------------
  //print to Serial
  Serial.println("Temperature = " + String(tempurature) + "*C");
  Serial.println("Humidity = " + String(humidity) + "%");
  Serial.println("Dust Density = " + String(dust) + "ug/m3");

  //-----------------------------------------------------------------
  //Firebase RTDB interactions - every 5s
  if(Firebase.ready() && signupOK && (millis() - prevMillis > 5000 || prevMillis == 0)){
  //-----------------------------------------------------------------
    //upload data (Temperature, Humidity and Dust Density) to Firebase RTDB
    //upload Temperature
    if(Firebase.RTDB.setInt(&fbdo, "Board_IoT/TEMPERATURE", (int)tempurature)){
      Serial.println("Temperature uploaded to RTDB"); //print to Serial
    }else{
      Serial.println("Failed to upload Temperature");
      Serial.println("REASON: " + fbdo.errorReason()); //report error
    }

    //upload Humidity
    if(Firebase.RTDB.setInt(&fbdo, "Board_IoT/HUMIDITY", (int)humidity)){
      Serial.println("Humidity uploaded to RTDB"); //print to Serial
    }else{
      Serial.println("Failed to upload Humidity");
      Serial.println("REASON: " + fbdo.errorReason()); //report error
    }

    //upload Dust Density
    if(Firebase.RTDB.setInt(&fbdo, "Board_IoT/AIR_Q", (int)dust)){
      Serial.println("Dust Density uploaded to RTDB"); //print to Serial
    }else{
      Serial.println("Failed to upload Dust Density");
      Serial.println("REASON: " + fbdo.errorReason()); //report error
    }


    //-----------------------------------------------------------------
    //download data flags (Manual flag & thread values) from Firebase RTDB
    //download Manual flag
    if(Firebase.RTDB.getInt(&fbdo, "Board_IoT/MANUAL")){
      if(fbdo.dataType() == "int"){
        manual_flag = fbdo.intData();
        if(manual_flag){
          Serial.println("Manual mode");
        }else{
          Serial.println("Auto mode");
        }
      }
      else{
        Serial.println("Failed to retrieve MODE activation state");
        Serial.println("REASON: " + fbdo.errorReason()); //report error
      }
    }

    //download humid thresh
    if(Firebase.RTDB.getInt(&fbdo, "Board_IoT/HUMID_THRESH")){
      if(fbdo.dataType() == "int"){
        humid_thresh = fbdo.intData();
        Serial.println("HUMID_THRESH: ");
        Serial.println(humid_thresh);
      }
      else{
        Serial.println("Failed to retrieve Fan activation state");
        Serial.println("REASON: " + fbdo.errorReason()); //report error
      }
    }
            
    //download temp thresh
    if(Firebase.RTDB.getInt(&fbdo, "Board_IoT/TEMP_THRESH")){
      if(fbdo.dataType() == "int"){
        temp_thresh = fbdo.intData();
        Serial.println("TEMP_THRESH: ");
        Serial.println(temp_thresh);
      }
      else{
        Serial.println("Failed to retrieve Fan activation state");
        Serial.println("REASON: " + fbdo.errorReason()); //report error
      }
    }
            
    //download airq thresh
    if(Firebase.RTDB.getInt(&fbdo, "Board_IoT/AIRQ_THRESH")){
      if(fbdo.dataType() == "int"){
        airq_thresh = fbdo.intData();
        Serial.println("AIRQ_THRESH: ");
        Serial.println(airq_thresh);
      }
      else{
        Serial.println("Failed to retrieve Fan activation state");
        Serial.println("REASON: " + fbdo.errorReason()); //report error
      }
    }

    if(manual_flag == 1){
      //-----------------------------------------------------------------
      //download data flags (Fan and Buzzer activation flags) from Firebase RTDB
      //download LED flag
      if(Firebase.RTDB.getInt(&fbdo, "Board_IoT/FAN")){
        if(fbdo.dataType() == "int"){
          fan_flag = fbdo.intData();
          if(fan_flag){
            Serial.println("FAN is ON");
          }else{
            Serial.println("FAN is OFF");
          }
        }
        else{
          Serial.println("Failed to retrieve FAN activation state");
          Serial.println("REASON: " + fbdo.errorReason()); //report error
        }
      }

      //download Buzzer flag
      if(Firebase.RTDB.getInt(&fbdo, "Board_IoT/BUZZER")){
        if(fbdo.dataType() == "int"){
          buzzer_flag = fbdo.intData();
          if(buzzer_flag){
            Serial.println("Buzzer is ON");
          }else{
            Serial.println("Buzzer is OFF");
          }
        }
        else{
          Serial.println("Failed to retrieve Buzzer activation state");
          Serial.println("REASON: " + fbdo.errorReason()); //report error
        }
      }

      // fan
      if(fan_flag == 1){
        ledcWrite(0, 20);
      } else {
        ledcWrite(0, 0);
      }

      // buzzer 
      if(buzzer_flag == 1){
        // todo
      }

    } else {
        if(tempurature > temp_thresh || humidity > humid_thresh || dust > airq_thresh) {
          // fan
          ledcWrite(0, 20);
          fan_flag = 1;

          // buzzer
          // todo
          // buzzer_flag = 1;
        } else {
          ledcWrite(0, 0);
          fan_flag = 0;

          // buzzer
          // todo
          // buzzer_flag = 0;
        }

        if(fan_flag){
          Serial.println("FAN is ON");
        }else{
          Serial.println("FAN is OFF");
        }

      //upload Fan flag
      if(Firebase.RTDB.setInt(&fbdo, "Board_IoT/FAN", (int)fan_flag)){
        Serial.println("Fan activation state uploaded to RTDB"); //print to Serial
      }else{
        Serial.println("Failed to upload Fan activation state");
        Serial.println("REASON: " + fbdo.errorReason()); //report error
      }

      //upload Dust Density
      if(Firebase.RTDB.setInt(&fbdo, "Board_IoT/BUZZER", (int)buzzer_flag)){
        Serial.println("Buzzer activation state uploaded to RTDB"); //print to Serial
      }else{
        Serial.println("Failed to upload Buzzer activation state");
        Serial.println("REASON: " + fbdo.errorReason()); //report error
      }
    }
  }

  //-----------------------------------------------------------------
  //print to LCD (6s)
  //display temperature and humidity (3s)
  LCD.clear();     
  LCD.setCursor(0, 0);
  LCD.print("Temp: " + String(tempurature) + "*C");
  LCD.setCursor(0, 1);
  LCD.print("Humi: " + String(humidity) + "%");
  delay(3000);
  LCD.clear();
  //display dust density (3s)
  LCD.setCursor(0, 0);
  LCD.print("Dust Density:");
  LCD.setCursor(0, 1);
  LCD.print(String(dust) + "ug/m3");
  delay(3000);
  LCD.clear();

  //-----------------------------------------------------------------
  //time display (4s)
  for(int i = 0; i < 20; i++){
    printLocalTime();
    delay(200);
  }
  LCD.clear();
}

void spinner(){ //spinner function (to look fun while connecting to the wifi)
  static int8_t counter = 0;
  const char* glyphs = "\xa1\xa5\xdb";
  LCD.setCursor(15, 1);
  LCD.print(glyphs[counter++]);
  if (counter == strlen(glyphs)) {
    counter = 0;
  }
}

void printLocalTime(){ //print time on LCD
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)){//check time server connection
    LCD.setCursor(0, 0);
    LCD.println("Time Server");
    LCD.setCursor(0, 1);
    LCD.println("Connection Err");
    return;
  }

  //LCD.clear();

  //print time and timezone
  LCD.setCursor(0, 0);
  LCD.println(&timeinfo, "%Z%z"); //print timezone
  LCD.setCursor(8, 0);
  LCD.println(&timeinfo, "%H:%M:%S"); //print time

  //print day of the week and current date
  LCD.setCursor(0, 1);
  LCD.println(&timeinfo, "%a"); //print day of the week
  LCD.setCursor(8, 1);
  LCD.println(&timeinfo, "%d/%m/%y"); //print date
}