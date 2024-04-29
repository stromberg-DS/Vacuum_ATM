/* 
 * Servo Test: Vacuum ATM
 * Author: Daniel Stromberg
 * Date: 4/28/24
*/

#include "Particle.h"

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

const int SERVO_PIN = A5;

Servo myServo;


void setup() {
  Serial.begin(9600);
  myServo.attach(SERVO_PIN);
}

void loop() {
  myServo.write(10);     //pushing coin out
  delay(1000);
  myServo.write(140);   //retract and wait
  delay(500);
  myServo.detach();     //detach for less servo noise.
  delay(6000);
  myServo.attach(SERVO_PIN);
}
 