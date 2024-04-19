/* 
 * Enroll Test
 * Author: Your Name
 * Date: 
 * For comprehensive documentation and examples, please visit:
 * https://docs.particle.io/firmware/best-practices/firmware-template/
 */

// Include Particle Device OS APIs
#include "Particle.h"
#include "Adafruit_Fingerprint.h"

uint8_t getFingerprintEnroll(uint8_t id);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial1);

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

void setup() {
  Serial.begin(9600);

  Serial.printf("fingertest\n");
  finger.begin(57600);
  
  delay(3000);
  if (finger.verifyPassword()){
    Serial.printf("found sensor!\n");
  } else{
    Serial.printf("Did NOT find sensor.\n");
  }
}

void loop() {
  Serial.printf("Type the ID # that you want for this finger\n");
  uint8_t id = 0;

  while (true){
    while(! Serial.available());
    Particle.process();

    char c = Serial.read();
    if (! isdigit(c)) break;
    id *=10;
    id +=c -'0';
  }
  Serial.printf("Enrolling ID #%i\n", id);
}
