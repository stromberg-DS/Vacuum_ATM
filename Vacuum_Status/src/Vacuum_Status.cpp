/* 
 * Vacuum ATM - Vacuum Status Detector
 * Author: Daniel Stromberg
 * Date: 5/5/24
*/

#include "Particle.h"
#include "credentials.h"
#include "Button_DS.h"
#include "neopixel.h"
#include <Adafruit_MQTT.h>
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"


SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

const int RED_LED_PIN = D1;

bool isVacCharging;
bool lastVacState;
unsigned int stateChangeUnixTime;

//Functions
void MQTT_connect();
void adaPublish();

TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish vacStatus = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/vacuumstatus");


Button vacButton(A2);

void setup() {
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    pinMode(RED_LED_PIN, OUTPUT);
}

void loop() {
    MQTT_connect();
    isVacCharging = vacButton.isPressed();
    digitalWrite(RED_LED_PIN, isVacCharging);

    // Serial.printf("Current Vac Status: %i\nPrevious Status: %i\n\n", isVacCharging, lastVacState);

    if(isVacCharging != lastVacState){
        adaPublish();
        Serial.printf("Status changed! Send to adafruit!\nCharging: %i\nCurrent Time: %u\n\n", isVacCharging, stateChangeUnixTime);
        lastVacState = isVacCharging;
        stateChangeUnixTime = Time.now();
    }

    // delay(500);
}

//Publish to Adafruit.io
void adaPublish(){
  if(mqtt.Update()){
    vacStatus.publish(isVacCharging);
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