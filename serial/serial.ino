#include <Adafruit_NeoPixel.h>
#include <avr/power.h>

// Make sure that HardwareSerial.h contains
// define SERIAL_RX_BUFFER_SIZE to 128
// after its #ifndef HardwareSerial_h
// TODO: benchmark actual requirement


// Used in heartbeat
#define DEVICEID       "1234"

// heartbeat period in ms
#define HBPERIOD       250

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN            6

// active low button for lamp test
#define PIN_LAMPTEST   7

#define PIN_IDENT_BUTTON 8
#define PIN_IDENT_LED 9
#define PIN_HB_LED 10

#define PIN_UPDATE_LED_1  2
#define PIN_UPDATE_LED_2  3
#define PIN_UPDATE_LED_3  4
#define PIN_UPDATE_LED_4  5

// How many NeoPixels are attached to the Arduino?
#define MAXNUMPIXELS      512

#define DEBOUNCE_MILLIS   50


// Serial port bitrate
#define BITRATE        1000000

// Setial port state machine
// We are in empty packets
#define STATE_IDLE     0

// Something wrong happened, ignore everything until the first newline or ';' character
#define STATE_ERROR    1

// Question mark found
#define STATE_QM       2

// We are in receiving data ('!')
#define STATE_DATA     3

// Next chars are the LED counts
#define STATE_RCV_BINARY_COUNT_MSB    4
#define STATE_RCV_BINARY_COUNT_LSB    5

// Next char is binary data
#define STATE_RCV_BINARY_DATA_NO_REALLOC     6
#define STATE_RCV_BINARY_DATA_WILL_REALLOC   7

// Next char is expected to be closing ';'
#define STATE_RCV_BINARY_DATA_NO_REALLOC_EOF     8
#define STATE_RCV_BINARY_DATA_WILL_REALLOC_EOF   9

// State of serial receive loop, using STATE_xxx values
uint8_t serialState;

// Next R/G/B index expected (text mode)
uint8_t   rgbindex;

// Next LED address expected (text mode)
uint8_t   nextled;

// Next digit index (text mode)
uint8_t   nextdigit;

uint8_t rgb[3];

// Used in binary mode
uint16_t  allBytesToReceive;

// Used in binary mode
uint16_t  nextByteIndex;

// Neopixel driver library sends out all the LEDs
// Buffer is reallocated when binary protocol is used after frame transmission
// Note that when reallocation is needed the first frame will be dropped
bool       needsRealloc;
uint16_t   reallocTo;

unsigned long nextHb;



// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(MAXNUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
#if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  // End of trinket special code

  // for lamp test
  pinMode(PIN_LAMPTEST, INPUT_PULLUP);
  pinMode(PIN_IDENT_BUTTON, INPUT_PULLUP);
  pinMode(PIN_IDENT_LED, OUTPUT);
  pinMode(PIN_HB_LED, OUTPUT);
  pinMode(PIN_UPDATE_LED_1, OUTPUT);
  pinMode(PIN_UPDATE_LED_2, OUTPUT);
  pinMode(PIN_UPDATE_LED_3, OUTPUT);
  pinMode(PIN_UPDATE_LED_4, OUTPUT);

  digitalWrite(PIN_IDENT_LED, HIGH);
  digitalWrite(PIN_HB_LED, HIGH);
  digitalWrite(PIN_UPDATE_LED_1, HIGH);
  digitalWrite(PIN_UPDATE_LED_2, HIGH);
  digitalWrite(PIN_UPDATE_LED_3, HIGH);
  digitalWrite(PIN_UPDATE_LED_4, HIGH);


  pixels.begin();

  pixels.clear();
  pixels.show();
  delay(100);
  for(int i = 1; i < MAXNUMPIXELS; i = i << 1) {
    pixels.clear();
    memset(pixels.getPixels(), 255, i * 3);
    pixels.show();
    delay(100);
  }

  for(int i = 0; i < 2; i++) {
    for (int j = 0; j < MAXNUMPIXELS; j++) {
      pixels.setPixelColor(j, 255, 0, 0);
    }
    pixels.show();
    delay(100);
    for (int j = 0; j < MAXNUMPIXELS; j++) {
      pixels.setPixelColor(j, 0, 255, 0);
    }
    pixels.show();
    delay(100);
    for (int j = 0; j < MAXNUMPIXELS; j++) {
      pixels.setPixelColor(j, 0, 0, 255);
    }
    pixels.show();
    delay(100);
    for (int j = 0; j < MAXNUMPIXELS; j++) {
      pixels.setPixelColor(j, 255, 255, 255);
    }
    pixels.show();
    delay(100);
  }
  pixels.clear();
  pixels.show();
  
  digitalWrite(PIN_IDENT_LED, LOW);
  digitalWrite(PIN_UPDATE_LED_1, LOW);
  digitalWrite(PIN_UPDATE_LED_2, LOW);
  digitalWrite(PIN_UPDATE_LED_3, LOW);
  digitalWrite(PIN_UPDATE_LED_4, LOW);

  serialState = STATE_IDLE;
  Serial.begin(BITRATE);
}

uint8_t * pixelsArr = pixels.getPixels();

// message "e" (serial buffer empty) was sent since last reception
bool esent = false;
bool doingLt = false;
bool doingIdent = false;
bool identButtonReleased = true;
unsigned long identButtonDebounce = 0;
unsigned long updateLedsTimeout = 0;
unsigned long hbLedTimeout = 0;
uint8_t timecheck = 0;


void sendHeartbeat() {
  digitalWrite(PIN_HB_LED, HIGH);
  Serial.print("wssgw @ ");
  Serial.print(DEVICEID);
  if (doingIdent) {
    Serial.print(" ident");
  }
  Serial.println();
  hbLedTimeout = millis() + 25;
}

void sendAbort() {
  Serial.println("x");
}

void pollLamptest() {
  if (doingLt && digitalRead(PIN_LAMPTEST) == HIGH) {
    sendAbort();
    doingLt = false;
    digitalWrite(PIN_UPDATE_LED_1, LOW);
    digitalWrite(PIN_UPDATE_LED_2, LOW);
    digitalWrite(PIN_UPDATE_LED_3, LOW);
    digitalWrite(PIN_UPDATE_LED_4, LOW);
    digitalWrite(PIN_IDENT_LED, doingIdent ? HIGH : LOW);
    digitalWrite(PIN_HB_LED, LOW);
    pixels.clear();
    pixels.show();
    sendAbort();
  } else if (!doingLt && digitalRead(PIN_LAMPTEST) == LOW) {
    sendAbort();
    doingLt = true;
    pixels.updateLength(MAXNUMPIXELS);
    sendAbort();
    for (int j = 0; j < MAXNUMPIXELS; j++) {
      pixels.setPixelColor(j, 255, 255, 255);
    }
    pixels.show();
    sendAbort();
  }
  if (doingLt) {
    digitalWrite(PIN_UPDATE_LED_1, HIGH);
    digitalWrite(PIN_UPDATE_LED_2, HIGH);
    digitalWrite(PIN_UPDATE_LED_3, HIGH);
    digitalWrite(PIN_UPDATE_LED_4, HIGH);
    digitalWrite(PIN_IDENT_LED, HIGH);
    digitalWrite(PIN_HB_LED, HIGH);
  }
}

void doTimeCheck() {
  unsigned long now = millis();

  if (identButtonDebounce < now) {
    identButtonDebounce = now;
    if (digitalRead(PIN_IDENT_BUTTON) == HIGH && !identButtonReleased) {
      identButtonReleased = true;
      identButtonDebounce = now + DEBOUNCE_MILLIS;
    } else if (digitalRead(PIN_IDENT_BUTTON) == LOW && identButtonReleased) {
      identButtonReleased = false;
      identButtonDebounce = now + DEBOUNCE_MILLIS;
      doingIdent = !doingIdent;
      digitalWrite(PIN_IDENT_LED, doingIdent ? HIGH : LOW);
    }
  }
  if (updateLedsTimeout < now) {
    digitalWrite(PIN_UPDATE_LED_1, LOW);
    digitalWrite(PIN_UPDATE_LED_2, LOW);
    digitalWrite(PIN_UPDATE_LED_3, LOW);
    digitalWrite(PIN_UPDATE_LED_4, LOW);
  }
  if (hbLedTimeout < now) {
    digitalWrite(PIN_HB_LED, LOW);
  }
  if (now > nextHb) {
    nextHb = now + HBPERIOD;
    sendHeartbeat();
  }
}

void loop() {
  if (!Serial.available()) {
    if(!esent) {
      // send only one "e" message
      Serial.println("e");
      esent = true;
    }
    pollLamptest();
    if (!timecheck) {
      timecheck = 255;
      doTimeCheck();
    } else {
      timecheck --;
    }
  }
  while (Serial.available()) {
    esent = false;
    byte rcv = Serial.read();
    if (serialState == STATE_RCV_BINARY_DATA_NO_REALLOC) {
        pixelsArr[nextByteIndex] = (uint8_t) rcv;
        nextByteIndex++;
        if (nextByteIndex == allBytesToReceive) {
          serialState = STATE_RCV_BINARY_DATA_NO_REALLOC_EOF;
        }
        continue;
    }
    if (serialState == STATE_RCV_BINARY_DATA_WILL_REALLOC) {
        nextByteIndex++;
        if (nextByteIndex == allBytesToReceive) {
          serialState = STATE_RCV_BINARY_DATA_WILL_REALLOC_EOF;
        }
        continue;
    }
    if (serialState == STATE_IDLE && rcv == ' ') {
      continue;
    }
    switch (serialState) {
      case STATE_RCV_BINARY_COUNT_MSB:
        allBytesToReceive = (uint8_t) rcv;
        allBytesToReceive = allBytesToReceive << 8;
        serialState = STATE_RCV_BINARY_COUNT_LSB;
        continue;
      case STATE_RCV_BINARY_COUNT_LSB:
        allBytesToReceive = allBytesToReceive | (uint8_t) rcv;
        nextByteIndex = 0;
        reallocTo = 0;
        needsRealloc = false;
        if (allBytesToReceive == 0) {
          serialState = STATE_IDLE;
          Serial.println("+");
        } else {
          // it is number of leds now before applying *= 3
          if (allBytesToReceive > MAXNUMPIXELS) {
            serialState = STATE_ERROR;
            continue;
          }
          if (allBytesToReceive != pixels.numPixels() || doingLt) {
            reallocTo = allBytesToReceive;
            needsRealloc = true;
            serialState = STATE_RCV_BINARY_DATA_WILL_REALLOC;
          } else {
            serialState = STATE_RCV_BINARY_DATA_NO_REALLOC;
          }
          allBytesToReceive *= 3;
        }
        continue;
      case STATE_RCV_BINARY_DATA_NO_REALLOC_EOF:
        if (rcv == ';') {
          serialState = STATE_IDLE;
          if (!doingLt) {
            pixels.show();

            unsigned long now = millis();
            unsigned long dt = now - updateLedsTimeout + 50 + pixels.numPixels() / 33;
            updateLedsTimeout = now + 50;
            digitalWrite(PIN_UPDATE_LED_1, LOW);
            digitalWrite(PIN_UPDATE_LED_2, LOW);
            digitalWrite(PIN_UPDATE_LED_3, LOW);
            digitalWrite(PIN_UPDATE_LED_4, LOW);
            if (dt <= 10) {
              digitalWrite(PIN_UPDATE_LED_1, HIGH);
            } else if (dt <= 20) {
              digitalWrite(PIN_UPDATE_LED_2, HIGH);
            } else if (dt < 40) {
              digitalWrite(PIN_UPDATE_LED_3, HIGH);
            } else {
              digitalWrite(PIN_UPDATE_LED_4, HIGH);
            }
          }
          Serial.println("+");
        } else if (rcv == ' ') {
          serialState = STATE_IDLE;
        } else {
          serialState = STATE_ERROR;
        }
        break;
      case STATE_RCV_BINARY_DATA_WILL_REALLOC_EOF:
        if (rcv == ';') {
          serialState = STATE_IDLE;
          if (!doingLt) {
            // Lamp test is for all LEDS, do not realloc
            pixels.updateLength(reallocTo);
            pixelsArr = pixels.getPixels();
          }
          Serial.println("+");
        } else if (rcv == ' ') {
          serialState = STATE_IDLE;
        } else {
          serialState = STATE_ERROR;
        }
        break;
      case STATE_IDLE:
        if (rcv == 'b') {
          serialState = STATE_RCV_BINARY_COUNT_MSB;
        } else if (rcv == ';') {
          // send ack
          Serial.println("+");
        } else if (rcv == 'i') {
          doingIdent = false;
          digitalWrite(PIN_IDENT_LED, LOW);
          Serial.println("+");
        } else if (rcv == 'I') {
          doingIdent = true;
          digitalWrite(PIN_IDENT_LED, HIGH);
          Serial.println("+");
        } else if (rcv == '?') {
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
          Serial.print(MAXNUMPIXELS, DEC);
          Serial.print(" bitRate:");
          Serial.print(BITRATE, DEC);
          Serial.println();
          Serial.println("+");
          serialState = STATE_IDLE;
        } else {
          serialState = STATE_ERROR;
        }
        break;
      case STATE_DATA:
        if (rcv == ';') {
          for (int i = nextled; i < MAXNUMPIXELS; i++) {
            pixels.setPixelColor(i, 0);
          }  
          pixels.show();

          if (pixels.numPixels() != nextled) {
            pixels.updateLength(nextled);
          }
          Serial.println("+");
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
        if (nextled == MAXNUMPIXELS) {
          serialState = STATE_ERROR;
          break;
        }
        if (nextdigit == 4) {
          nextdigit = 0;
        } else {
          nextdigit = 4;
          if (rgbindex == 2) {
            rgbindex = 0;
            // setPixelColor checks LED index bounds
            // realloc happes at the end of the frame, extra but otherwise valid data will be dropped from this frame
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

      case STATE_ERROR:
      default: 
        // Also STATE_ERROR handled here
        // Exit on frame delimiter
        serialState = STATE_ERROR;
        if (rcv == '\r' || rcv == '\n' || rcv == ';' || rcv == ' ') {
          serialState = STATE_IDLE;
          if (rcv != ' ') {
            // safe to recover with sending spaces; terminate with double ';' to have an ack
            Serial.println("+");
          }
        } else {
          Serial.println("?");
        }
    }    
  }
}
