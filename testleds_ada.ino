#include <SPI.h>
#include <Adafruit_WS2801.h>

#define NUM_LEDS 64

uint8_t *buffer; //64 byte serial buffer

enum controlSet{
  CMDSTART = 255,    //commands start with this byte
  //CMDEND = 128,      //commands end with this byte
  LOADFRAME = 1,     //next 2 bytes are uint16_t for starting pixel followed by 
                     //48 bytes of pixel RGB data (16 pixels)
  LOADLED = 2,        //next 2 bytes are uint16_t for starting pixel followed by
                      //3 bytes of pixel RGB data (1 pixel)
  CLEARFRAME = 10,    //set all pixel data to black
  TESTSEQ = 20        //fire the startup test sequence, all incoming serial data during 
                      //sequence will be discarded
};

uint8_t commandMode = 0;
//if 0, no command
//otherwise value equals controlSet value

uint32_t timeStamp = 0,   //used to space out matrix refreshes
        cmdFrameStamp = 0;  //watchdog to kill serial buffer fills if they take too long
uint16_t LEDStartPointer = 0, LEDCurrentPointer = 0, LEDEndPointer = 0;
uint16_t r_i, r_j;  //rainbow function globals
boolean sizeRead = false;
boolean statLED = false;
uint8_t statLEDPin = 12;  
uint8_t mhv_gammaValues[256];
uint8_t ECCByte = 0;
Adafruit_WS2801 strip = Adafruit_WS2801(NUM_LEDS);

#define PIN 2

void setup()
{
  for(uint16_t rep=0;rep<256;rep++)
  {
    mhv_gammaValues[rep] = mhv_calculatedGammaCorrect(rep);
  }
  
  Serial.begin(115200);
  
  Serial.println("Serial on");
  
  pinMode(statLEDPin, OUTPUT);
  digitalWrite(statLEDPin, HIGH);
  
  strip.begin();
  Serial.println("strip initialized");
  strip.show();
  Serial.println("strip activated");
  
  checkLEDs();
  
  Serial.println("starup done");
  
  timeStamp = millis();
}

void loop() 
{ 
  uint8_t tempByte = 0;
  //Serial.println("loop");
  //statLED = !statLED;
  statLED = false;
  digitalWrite(statLEDPin, statLED);  
  
  //if buffer data does not start with a command byte and 
  //no pending command, flush the byte
  if(commandMode == 0 && Serial.available() && Serial.peek() != CMDSTART) 
  {  Serial.read(); }
  
  //start of a new command frame if there is command byte 
  //and at least one more byte in buffer
  if(Serial.available() > 1 && Serial.peek() == CMDSTART)
  { 
    Serial.read(); //flush the command byte
    commandMode = Serial.read();
    switch(commandMode)
    {
      case 1:
        cmdFrameStamp = millis();
        commandMode = 1;
        sizeRead = false;  //LED start pointer not yet set
      break;
      case 2:
        cmdFrameStamp = millis();
        commandMode = 2;
        sizeRead = false;  //LED start pointer not yet set
      break;
      case 10:
        for(int rep=0;rep<NUM_LEDS;rep++)
        {
          strip.setPixelColor(rep, 0);
        }
        //memset(leds, 0, NUM_LEDS * 3); //set all LEDs to 0 intensity
        commandMode = 0;
      break;
      case 20:
        checkLEDs();
        commandMode = 0;
      break;
      default:  //unrecognized command type
        commandMode = 0;
      break;
    }
  } 
    
  //LED update is started, LED start pointer not set but info received
  if(sizeRead == false && (commandMode == 1 || commandMode == 2) && Serial.available() > 1)
  {
    tempByte = Serial.read(); //unless panel is > 256 pixels, this should be 0
    LEDCurrentPointer = /*(tempByte << 8) + */Serial.read();
    if(commandMode == 2)  //expect 3 bytes of color data or one CRBG struct
      { LEDEndPointer = LEDCurrentPointer; }
    if(commandMode == 1)  //expect 48 bytes of color data or 16 CRBG structs
      { LEDEndPointer = LEDCurrentPointer + 15; }
    sizeRead = true;
    //Serial.write(byte(LEDCurrentPointer));
    //Serial.write(byte(LEDEndPointer));
    //Serial.write(255);
  }
  
  //if write pointer goes beyond the end of the LED array or has reached the expected
  //stop point or the entire update operation has exceeded 100ms, stop the write
  if(LEDCurrentPointer > NUM_LEDS ||
    LEDCurrentPointer > LEDEndPointer ||
    millis() - cmdFrameStamp > 100)
  { 
    sizeRead = false;
    commandMode = 0;
  }
  
  //consume incoming bytes for LED updates 3 bytes at a time
  if(sizeRead == true && Serial.available() > 2)
  {
    //uint32_t color = 0;
    //color = Serial.read();
    //color <<= 8;
    //color |= Serial.read();
    //color <<= 8;
    //color |= Serial.read();
    uint8_t r = Serial.read(); //get R
    uint8_t g = Serial.read(); //get G
    uint8_t b = Serial.read();
    //strip.setPixelColor(LEDCurrentPointer, ((r<<16) + (g<<8) + b));
    //strip.setPixelColor(LEDCurrentPointer, r, g, b);
    strip.setPixelColor(LEDCurrentPointer, corColor(r, g, b));
    //*(leds + LEDCurrentPointer) = tempCRBG;
    LEDCurrentPointer++;
    //Serial.write(byte(LEDCurrentPointer));
    //Serial.write(byte(LEDEndPointer));
  }
  
  if((millis() - timeStamp) > 3) //wait at least 5 ms between matrix refreshes
  {
    strip.show();
    //FastSPI_LED.show();    //this is redundant(?) with lpd6803 chips
    timeStamp = millis();
  }
}

/* Helper functions */

void checkLEDs()
{
  //rainbowCycle(5);
  rainbowCycle(1);
  //turn LEDs off
  for(int rep=0;rep<NUM_LEDS;rep++)
  {
    strip.setPixelColor(rep, 0);
  }
}

void rainbowCycle(uint8_t wait) {
  //unsigned int i, j;
  
  for (r_j=0; r_j < 256 * 5; r_j++) {     // 5 cycles of all 25 colors in the wheel
    for (r_i=0; r_i < strip.numPixels(); r_i++) {
      // tricky math! we use each pixel as a fraction of the full 96-color wheel
      // (thats the i / strip.numPixels() part)
      // Then add in j which makes the colors go around per pixel
      // the % 96 is to make the wheel cycle around
      strip.setPixelColor(r_i, Wheel( ((r_i * 256 / strip.numPixels()) + r_j) % 256) );
    }  
    strip.show();   // write all the pixels out
    delay(wait);
  }
}

// Create a 24 bit color value from R,G,B
uint32_t Color(byte r, byte g, byte b)
{
  uint32_t c;
  c = r;
  c <<= 8;
  c |= g;
  c <<= 8;
  c |= b;
  return c;
}

//Create Gamma corrected color value from RGB
uint32_t corColor(uint8_t r, uint8_t g, uint8_t b)
{
  uint32_t c;
  c = mhv_precalculatedGammaCorrect(r);
  c <<= 8;
  c |= mhv_precalculatedGammaCorrect(g);
  c <<= 8;
  c |= mhv_precalculatedGammaCorrect(b);
  return c;
}

//Input a value 0 to 255 to get a color value.
//The colours are a transition r - g -b - back to r
uint32_t Wheel(byte WheelPos)
{
  if (WheelPos < 85) {
   return corColor(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if (WheelPos < 170) {
   WheelPos -= 85;
   return corColor(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170; 
   return corColor(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

uint8_t mhv_calculatedGammaCorrect(uint8_t value) 
{
  return (uint8_t) round(255 * pow((float)value / 256, 3.0));
}

inline uint8_t mhv_precalculatedGammaCorrect(uint8_t value) {
           //return pgm_read_byte(mhv_gammaValues + value);
           return mhv_gammaValues[value];
}
