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

//Vacuum States
const int CHARGING_NOT_DIRTY = 0; //red
const int CHARGING_YES_DIRTY = 1; //green
const int NOW_VACUUM_NO_REWARD = 2; //yellos
const int NOW_VACUUM_REWARD_READY =3; //
const int STOPPED_EARLY = 4;
const int FINISHED_TAKE_REWARD = 5;
int vacuumState;
int vacStartTime;
int elapsedVacTime=0;
int lastVacStateTime;


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

TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe dustSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/plantinfo.dustsensor");
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/totaldust");

// Timer publishTimer(PUBLISH_TIME, adaPublish);

Adafruit_NeoPixel pixel(PIXEL_COUNT, SPI1, WS2812);

Button vacButton(A2);
IoTTimer flashTimer;

void setup() {
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    pixel.begin();
    pixel.setBrightness(20);
    fillLEDs(0x00FFFF);
    pixel.show();
    delay(1000);
    fillLEDs(0xFF0000);
    pixel.show();

    Serial.printf("Connecting to Particle cloud...");
    while(!Particle.connected()){
        //wait to connect to particle cloud
    }

    previousUnixTime = EEPROM.get(timeAddress, previousUnixTime);
    Serial.printf("PreviousTime: %u\n\n", previousUnixTime);


    totalDust = EEPROM.get(totalDustAddress, totalDust);
    if(totalDust>MAX_DUST){
        vacuumState = CHARGING_YES_DIRTY;
        lastVacStateTime = millis();
    } else{
        vacuumState = CHARGING_NOT_DIRTY;
        lastVacStateTime = millis();
    }

    // publishTimer.start();

    mqtt.subscribe(&dustSub);

    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);

}

void loop() {
    static int lastPrintTime;
    static bool isFirstVacuumEdge = true;
    MQTT_connect();

    unsigned int timeSinceVacuumed;
    currentUnixTime = Time.now();
    timeSinceVacuumed = currentUnixTime - previousUnixTime;

    if(millis()-lastPrintTime > 1000){
        Serial.printf("TotalDust: %0.2f x1000\n", totalDustK);
        Serial.printf("TimeSinceVacuumed: %u\n\n", timeSinceVacuumed);
        lastPrintTime = millis();
    }

    getNewDustData();
    newDataLEDFlash();


    /////////// CANT QUITE FIGURE OUT HOW TO GO TO DIRTY STATE W/O OVERRIDING OTHER STATES!
    if((totalDust>MAX_DUST) || (timeSinceVacuumed > MAX_TIME_SINCE_VAC)){
        // vacuumState = CHARGING_YES_DIRTY;
        lastVacStateTime = millis();

        //////////////////
        ///Testing a big nested if/////
        // if(vacButton.isReleased()){
        //     elapsedVacTime = elapsedVacTime + (millis()-vacStartTime);
        //     Serial.printf("Releaseeeed!\nElapsed Time: %i\n\n", elapsedVacTime);
        //     if(elapsedVacTime > 5000){
        //         totalDust = 0;
        //         elapsedVacTime = 0;
        //         previousUnixTime = currentUnixTime;
        //         EEPROM.put(timeAddress, currentUnixTime); //log vacuum time
        //         vacuumState = NOW_VACUUM_REWARD_READY;
        //         lastVacStateTime = millis();
        //     } else{
        //         vacuumState = STOPPED_EARLY;
        //         lastVacStateTime = millis();
        //         flashTimer.startTimer(2000);
        //     }
        // } else if(vacButton.isClicked()){   //When vacuum is removed
        //     if(isFirstVacuumEdge){
        //         vacStartTime = millis();
        //         isFirstVacuumEdge = false;
        //         vacuumState = NOW_VACUUM_NO_REWARD;
        //         lastVacStateTime = millis();
        //     }
        // } else{
        //     isFirstVacuumEdge = true;
        //     vacuumState = CHARGING_NOT_DIRTY;
        //     lastVacStateTime = millis();
        // }
        ////end of test////
        //////////////

        //Has the vacuum been returned to the charger?
        if(vacButton.isReleased()){
            elapsedVacTime = elapsedVacTime + (millis()-vacStartTime);
            Serial.printf("Releaseeeed!\nElapsed Time: %i\n\n", elapsedVacTime);
            if(elapsedVacTime > 5000){
                totalDust = 0;
                totalDustK = 0;
                elapsedVacTime = 0;
                previousUnixTime = currentUnixTime;
                EEPROM.put(timeAddress, currentUnixTime); //log vacuum time
                vacuumState = NOW_VACUUM_REWARD_READY;
                lastVacStateTime = millis();
            } else{
                vacuumState = STOPPED_EARLY;
                lastVacStateTime = millis();
                flashTimer.startTimer(2000);
            }
        } 

        
        if(vacButton.isPressed()){
            if(isFirstVacuumEdge){
                vacStartTime = millis();
                isFirstVacuumEdge = false;
                vacuumState = NOW_VACUUM_NO_REWARD;
                lastVacStateTime = millis();
            }
        } else{
            isFirstVacuumEdge = true;
            vacuumState = CHARGING_YES_DIRTY;
        }

    } else{
        vacuumState = CHARGING_NOT_DIRTY;
        lastVacStateTime = millis();
    }


///Switch Case///
    switch (vacuumState){
        case CHARGING_NOT_DIRTY:
            // Serial.printf("charging, not dirty\n");
            fillLEDs(0xFF0000);
            break;

        case CHARGING_YES_DIRTY:
            // Serial.printf("charging, yes dirty\n");
            fillLEDs(0x00FF00);
            break;

        case NOW_VACUUM_NO_REWARD:
            // Serial.printf("Now vacuuming, no reward\n");
            fillLEDs(0xFFFF00);
            break;

        case NOW_VACUUM_REWARD_READY:
            // Serial.printf("Now Vacuuming, reward ready\n");
            if(millis()-lastVacStateTime < 2000){
                fillLEDs(0x00FFFF);
            } else{
                vacuumState = FINISHED_TAKE_REWARD;
                lastVacStateTime = millis();
            }
            break;

        case STOPPED_EARLY:
            // Serial.printf("stopped early\n");
            if(millis()-lastVacStateTime<2000){
                ////////////Would be nice to make this blinking a function
                if((millis()-lastVacStateTime)%500<250){
                    fillLEDs(0xFF0000);
                } else{
                    fillLEDs(0x550000);
                }
            }else{
                fillLEDs(0);
                vacuumState = CHARGING_YES_DIRTY;
                lastVacStateTime = millis();
            }
            break;

        case FINISHED_TAKE_REWARD:
            // Serial.printf("Finished, take reward\n");
            if(millis()-lastVacStateTime <2000){
                fillLEDs(0xFFFFFF);
            } else{
                vacuumState = CHARGING_NOT_DIRTY;
                lastVacStateTime = millis();
            }
            break;

    }



}

void fillLEDs(int ledColor, int startLED, int lastLED){
    for(int i=startLED; i<lastLED; i++){
        pixel.setPixelColor(i, ledColor);
    }
    pixel.show();
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

