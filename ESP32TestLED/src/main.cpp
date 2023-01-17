#include <Arduino.h>
#include "FastLED.h"
#define NUM_LEDS 10
CRGB leds[NUM_LEDS];
void setup() 
{ 
  FastLED.addLeds<NEOPIXEL, 16>(leds, NUM_LEDS); 

  for (int i=0; i < NUM_LEDS; i++)
    leds[i] = CRGB::Yellow; FastLED.show();
  delay(2000);
  for (int i=0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::Black; FastLED.show();
    delay(500);
  }

}

int i=0;
void loop() {
		//leds[i] = CRGB::Yellow; FastLED.show(); delay(30);
		//leds[i] = CRGB::Black; FastLED.show(); delay(80);
    i++;
    if (i> NUM_LEDS)
      i=0;
}