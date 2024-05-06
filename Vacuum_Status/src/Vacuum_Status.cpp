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
#include <math.h>


SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

const int RED_LED_PIN = D1;

bool isVacCharging;
bool lastVacState;
unsigned int stateChangeUnixTime;
String pubInfoString;
float t;
float ledBrightness;

//Functions
void MQTT_connect();
void adaPublish();

TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish vacStatus = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/vacuumstatus");


Button vacButton(A2);

void noUglyLEDs();
void lightRedLED();

void setup() {
    Serial.begin(9600);
    waitFor(Serial.isConnected, 10000);

    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(D7, OUTPUT);
    digitalWrite(D7, LOW);
}

void loop() {
    MQTT_connect();
    isVacCharging = vacButton.isPressed();

    lightRedLED();
    
    if(isVacCharging != lastVacState){
        Serial.printf("Status changed! Send to adafruit!\nCharging: %i\nCurrent Time: %u\n\n", isVacCharging, stateChangeUnixTime);
        lastVacState = isVacCharging;
        stateChangeUnixTime = Time.now();
        pubInfoString = String(isVacCharging)+String(stateChangeUnixTime);      //Add current vacuum state onto beginning of string
        Serial.printf("%s\n\n", pubInfoString.c_str());
        adaPublish();       //send string to adafruit - contains time when state changed and current state.
    }

    noUglyLEDs();
}

//Pulse red LED when vacuum is charging
void lightRedLED(){
    if(isVacCharging){
        t = millis()/1000.0 + 0.5 ;
        ledBrightness = 23 * sin(2*M_PI*t/4.0)+28;
        analogWrite(RED_LED_PIN, ledBrightness);
    }else{
        digitalWrite(RED_LED_PIN, 0);
    }
}

//turn off other onboard LEDs
void noUglyLEDs(){
    if((millis()>20000)&&(Particle.connected)){
        RGB.control(true);
        RGB.brightness(0);
    } else{
        RGB.control(false);
    }
}

//Publish to Adafruit.io
void adaPublish(){
  if(mqtt.Update()){
    vacStatus.publish(pubInfoString);
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