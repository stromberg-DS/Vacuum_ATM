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

const int PIXEL_COUNT = 33;
const int RING_PIXEL_MIN = 1;   //first pixel to light on ring
const int RING_PIXEL_MAX = 15;
const int STRIP_PIXEL_MIN = 16; //first pixel on strip
const int STRIP_PIXEL_MAX = 33; //last pixel on strip
const int PUBLISH_TIME = 30000;
const int RED = 0xFF0000;     //not ready to vacuum
const int REDDISH_RING = 0x991100;
const int REDDISH_STRIP = 0xFF2200;
const int YELLOWISH_RING = 0x553300;
const int GREENISH_RING = 0X15fe09;
const int BLUE = 0x0000FF;    //successfully vacuumed!
const int GREEN = 0x00FF00;   //ready to vacuum
const int YELLOW = 0xFFFF00;  //currently vacuuming
const int MAGENTA = 0xFF00FF; //need more vacuuming
const int WHITE = 0xFFFFFF;        //Take a reward!
const int BASELINE_BRIGHTNESS = 50;
const int SERVO_PIN = A5;
const int SERVO_CLOSED = 140; //door is closed
const int SERVO_OPEN = 10;
const int CAM_PIN = D3;     //changed from D18 - my PCB is weird and D18 is connected to A5. BAD!
const int VAC_PIN = A2;

//Vacuum States
int vacuumState;
int lastVacuumState;
const int CHARGING_NOT_DIRTY = 0; //red
const int CHARGING_YES_DIRTY = 1; //green
const int NOW_VACUUM_NO_REWARD = 2; //yellow
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
bool isVacCharging;
bool lastVacChargeState;
unsigned int timeSinceVacuumed;
bool isVacReturned = 0;
bool isVacRemoved = 0;


const int VACUUMING_TIME = 600000;   //900,000 is 15 min, 600,000 = 10 min
const int MAX_DUST = 3000000;       //3,500,000 - roughly 1 week @500 particles/15 min 
const int MAX_TIME_SINCE_VAC = 1209600;            //time in seconds - 1,209,600sec = 14 days

//EEPROM Setup
int len = EEPROM.length();
int totalDustAddress = 0x0001; //4 bytes from 0x0001 to 0x0004
int timeAddress = 0x0010;   //

bool isLEDOn = false;
unsigned int totalDust = 0; //4 bytes - 
float totalDustK = 0;
int lastRXTime = 0;
int ringLEDDustLevel = 0;
int ringVacTimeLevel = 0;

//Time
unsigned int previousUnixTime;
unsigned int currentUnixTime;
unsigned int incomingStateChangeTime;
time32_t now();

//Functions
void MQTT_connect();
bool MQTT_ping();
void getNewDustData();
void adaPublish();
void dustToBytes(int dustIn, byte *dustHOut, byte *dustMOut, byte *dustLOut);
void newDataLEDFlash();
void fillLEDs(int ledColor, int startLED=0, int lastLED=PIXEL_COUNT);
void breatheLEDs(int ledColor, int startLED=0, int lastLED=PIXEL_COUNT);
void checkLEDs(int ledColor, int startLED, int lastLED);
void moveServo(int position);
void periodicPrint();

TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe dustSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/plantinfo.dustsensor");
Adafruit_MQTT_Subscribe vacInfoSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/vacuumstatus");
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/totaldust");

// Timer publishTimer(PUBLISH_TIME, adaPublish);

Adafruit_NeoPixel pixel(PIXEL_COUNT, SPI1, WS2812);
Servo myServo;
Button vacButton(VAC_PIN);
Button camButton(CAM_PIN);
IoTTimer flashTimer;
IoTTimer servoTimer;

void setup() {
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    Watchdog.init(WatchdogConfiguration().timeout(600s));     //Set watchdog timer to 5 min
    Watchdog.start();                                         //Start watchdog timer

    myServo.attach(SERVO_PIN);
    moveServo(SERVO_CLOSED);
    pixel.begin();
    pixel.setBrightness(BASELINE_BRIGHTNESS);

    checkLEDs(0x000099, RING_PIXEL_MAX, RING_PIXEL_MIN);
    checkLEDs(0, RING_PIXEL_MAX, RING_PIXEL_MIN);

    checkLEDs(0xFF0000, STRIP_PIXEL_MAX, STRIP_PIXEL_MIN);
    checkLEDs(0, STRIP_PIXEL_MAX, STRIP_PIXEL_MIN);
    pixel.clear();
    pixel.show();


    Serial.printf("Connecting to Particle cloud...");
    while(!Particle.connected()){
        //wait to connect to particle cloud
    }

    previousUnixTime = EEPROM.get(timeAddress, previousUnixTime);
    Serial.printf("PreviousTime: %u\n\n", previousUnixTime);


    totalDust = EEPROM.get(totalDustAddress, totalDust);
    ringLEDDustLevel = map(totalDust, 0, MAX_DUST, RING_PIXEL_MAX, RING_PIXEL_MIN);
    ringLEDDustLevel = constrain(ringLEDDustLevel, RING_PIXEL_MIN, RING_PIXEL_MAX);
    fillLEDs(0x330000, RING_PIXEL_MIN, ringLEDDustLevel);
    pixel.show();
    // publishTimer.start();

    mqtt.subscribe(&dustSub);
    mqtt.subscribe(&vacInfoSub);

    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);

}

void loop() {
  Watchdog.refresh();
    MQTT_connect();
    MQTT_ping();

    currentUnixTime = Time.now();
    timeSinceVacuumed = currentUnixTime - previousUnixTime;

    if(vacuumState != lastVacuumState){
        Serial.printf("New Vacuum State:\n  %s\n\n", VAC_STATE_STRING[vacuumState].c_str());
        lastVacStateTime = millis();    //track when state changes
        lastVacuumState = vacuumState;
    }

    // periodicPrint();
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
    ringVacTimeLevel = map(elapsedVacTime, 0, VACUUMING_TIME, RING_PIXEL_MAX, RING_PIXEL_MIN);
    ringVacTimeLevel = constrain(ringVacTimeLevel, RING_PIXEL_MIN, RING_PIXEL_MAX);
    fillLEDs(REDDISH_RING, RING_PIXEL_MIN, RING_PIXEL_MAX);
    fillLEDs(GREENISH_RING, ringVacTimeLevel, RING_PIXEL_MAX);
    fillLEDs(REDDISH_STRIP, STRIP_PIXEL_MIN, STRIP_PIXEL_MAX);

    // fillLEDs(REDDISH_RING, RING_PIXEL_MIN, RING_PIXEL_MAX);
    // fillLEDs(REDDISH_STRIP, STRIP_PIXEL_MIN, STRIP_PIXEL_MAX);
    vacuumState = CHARGING_YES_DIRTY;


    if(isVacRemoved){      //When vacuum removed
      vacStartTime = millis();        //set the start vacuum timer 
      isVacRemoved = false;
    }
    //if you are vacuuming
    if(!isVacCharging){
      elapsedVacTime = prevVacTime+ (millis() - vacStartTime);
      // ringVacTimeLevel = map(elapsedVacTime, 0, VACUUMING_TIME, RING_PIXEL_MAX, RING_PIXEL_MIN);
      // ringVacTimeLevel = constrain(ringVacTimeLevel, RING_PIXEL_MIN, RING_PIXEL_MAX);
      if(elapsedVacTime > VACUUMING_TIME){  //check if you have vacuumed long enough
        fillLEDs(GREENISH_RING, RING_PIXEL_MIN, RING_PIXEL_MAX);
        fillLEDs(0, STRIP_PIXEL_MIN, STRIP_PIXEL_MAX);
        vacuumState = NOW_VACUUM_REWARD_READY;
      } else{
        // fillLEDs(REDDISH_RING, RING_PIXEL_MIN, RING_PIXEL_MAX);
        // fillLEDs(0x553300, ringVacTimeLevel, RING_PIXEL_MAX);
        // fillLEDs(REDDISH_STRIP, STRIP_PIXEL_MIN, STRIP_PIXEL_MAX);
        vacuumState = NOW_VACUUM_NO_REWARD;
      }
    }
    //If the vacuum is returned
    if(isVacReturned){         
      isVacReturned = false;
      if(elapsedVacTime > VACUUMING_TIME){  //check if you have vacuumed enough
        totalDust =0;
        totalDustK = 0;
        timeSinceVacuumed = 0;
        elapsedVacTime =0;
        prevVacTime = 0;
        previousUnixTime = currentUnixTime;
        EEPROM.put(timeAddress, currentUnixTime);   //log last vacuum time in EEPROM
        vacuumState = FINISHED_TAKE_REWARD;
        isReadyToDispense = true;
      }else{
        isReadyToDispense = false;
        prevVacTime = elapsedVacTime;
        vacuumState = STOPPED_EARLY;
        flashTimer.startTimer(2000);
      }
    }
  }else{    //If the house is not dirty enough

    pixel.clear();
    fillLEDs(0x553300, RING_PIXEL_MIN, RING_PIXEL_MAX);
    fillLEDs(REDDISH_RING, ringLEDDustLevel, RING_PIXEL_MAX);
    vacuumState = CHARGING_NOT_DIRTY;
  }

if(isReadyToDispense){
  fillLEDs(0x443322, RING_PIXEL_MIN, RING_PIXEL_MAX);
  fillLEDs(0, STRIP_PIXEL_MIN, STRIP_PIXEL_MAX);
  if(camButton.isClicked()){
    moveServo(SERVO_OPEN);
    Serial.printf("Door opening - cam clicked\n");
  } else if (camButton.isReleased()){
    isReadyToDispense = false;
    moveServo(SERVO_CLOSED);
    ringLEDDustLevel = RING_PIXEL_MAX;
    pixel.clear();
  }
}

    pixel.show();
}

void moveServo(int position){
    position = constrain(position, 0, 180);
    myServo.write(position);
    delay(500);
    // Serial.printf("!!!!!!!MOVING SERVO: %i!!!!!!!\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n", position);
}

void periodicPrint(){
  static int lastPrintTime;

  if(millis()-lastPrintTime > 1000){
        // Serial.printf("Dust: %i\nTime: %i\nTotal Vac time: %i\n", totalDust, timeSinceVacuumed,elapsedVacTime);
        Serial.printf("CamButton: %i\n\n", camButton.isPressed());
        lastPrintTime = millis();
    }
}

void fillLEDs(int ledColor, int startLED, int lastLED){
  if(startLED < lastLED){
    for(int i=startLED; i<lastLED; i++){
        pixel.setPixelColor(i, ledColor);
    }
  }
}

void breatheLEDs(int ledColor, int startLED, int lastLED){

}

void checkLEDs(int ledColor, int startLED, int lastLED){
  for(int i=startLED; i>lastLED; i--){
      // pixel.clear();
      pixel.setPixelColor(i, ledColor);
      pixel.show();
      delay(50);
    }
}

//Wait for new dust data and save it to the EEPROM
void getNewDustData(){
    int incomingDust;
    String incomingVacInfo;

    Adafruit_MQTT_Subscribe *subscription;
    while((subscription = mqtt.readSubscription(100))){
        if(subscription == &dustSub){
            incomingDust = strtol((char *)dustSub.lastread,NULL,10);
            Serial.printf("Int incoming dust: %i\n", incomingDust);
            totalDust = totalDust + incomingDust;
            EEPROM.put(totalDustAddress, totalDust);

            ringLEDDustLevel = map(totalDust, 0, MAX_DUST, RING_PIXEL_MAX, RING_PIXEL_MIN);
            ringLEDDustLevel = constrain(ringLEDDustLevel, RING_PIXEL_MIN, RING_PIXEL_MAX);
            Serial.printf("Ring LED #%i\n\n", ringLEDDustLevel);
            totalDustK = totalDust / 1000.0;    //divide by 1,000 for nicer visualization
            lastRXTime = millis();
            Serial.printf("%0.2fk Total Dust Particles\n\n", totalDustK);
            adaPublish();
        } else if (subscription == &vacInfoSub){
            lastRXTime = millis();
            incomingStateChangeTime = Time.now();
            incomingVacInfo = (char *)vacInfoSub.lastread;
            isVacCharging = atoi(incomingVacInfo);
            
            //VacStatus Photon only sends chargin/not charging on state change
            //  so we can assume the below are the rising/falling edge of state change
            isVacReturned = isVacCharging;
            isVacRemoved = !isVacCharging;

            Serial.printf("### vac info incoming ###\n");
            Serial.printf("isVacCharging: %i\n", isVacCharging);
            Serial.printf("Last Vac state change time: %u\n\n", incomingStateChangeTime);
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

//Keeps the connection open to Adafruit
bool MQTT_ping() {
    static unsigned int last;
    bool pingStatus;

    if ((millis()-last)>120000) {
        Serial.printf("Pinging MQTT \n");
        pingStatus = mqtt.ping();
        if(!pingStatus) {
        Serial.printf("Disconnecting \n");
        mqtt.disconnect();
        }
        last = millis();
    }
    return pingStatus;
}