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
const int CHARGING_NOT_DIRTY = 0;
const int CHARGING_YES_DIRTY = 1;
const int NOW_VACUUM_NO_REWARD = 2;
const int NOW_VACUUM_REWARD_READY =3;
const int STOPPED_EARLY = 4;
const int FINISHED_TAKE_REWARD = 5;
int vacuumState;
int vacStartTime;
int elapsedVacTime=0;


const int VACUUMING_TIME = 5000;   //900,000 is 15 min
const int MAX_DUST = 1000;       //3,500,000 - roughly 1 week @500 particles/15 min 
const int MAX_TIME = 14;            //14 days

//EEPROM Setup
byte dustByteH, dustByteM, dustByteL;   //3 bytes to allow for ~16 million dust particles 
int len = EEPROM.length();
int dustHAddress = 0x0001;
int dustMAddress = 0x0002;
int dustLAdress = 0x0003;

bool isLEDOn = false;
long totalDust = 0;
float totalDustK = 0;
int lastRXTime = 0;

//Functions
void MQTT_connect();
bool MQTT_ping();
void getNewDustData();
void adaPublish();
void dustToBytes(int dustIn, byte *dustHOut, byte *dustMOut, byte *dustLOut);
void newDataLEDFlash();
void fillLEDs(int ledColor, int startLED=0, int lastLED=PIXEL_COUNT);

TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe dustSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/plantinfo.dustsensor");
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/totaldust");

Timer publishTimer(PUBLISH_TIME, adaPublish);

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

    dustByteH = EEPROM.read(dustHAddress);
    dustByteM = EEPROM.read(dustMAddress);
    dustByteL = EEPROM.read(dustLAdress);

    //recombine totalDust from EEPROM
    totalDust = (dustByteH<<16) | (dustByteM<<8) | dustByteL;
    Serial.printf("Last total dust amount: %06X\n\n", totalDust);
    Serial.printf("Last Dust Levels:\nHigh: 0x%02X\nMed: 0x%02X\nLow: 0x%02X\n\n", dustByteH, dustByteM, dustByteL);

    if(totalDust>MAX_DUST){
        vacuumState = CHARGING_YES_DIRTY;
    } else{
        vacuumState = CHARGING_NOT_DIRTY;
    }

    publishTimer.start();

    mqtt.subscribe(&dustSub);

    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);

}

void loop() {
    static bool isFirstVacuumEdge = true;
    MQTT_connect();
    // MQTT_ping();

    getNewDustData();
    newDataLEDFlash();

    switch (vacuumState){
        case CHARGING_NOT_DIRTY:
            fillLEDs(0x000000);
            break;

        case CHARGING_YES_DIRTY:
            fillLEDs(0x00FF00);
            break;

        case NOW_VACUUM_NO_REWARD:
            fillLEDs(0xFFFF00);
            break;

        case NOW_VACUUM_REWARD_READY:
            fillLEDs(0x00FFFF);
            break;

        case STOPPED_EARLY:
            if(flashTimer.isFinished()){
                pixel.clear();
                pixel.show();
                vacuumState = CHARGING_YES_DIRTY;
            } else{
                fillLEDs(0xFF0000);
            }
            break;

        case FINISHED_TAKE_REWARD:
            break;

    }


    //Has the vacuum been returned to the charger?
    if(vacButton.isReleased()){
        elapsedVacTime = elapsedVacTime + (millis()-vacStartTime);
        Serial.printf("Releaseeeed!\nElapsed Time: %i\n\n", elapsedVacTime);
        if(elapsedVacTime > 5000){
            vacuumState = NOW_VACUUM_REWARD_READY;
        } else{
            vacuumState = STOPPED_EARLY;
            flashTimer.startTimer(2000);
        }
    }

    //Has the vacuum been taken off the charger?
    if(vacButton.isPressed()){
        if(isFirstVacuumEdge){
            vacStartTime = millis();
            isFirstVacuumEdge = false;
            vacuumState = NOW_VACUUM_NO_REWARD;
        }
    } else{
        isFirstVacuumEdge = true;
    }

    // if(vacButton.isClicked()){
    //     vacStartTime = millis();
    //     fillLEDs(0xFFFF00);
        // Serial.printf("Currently Vacuuming!\n");
    //     Serial.printf("vacStartTime: %i\n\n", vacStartTime);
    //     vacuumState = NOW_VACUUM_NO_REWARD;
    // }

    //////Need to pause or subtract any time after vacuum is returned
    //////  Don't want time on the charger to count towards vacuuming time.
    //////Can't get isClicked and isReleased to both work. Only the first one works.
    // if(vacButton.isReleased()){
    //     Serial.printf("Vacuum Returned!\n");
        // if((millis()-vacStartTime) > VACUUMING_TIME){
        //     totalDust = 0;
        //     Serial.printf("You vacuumed for long enough. Have a reward.");
        // } else{
        //     Serial.printf("Keep vacuuming...");
        // }
    // }

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

            //Break totalDust into bytes and save into EEPROM
            dustToBytes(totalDust, &dustByteH, &dustByteM, &dustByteL);
            EEPROM.write(dustHAddress, dustByteH);
            EEPROM.write(dustMAddress, dustByteM);
            EEPROM.write(dustLAdress, dustByteL);

            totalDustK = totalDust / 1000.0;    //divide by 1,000 for nicer visualization
            lastRXTime = millis();
            Serial.printf("%0.2fk Dust Particles\n", totalDustK);
        }
    }
}

//Break up total Dust into 3 bytes for EEPROM storage
void dustToBytes(int dustIn, byte *dustHOut, byte *dustMOut, byte *dustLOut){
    *dustHOut = dustIn>>16;
    *dustMOut = (dustIn>>8) & 0xFF;
    *dustLOut = dustIn & 0xFF;
}

//Flash onboard LED when new data comes in
void newDataLEDFlash(){
    if(millis() - lastRXTime <500){
        digitalWrite(7, HIGH);  
    } else{
        digitalWrite(7, LOW);
    }
}

//Publish to Adafruit.io
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

// bool MQTT_ping(){
//     static unsigned int last;
//     bool pingStatus;

//     if ((millis()-last)>120000){
//         Serial.printf("Pinging MQTT \n");
//         pingStatus = mqtt.ping();
//         if(!pingStatus){
//             Serial.printf("Disconnecting \n");
//             mqtt.disconnect();
//         }
//         last = millis();
//     }
//     return pingStatus;
// }
