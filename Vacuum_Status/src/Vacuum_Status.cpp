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
bool MQTT_ping();

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

    Watchdog.init(WatchdogConfiguration().timeout(600s));   //Set watchdog timer to 5 min
    Watchdog.start();
}

void loop() {
    Watchdog.refresh();     //Watchdog timer checks in every loop
    MQTT_connect();
    MQTT_ping();
    isVacCharging = vacButton.isPressed();

    lightRedLED();
    
    if(isVacCharging != lastVacState){
        Serial.printf("Status changed! Send to adafruit!\nCharging: %i\nCurrent Time: %u\n\n", isVacCharging, stateChangeUnixTime);
        lastVacState = isVacCharging;

        // I was planning to send vacuum status and time together, but changed my mind
        // 
        // stateChangeUnixTime = Time.now();
        // pubInfoString = String(isVacCharging)+String(stateChangeUnixTime);      //Add current vacuum state onto beginning of string
        // Serial.printf("%s\n\n", pubInfoString.c_str());

        adaPublish();       //send state to adafruit 
    }

    noUglyLEDs();  
}

//Pulse red LED when vacuum is charging
void lightRedLED(){
    if(isVacCharging){
        t = fmod((millis()/1000.0 + 0.5), 4.0) ;         //Keeps T between 0.0-4.0 to make calculation easier.
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