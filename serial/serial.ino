#include <Adafruit_NeoPixel.h>
#include <avr/power.h>

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN            6

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      256

// Serial port bitrate
#define BITRATE        115200

// Setial port state machine
// We are in empty packets
#define STATE_IDLE     0

// Something wrong happened, ignore everything until the first newline or ';' character
#define STATE_ERROR    1

// Question mark found
#define STATE_QM       2

// We are in receiving data ('!')
#define STATE_DATA     3

// State of serial receuvubg
uint8_t serialState;

// Next R/G/B index expected
uint8_t   rgbindex;

// Next LED address expected
uint8_t   nextled;

// Next digit index
uint8_t   nextdigit;

uint8_t rgb[3];



// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
#if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  // End of trinket special code

  pixels.begin(); // This initializes the NeoPixel library.
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, 0);
  }
  pixels.show();
  
  serialState = STATE_IDLE;
  Serial.begin(BITRATE);
}



void loop() {

  while (Serial.available()) {
    byte rcv = Serial.read();
    switch (serialState) {
      case STATE_IDLE:
        if (rcv == '?') {
          serialState = STATE_QM;
        } else if (rcv == '@') {
          serialState = STATE_DATA;
          rgbindex = 0;
          nextled = 0;
          nextdigit = 4;
          rgb[0] = 0;
          rgb[1] = 0;
          rgb[2] = 0;
        } else if (rcv != ' ' && rcv != '\n' && rcv != '\r' && rcv != ';') {
          serialState = STATE_ERROR;
        }
        break;
      case STATE_QM:
        if (rcv == ';') {
          Serial.print("numPixels:");
          Serial.print(NUMPIXELS, DEC);
          Serial.print(" bitRate:");
          Serial.print(BITRATE, DEC);
          Serial.println();
          serialState = STATE_IDLE;
        } else {
          serialState = STATE_ERROR;
        }
        break;
      case STATE_DATA:
        if (rcv == ';') {
          for (int i = nextled; i < NUMPIXELS; i++) {
            pixels.setPixelColor(i, 0);
          }  
          pixels.show();
          Serial.println("ok.");
          serialState = STATE_IDLE;
          break;
        }
        if (rcv >= '0' && rcv <='9') {
          rcv -= '0';  
        } else if (rcv >= 'A' && rcv <='F') {
          rcv -= 'A';  
          rcv += 10;
        } else if (rcv >= 'a' && rcv <='f') {
          rcv -= 'a';  
          rcv += 10;
        } else {
          serialState = STATE_ERROR;
          break;
        }
        rgb[rgbindex] |= rcv << nextdigit;
        if (nextled == NUMPIXELS) {
          serialState = STATE_ERROR;
          break;
        }
        if (nextdigit == 4) {
          nextdigit = 0;
        } else {
          nextdigit = 4;
          if (rgbindex == 2) {
            rgbindex = 0;
            pixels.setPixelColor(nextled, rgb[0], rgb[1], rgb[2]);
            rgb[0] = 0;
            rgb[1] = 0;
            rgb[2] = 0;
            nextled ++;
          } else {
            rgbindex ++;
          }
        }
        
        break;
    
            
      default: 
        // Also STATE_ERROR handled here
        // read and 
        serialState = STATE_ERROR;
        if (rcv == '\r' || rcv == '\n' || rcv == ';' || rcv == ' ') {
          serialState = STATE_IDLE;  
        } else {
          Serial.println("?");
        }
    }    
  }
}
