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

bool isLEDOn = false;
long totalDust = 0;
float totalDustK = 0;
int lastRXTime = 0;

//Functions
void MQTT_connect();
bool MQTT_ping();
long getNewDust();
void adaPublish();

TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe dustSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/plantinfo.dustsensor");
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/totaldust");
Timer publishTimer(PUBLISH_TIME, adaPublish);



void setup() {
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    publishTimer.start();

    mqtt.subscribe(&dustSub);
    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);

}

void loop() {
    MQTT_connect();
    MQTT_ping();

    int incomingDust;


    Adafruit_MQTT_Subscribe *subscription;
    while((subscription = mqtt.readSubscription(100))){
        if(subscription == &dustSub){
            Serial.printf("Raw dust info recieved: %s\n", (char *)dustSub.lastread);
            incomingDust = strtol((char *)dustSub.lastread,NULL,10);
            Serial.printf("Int incoming dust: %i\n", incomingDust);
            totalDust = totalDust + incomingDust;
            totalDustK = totalDust / 1000.0;
            lastRXTime = millis();
            Serial.printf("%0.1fk Dust Particles\n", totalDustK);
        }
    }
    
    if(millis() - lastRXTime <500){
        digitalWrite(7, HIGH);  
    } else{
        digitalWrite(7, LOW);
    }
}

void adaPublish(){
  if(mqtt.Update()){
    dustPub.publish(totalDustK);
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
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

bool MQTT_ping(){
    static unsigned int last;
    bool pingStatus;

    if ((millis()-last)>120000){
        Serial.printf("Pinging MQTT \n");
        pingStatus = mqtt.ping();
        if(!pingStatus){
            Serial.printf("Disconnecting \n");
            mqtt.disconnect();
        }
        last = millis();
    }
    return pingStatus;
}
