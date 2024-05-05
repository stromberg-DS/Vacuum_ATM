/* 
 * Get Dust & Vacuum General Logic
 * Get dust values from plant. Test vacuum logic and timing.
 * 
 * Author: Daniel Stromberg
 * Date: 4/20/24
*/

#include "Particle.h"
#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "credentials.h"
#include <neopixel.h>
#include "Button_DS.h"
#include "Timer_DS.h"


SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

const int PIXEL_COUNT = 14;
const int PUBLISH_TIME = 30000;
const int RED = 0xFF0000;     //not ready to vacuum
const int BLUE = 0x0000FF;    //successfully vacuumed!
const int GREEN = 0x00FF00;   //ready to vacuum
const int YELLOW = 0xFFFF00;  //currently vacuuming
const int MAGENTA = 0xFF00FF; //need more vacuuming
const int WHITE = 255;        //Take a reward!
const int SERVO_PIN = A5;
const int CAM_PIN = D18;

//Vacuum States
int vacuumState;
int lastVacuumState;
const int CHARGING_NOT_DIRTY = 0; //red
const int CHARGING_YES_DIRTY = 1; //green
const int NOW_VACUUM_NO_REWARD = 2; //yellos
const int NOW_VACUUM_REWARD_READY =3; //
const int STOPPED_EARLY = 4;
const int FINISHED_TAKE_REWARD = 5;
const String VAC_STATE_STRING[6] = {"Charging, not dirty.", "Charging, dirty", "Vacuuming, reward not ready",
                                    "Vacuuming, reward ready", "Stopped early", "Finished, take reward"};
int vacStartTime;
int elapsedVacTime=0;   //total time spent vacuuming
int prevVacTime = 0;    //previous total time vacuuming
int lastVacStateTime;   //last time the vacuum state changed
bool isReadyToDispense = false; //After vacuum is returned & has been used enough set true


const int VACUUMING_TIME = 5000;   //900,000 is 15 min
const int MAX_DUST = 500;       //3,500,000 - roughly 1 week @500 particles/15 min 
const int MAX_TIME_SINCE_VAC = 30;            //1,209,600sec = 14 days

//EEPROM Setup
int len = EEPROM.length();
int totalDustAddress = 0x0001; //4 bytes from 0x0001 to 0x0004
int timeAddress = 0x0010;   //

bool isLEDOn = false;
unsigned int totalDust = 0; //4 bytes - 
float totalDustK = 0;
int lastRXTime = 0;

//Time
unsigned int previousUnixTime;
unsigned int currentUnixTime;
time32_t now();

//Functions
void MQTT_connect();
void getNewDustData();
void adaPublish();
void dustToBytes(int dustIn, byte *dustHOut, byte *dustMOut, byte *dustLOut);
void newDataLEDFlash();
void fillLEDs(int ledColor, int startLED=0, int lastLED=PIXEL_COUNT);
void moveServo(int position);

TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe dustSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/plantinfo.dustsensor");
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/totaldust");

// Timer publishTimer(PUBLISH_TIME, adaPublish);

Adafruit_NeoPixel pixel(PIXEL_COUNT, SPI1, WS2812);
Servo myServo;
Button vacButton(A2);
Button camButton(CAM_PIN);
IoTTimer flashTimer;
IoTTimer servoTimer;

void setup() {
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    myServo.attach(SERVO_PIN);
    myServo.write(140);
    pixel.begin();
    pixel.setBrightness(10);
    fillLEDs(0x0000FF);
    pixel.show();
    delay(1000);
    fillLEDs(0xFF0000);
    pixel.show();
    myServo.detach();

    Serial.printf("Connecting to Particle cloud...");
    while(!Particle.connected()){
        //wait to connect to particle cloud
    }

    previousUnixTime = EEPROM.get(timeAddress, previousUnixTime);
    Serial.printf("PreviousTime: %u\n\n", previousUnixTime);


    totalDust = EEPROM.get(totalDustAddress, totalDust);

    // publishTimer.start();

    mqtt.subscribe(&dustSub);

    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);

}

void loop() {
    static int lastPrintTime;
    MQTT_connect();

    unsigned int timeSinceVacuumed;
    currentUnixTime = Time.now();
    timeSinceVacuumed = currentUnixTime - previousUnixTime;

    if(vacuumState != lastVacuumState){
        Serial.printf("New Vacuum State:\n  %s\n\n", VAC_STATE_STRING[vacuumState].c_str());
        lastVacStateTime = millis();    //track when state changes
        lastVacuumState = vacuumState;
    }

    if(millis()-lastPrintTime > 1000){
        Serial.printf("Dust: %i\nTime: %i\nTotal Vac time: %i\n", totalDust, timeSinceVacuumed,elapsedVacTime);
        Serial.printf("%i\n\n", camButton.isPressed());
        lastPrintTime = millis();
    }

    getNewDustData();
    newDataLEDFlash();


  ////Treat button like real vacuum
  ////  pressed = vacuum on charger
  ////  released = taking off charger
  ////  not pressed = vacuuming
  ////  clicked = putting back on charger
  //
  //If the house is dirty or it has been too long...
  if((totalDust > MAX_DUST) || (timeSinceVacuumed>MAX_TIME_SINCE_VAC)){
    fillLEDs(GREEN);
    vacuumState = CHARGING_YES_DIRTY;

    if(vacButton.isReleased()){      //When vacuum removed
      vacStartTime = millis();        //set the start vacuum timer
    }
    //if you are vacuuming
    if(!vacButton.isPressed()){
      elapsedVacTime = prevVacTime+ (millis() - vacStartTime);
      if(elapsedVacTime > VACUUMING_TIME){  //check if you have vacuumed long enough
        fillLEDs(BLUE);
        vacuumState = NOW_VACUUM_REWARD_READY;
      } else{
        fillLEDs(YELLOW);
        vacuumState = NOW_VACUUM_NO_REWARD;
      }
    }
    //If the vacuum is returned
    if(vacButton.isClicked()){         
      if(elapsedVacTime > VACUUMING_TIME){  //check if you have vacuumed enough
        totalDust =0;
        totalDustK = 0;
        timeSinceVacuumed = 0;
        elapsedVacTime =0;
        prevVacTime = 0;
        previousUnixTime = currentUnixTime;
        EEPROM.put(timeAddress, currentUnixTime);   //log last vacuum time in EEPROM
        vacuumState = FINISHED_TAKE_REWARD;
        servoTimer.startTimer(2000);
        fillLEDs(WHITE);
        isReadyToDispense = true;
      }else{
        fillLEDs(GREEN);
        isReadyToDispense = false;
        prevVacTime = elapsedVacTime;
        vacuumState = STOPPED_EARLY;
        flashTimer.startTimer(2000);
      }
    }
  }else{
    fillLEDs(RED); 
    vacuumState = CHARGING_NOT_DIRTY;
  }

if(isReadyToDispense){
  if(camButton.isClicked()){
    moveServo(10);
  } else if (camButton.isReleased()){
    isReadyToDispense = false;
    moveServo(140);
  }
}

    pixel.show();
}

void moveServo(int position){
    position = constrain(position, 0, 180);
    myServo.attach(SERVO_PIN);
    myServo.write(position);
    delay(500);
    myServo.detach();
}


void fillLEDs(int ledColor, int startLED, int lastLED){
    for(int i=startLED; i<lastLED; i++){
        pixel.setPixelColor(i, ledColor);
    }
}

//Wait for new dust data and save it to the EEPROM
void getNewDustData(){
    int incomingDust;

    Adafruit_MQTT_Subscribe *subscription;
    while((subscription = mqtt.readSubscription(100))){
        if(subscription == &dustSub){
            incomingDust = strtol((char *)dustSub.lastread,NULL,10);
            Serial.printf("Int incoming dust: %i\n", incomingDust);
            totalDust = totalDust + incomingDust;
            EEPROM.put(totalDustAddress, totalDust);

            totalDustK = totalDust / 1000.0;    //divide by 1,000 for nicer visualization
            lastRXTime = millis();
            Serial.printf("%0.2fk Dust Particles\n", totalDustK);
            adaPublish();
        }
    }
}


//Flash onboard LED when new data comes in
void newDataLEDFlash(){
    if(millis() - lastRXTime <500){
        digitalWrite(7, HIGH);  
    } else{
        digitalWrite(7, LOW);
    }
}

//Publish to Adafruit.io - dust is divided by 1,000 for legibility
void adaPublish(){
  if(mqtt.Update()){
    dustPub.publish(totalDustK);
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care of connecting.
void MQTT_connect(){
    int8_t ret;

    // Return if already connected.
    if (mqtt.connected()){
        return;
    }

    Serial.print("Connecting to MQTT... ");

    while((ret = mqtt.connect()) != 0){
        Serial.printf("Error Code %s\n", mqtt.connectErrorString(ret));
        Serial.printf("Retrying MQTT connection in 5 seconds...\n");
        mqtt.disconnect();
        delay(5000);
    }
    Serial.printf("MQTT Connected!\n");
}

