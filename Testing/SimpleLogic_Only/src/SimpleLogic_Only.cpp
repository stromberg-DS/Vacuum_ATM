/* 
 * Simple Vacuum Logic
 * Author: Daniel Stromberg
 * Date: 4/26/24
*/

#include "Particle.h"
#include <neopixel.h>
#include "Button_DS.h"

SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

const int PIXEL_COUNT = 14;
const int RED = 0xFF0000;     //not ready to vacuum
const int BLUE = 0x0000FF;    //successfully vacuumed!
const int GREEN = 0x00FF00;   //ready to vacuum
const int YELLOW = 0xFFFF00;  //currently vacuuming
const int MAGENTA = 0xFF00FF; //need more vacuuming
const int WHITE = 255;        //Take a reward!

int dustSim = 0;
int timeSim = 0;
int vacStart = 0;
int vacTotalTime = 0;
int prevVacTime = 0;

void counterTimer();

Adafruit_NeoPixel pixel(PIXEL_COUNT, SPI1, WS2812);
Button vacuum(A2);


void setup() {
  Serial.begin(9600);

  pixel.begin();
  pixel.setBrightness(30);
  pixel.clear();
  pixel.show();
  pixel.setPixelColor(0, 255);
  delay(1000);
  pixel.clear();
  pixel.show();
  
}

void loop() {

  counterTimer();

  ////Treat button like real vacuum
  ////  pressed = vacuum on charger
  ////  released = taking off charger
  ////  not pressed = vacuuming
  ////  clicked = putting back on charger


  if((dustSim > 10000) || (timeSim>30000)){   //If house is dirty or it's been too long
    pixel.setPixelColor(0, GREEN);

    if(vacuum.isReleased()){      //When vacuum removed
      vacStart = millis();        //set the start vacuum timer
    }

    if(!vacuum.isPressed()){        //if you are vacuuming
      vacTotalTime = prevVacTime+ (millis() - vacStart);
      if(vacTotalTime > 4000){
        pixel.setPixelColor(0, BLUE);
      } else{
        pixel.setPixelColor(0, YELLOW);
      }
    }

    if(vacuum.isClicked()){         //if vacuum returned
      if(vacTotalTime > 4000){
        dustSim =0;
        timeSim = 0;
        vacTotalTime =0;
        prevVacTime = 0;
      }else{
        pixel.setPixelColor(0, GREEN);
        prevVacTime = vacTotalTime;
      }
    }


  }else{
    pixel.setPixelColor(0, RED);
  }





  pixel.show(); 
//Pseudo pseudo code

  //Is the dust above the threshold?
      //yes - turn LEDs green, ready to vacuum
          //Has the vacuum been removed?
              //YES - track time since removal, display yellow
              //NO - continue to show ready (green)

      //no 
            //Has it been long enough to vacuum anyways?
                //yes - turn LEDs green, ready to vacuum
                //no - LEDs red, not ready to vacuum
  
  //

  
}


void counterTimer(){
  static int lastUpdate = 0;
  static int countTime = 0;
  
  //
  if(millis()-lastUpdate > 500){
    Serial.printf("Dust: %i\nTime: %i\nTotal Vac time: %i\n\n", dustSim, timeSim,vacTotalTime);
    lastUpdate = millis();
  }

  if(millis()-countTime > 100){
    dustSim++;
    timeSim+= 1.25;
    countTime =0;
  }
}