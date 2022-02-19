# 75x16-ws2812-leds-matrix

mostly teensy 4.1 

-display text and execute commands based on text file on SD, see documentation  
-display .ppm images stored on SD  
-several macros (background color, text color, fades, glitter ...)  
-midi implementation see documentation 

use ext lib:  
#include <Adafruit_GFX.h>  
#include <Adafruit_NeoMatrix.h>  
#include <Adafruit_NeoPixel.h>  
#include <SD.h>  
#include <SPI.h>  
#include <MIDI.h>  
