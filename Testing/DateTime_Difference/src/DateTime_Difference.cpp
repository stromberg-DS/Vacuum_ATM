/* 
 * Date & Time Difference - get number of days/hours/minutes since last check
 * Author: Daniel Stromberg
 * Date: 4/25/24
*/

#include "Particle.h"
SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

//EEPROM setup
byte timeByte[4];
int len = EEPROM.length();
int timeByteAddress[4] = {0x0004, 0x0005, 0x0006, 0x0007};

uint32_t previousTime;
uint32_t currentTime;

time32_t now();
void timeToBytes(int timeIn, byte *byteZero, byte *byteOne, byte *byteTwo, byte *byteThree);

void setup() {
  Serial.begin(9600);
  waitFor(Serial.isConnected, 10000);
  for (int i=0; i<4; i++){
    timeByte[i] = EEPROM.read(timeByteAddress[i]);
    Serial.printf("timeByte[%i] = %X\n", i, timeByte[i]);
  }
  previousTime = (timeByte[0]<<24) | (timeByte[1]<<16) | (timeByte[2]<<8) | timeByte[3];
  Serial.printf("\nPreviousTime = %i\n\n", previousTime);
  Serial.printf("Waiting to connect to Particle cloud.");
  while(!Particle.connected()){
    Serial.printf(".");
  }
}

void loop() {
  uint32_t timeDifference;
  currentTime = Time.now();
  timeToBytes(currentTime, &timeByte[0], &timeByte[1], &timeByte[2], &timeByte[3]);
  for(int i=0; i<4; i++){
    EEPROM.write(timeByteAddress[i], timeByte[i]);
    // Serial.printf("Writing byte#%i: %02X\n", i, timeByte[i]);
  }
  timeDifference = currentTime - previousTime;
  Serial.printf("Current Time: %i\n", currentTime);
  Serial.printf("Previous Time: %i\n\n", previousTime);

  Serial.printf("Time difference: %i seconds\n", timeDifference);
  Serial.printf("Time difference: %f minutes\n\n", float(timeDifference/60.0));
  delay(1000);

}

//Breaks up 32 bit time value into 4 bytes for EEPROM
//Can I put the 4 bytes in an array???
void timeToBytes(int timeIn, byte *byteZero, byte *byteOne, byte *byteTwo, byte *byteThree){
  *byteZero = timeIn>>24;
  *byteOne = (timeIn>>16) & 0xFF;
  *byteTwo = (timeIn>>8) & 0xFF;
  *byteThree = timeIn & 0xFF;

}