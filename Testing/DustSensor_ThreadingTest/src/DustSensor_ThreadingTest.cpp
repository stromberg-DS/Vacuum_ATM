/* 
 * Threaded Dust Sensor Test
 * Author: Daniel Stromberg
 * Date: 4/19/24
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

#include "Particle.h"
#include "math.h"
#include <Adafruit_MQTT_SPARK.h>
#include <Adafruit_MQTT.h>
#include "credentials.h"

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

const int DUST_PIN = 10;
const int LED_PIN = 7;
const int SAMPLE_TIME = 30000;
float lowPulseOccupancy;
float ratio;
float concentration;
int lastTime;
int updateTime = 1000;
int totalDust = 0;
bool isLedOn = false;

TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/plantinfo.dustsensor");

//functions
void getConc();
void adaPublish();

Timer publishTimer(SAMPLE_TIME, adaPublish);


void setup() {
  Serial.begin(9600);
  new Thread("concThread", getConc);

  publishTimer.start();

  pinMode(LED_PIN, OUTPUT);
  pinMode(DUST_PIN, INPUT);
}

void loop() {
  if((millis()-lastTime) > updateTime){
    Serial.printf("Time: %0.2f, Conc: %0.2f\n", millis()/1000.0, concentration);
    // Serial.printf("Ratio: %0.2f\n\n", ratio);
    isLedOn = !isLedOn;
    lastTime = millis();
  }

  digitalWrite(LED_PIN, isLedOn);
}

//
void getConc(){
  unsigned int duration, startTime;

  startTime = 0;
  lowPulseOccupancy = 0;

  while(true){
    duration = pulseIn(DUST_PIN, LOW);
    lowPulseOccupancy = lowPulseOccupancy + duration;
    if ((millis()-startTime) > SAMPLE_TIME){
      ratio = lowPulseOccupancy/(SAMPLE_TIME*10.0);
      concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62;
      startTime = millis();
      lowPulseOccupancy=0;
      totalDust = totalDust + concentration;
    }
  }
}

void adaPublish(){
  if(mqtt.Update()){
    dustPub.publish(totalDust);
  }
}