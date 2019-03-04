// Paint example specifically for the TFTLCD breakout board.
// If using the Arduino shield, use the tftpaint_shield.pde sketch instead!
// DOES NOT CURRENTLY WORK ON ARDUINO LEONARDO

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <TouchScreen.h>

#if defined(__SAM3X8E__)
    #undef __FlashStringHelper::F(string_literal)
    #define F(string_literal) string_literal
#endif

// When using the BREAKOUT BOARD only, use these 8 data lines to the LCD:
// For the Arduino Uno, Duemilanove, Diecimila, etc.:
//   D0 connects to digital pin 8  (Notice these are
//   D1 connects to digital pin 9   NOT in order!)
//   D2 connects to digital pin 2
//   D3 connects to digital pin 3
//   D4 connects to digital pin 4
//   D5 connects to digital pin 5
//   D6 connects to digital pin 6
//   D7 connects to digital pin 7

// For the Arduino Mega, use digital pins 22 through 29
// (on the 2-row header at the end of the board).
//   D0 connects to digital pin 22
//   D1 connects to digital pin 23
//   D2 connects to digital pin 24
//   D3 connects to digital pin 25
//   D4 connects to digital pin 26
//   D5 connects to digital pin 27
//   D6 connects to digital pin 28
//   D7 connects to digital pin 29

// For the Arduino Due, use digital pins 33 through 40
// (on the 2-row header at the end of the board).
//   D0 connects to digital pin 33
//   D1 connects to digital pin 34
//   D2 connects to digital pin 35
//   D3 connects to digital pin 36
//   D4 connects to digital pin 37
//   D5 connects to digital pin 38
//   D6 connects to digital pin 39
//   D7 connects to digital pin 40

#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
// optional
#define LCD_RESET A4

// Assign human-readable names to some common 16-bit color values:
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF


Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

#define BOXSIZE 40
#define PENRADIUS 3

#define LCD_HIGH    320
#define LCD_WIDE    240

#define LINE_WIDTH  30


#define TO_NORMAL   1     // turnout settings
#define TO_DIVERGE  2


#define OUT_TO_IN_TO_ID   1     // turnout IDS
#define IN_TO_OUT_TO_ID   2
#define SWIFTYS_TO_ID     3


#define MAINLINE_CTRL_X     85
#define OUT_TO_IN_CTRL_X    MAINLINE_CTRL_X
#define IN_TO_OUT_CTRL_X    MAINLINE_CTRL_X
#define SWIFTYS_CTRL_X      145

#define IN_TO_OUT_CTRL_Y   110
#define OUT_TO_IN_CTRL_Y   (LCD_HIGH - IN_TO_OUT_CTRL_Y)
#define SWIFTYS_CTRL_Y     (LCD_HIGH - 50)

#define TO_IND_RADIUS       20


// bits for inter-controller communications
//
// each time the panel is touched, a single byte with the following bits set or unset will be sent to the other controller
//
// bits are set if the turnouts are not TO_NORMAL
//
// if a controller gets a byte from the other controller, it changes it's status to reflect the new conditions
//
// each controller sets it's display to reflect the other controller but only one controller drives the H-bridge
// for the tortoises (both have the code to drive the H-bridges and set the bits that would drive the H-bridges but they
// are only conected to one controller
//
// for easier debugging, we actually or 0x30 with the bit values so the command byte is offset from ASCII '0'

#define   OUT_TO_IN_COMM_BIT    0x01  // bits for inter-controller communications
#define   IN_TO_OUT_COMM_BIT    0x02  // each controller starts in an all through state (bit is 0) and sends regular updates
#define   SWIFTYS_SW_COMM_BIT      0x04  

#define   IN_TO_OUT_HB_PIN   10    // output pins for the H-bridge (high means reversed)
#define   OUT_TO_IN_HB_PIN   11
#define   SWIFTYS_HB_PIN     12


#define   LOOP_SLEEP_DELAY  25    // enough time (in ms) for the "other" side to get the update, process it, and reflect it back


int OutToInState = TO_NORMAL;
int InToOutState = TO_NORMAL;
int SwiftysState = TO_NORMAL;

#define DEBOUNCE_DELAY  1000    // milliseconds between switch activations

#define MINPRESSURE 10
#define MAXPRESSURE 1000


void  getRemoteUpdate();



void setTurnout(int turnout, int direction) {

int color;
int pinOut;
int ctlrStatus;

  switch (direction) {

    case TO_NORMAL:
      color = GREEN;
      pinOut = HIGH;    // HIGH is mainline
      break;

    case TO_DIVERGE:
      color = RED;
      pinOut = LOW;     // LOW is diverging
      break;
  }

  switch (turnout) {

    case OUT_TO_IN_TO_ID:
      tft.fillCircle(OUT_TO_IN_CTRL_X, OUT_TO_IN_CTRL_Y, TO_IND_RADIUS, color);
      break;

    case IN_TO_OUT_TO_ID:
      tft.fillCircle(IN_TO_OUT_CTRL_X, IN_TO_OUT_CTRL_Y, TO_IND_RADIUS, color);
      break;

    case SWIFTYS_TO_ID:
        tft.fillCircle(SWIFTYS_CTRL_X, SWIFTYS_CTRL_Y, TO_IND_RADIUS, color);
        break;
  }

  // send an update - "on" bits indicate a thrown turnout

  ctlrStatus = ((OutToInState == TO_NORMAL) ? 0 : OUT_TO_IN_COMM_BIT) |
               ((InToOutState == TO_NORMAL) ? 0 : IN_TO_OUT_COMM_BIT) |
               ((SwiftysState == TO_NORMAL) ? 0 : SWIFTYS_SW_COMM_BIT);

  Serial.print((char)(ctlrStatus | 0x30));      // base on ASCII '0' to make it printable for easier debugging
}


void setup(void) {
  Serial.begin(9600);
  
  tft.reset();
  
  uint16_t identifier = tft.readID();

  if(identifier == 0x9325) {
    //Serial.println(F("Found ILI9325 LCD driver"));
  } else if(identifier == 0x9328) {
    //Serial.println(F("Found ILI9328 LCD driver"));
  } else if(identifier == 0x7575) {
    //Serial.println(F("Found HX8347G LCD driver"));
  } else if(identifier == 0x9341) {
    //Serial.println(F("Found ILI9341 LCD driver"));
  } else if(identifier == 0x8357) {
    //Serial.println(F("Found HX8357D LCD driver"));
  } else {
    //Serial.print(F("Unknown LCD driver chip: "));
    //Serial.println(identifier, HEX);
    //Serial.println(F("If using the Adafruit 2.8\" TFT Arduino shield, the line:"));
    //Serial.println(F("  #define USE_ADAFRUIT_SHIELD_PINOUT"));
    //Serial.println(F("should appear in the library header (Adafruit_TFT.h)."));
    //Serial.println(F("If using the breakout board, it should NOT be #defined!"));
    //Serial.println(F("Also if using the breakout, double-check that all wiring"));
    //Serial.println(F("matches the tutorial."));
    return;
  }

  tft.begin(identifier);

  tft.fillScreen(BLACK);

// mainlines
  tft.fillRect(40, 0, LINE_WIDTH, LCD_HIGH, BLUE);          // outside main
  tft.fillRect(100, 0, LINE_WIDTH, LCD_HIGH, BLUE);          // inside main
  tft.fillRect(175, 20, LINE_WIDTH, 200, BLUE);  // siding

// Crossovers (drawn with lot's of angled lines

  int i, xvrRight, xvrLeft;

  xvrRight = MAINLINE_CTRL_X - LINE_WIDTH / 2 - LINE_WIDTH;
  xvrLeft  = MAINLINE_CTRL_X + LINE_WIDTH / 2;     // out to in

  for (i = 0 ; i < 30 ; i++) {
    tft.drawLine(xvrRight, 10, xvrLeft, 100, BLUE);
    xvrRight++ ; xvrLeft++;
  }

  xvrRight = MAINLINE_CTRL_X - LINE_WIDTH / 2 - LINE_WIDTH;
  xvrLeft  = MAINLINE_CTRL_X + LINE_WIDTH / 2;     // in to out
  // xvrRight = 30 ; xvrLeft = 90;     // in to out

  for (i = 0 ; i < 30 ; i++) {
    tft.drawLine(xvrRight, LCD_HIGH - 10, xvrLeft, LCD_HIGH - 100, BLUE);
    xvrRight++ ; xvrLeft++;
  }
  
  xvrRight = 100 ; xvrLeft = 175;    // siding

  for (i = 0 ; i < 30 ; i++) {
    tft.drawLine(xvrRight, LCD_HIGH - 20, xvrLeft, 220, BLUE);
    xvrRight++ ; xvrLeft++;
  }

  setTurnout(OUT_TO_IN_TO_ID, OutToInState);
  setTurnout(IN_TO_OUT_TO_ID, InToOutState);
  setTurnout(SWIFTYS_TO_ID, SwiftysState);
  
  pinMode(13, OUTPUT);
}

unsigned long now;
unsigned long outToInTime = 0;
unsigned long inToOutTime = 0;
unsigned long swiftysTime = 0;


void loop()
{
  now = millis();
  
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);

  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  // pinMode(YM, OUTPUT);

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {

    // scale from 0->1023 to tft.width
    p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);

    p.x = LCD_WIDE - p.x; // Normalize to match graphic coordinates
    p.y = LCD_HIGH - p.y;
    
    // check to see if this is an active turnout control

    // OUT_TO_IN_TO_ID?

    if (p.x > (OUT_TO_IN_CTRL_X - TO_IND_RADIUS * 2) && p.x < (OUT_TO_IN_CTRL_X + TO_IND_RADIUS * 2) &&
        p.y > (OUT_TO_IN_CTRL_Y - TO_IND_RADIUS * 2) && p.y < (OUT_TO_IN_CTRL_Y + TO_IND_RADIUS * 2) &&
        now - outToInTime > DEBOUNCE_DELAY) {

       if (OutToInState == TO_NORMAL) {
         OutToInState = TO_DIVERGE;
       } else {
         OutToInState = TO_NORMAL;
       }

       setTurnout(OUT_TO_IN_TO_ID, OutToInState);
       outToInTime = now;
     }

    // IN_TO_OUT?

    if (p.x > (IN_TO_OUT_CTRL_X - TO_IND_RADIUS * 2) && p.x < (IN_TO_OUT_CTRL_X + TO_IND_RADIUS * 2) &&
        p.y > (IN_TO_OUT_CTRL_Y - TO_IND_RADIUS * 2) && p.y < (IN_TO_OUT_CTRL_Y + TO_IND_RADIUS * 2) &&
        now - inToOutTime > DEBOUNCE_DELAY) {

       if (InToOutState == TO_NORMAL) {
         InToOutState = TO_DIVERGE;
       } else {
         InToOutState = TO_NORMAL;
       }

       setTurnout(IN_TO_OUT_TO_ID, InToOutState);
       inToOutTime = now;
    }

    // SWIFTYS?

    if (p.x > (SWIFTYS_CTRL_X - TO_IND_RADIUS * 2) && p.x < (SWIFTYS_CTRL_X + TO_IND_RADIUS * 2) &&
        p.y > (SWIFTYS_CTRL_Y - TO_IND_RADIUS * 2) && p.y < (SWIFTYS_CTRL_Y + TO_IND_RADIUS * 2) &&
        now - swiftysTime > DEBOUNCE_DELAY) {

       if (SwiftysState == TO_NORMAL) {
         SwiftysState = TO_DIVERGE;
       } else {
         SwiftysState = TO_NORMAL;
       }

       setTurnout(SWIFTYS_TO_ID, SwiftysState);
       swiftysTime = now;
     }
  }

  delay(LOOP_SLEEP_DELAY);        // give the other side enough time to respond and reflect the change
  
  // check for an update from the other controller - if there are bytes to be read, process them

  if (Serial.available()) {
    getRemoteUpdate();
  }
}



// read updates from paired controller and set turnouts appropriately
//
// if there are more than one updates pending, only process the last one

void getRemoteUpdate() {

int data;

  if (Serial.available() == 0) {
    return;     // something happened - we thought there were updates but now there are none
  }

  // spin until we get the last one

  while (Serial.available() > 1) {
    data = Serial.read();         // eat any earlier ones until the count gets down to the last one
  }

  data = Serial.read();           // get the last one

  if (data < '0' || data > '7') {
    return;                       // out of range
  }
  
  // decode the last update and look for changes (we only care about changes)

  data &= OUT_TO_IN_COMM_BIT | IN_TO_OUT_COMM_BIT | SWIFTYS_SW_COMM_BIT;               // clear all other bits and leave only turnout bits

  if ((data & OUT_TO_IN_COMM_BIT) && OutToInState == TO_NORMAL) {         // OutToIn is thrown at the other side
    OutToInState = TO_DIVERGE;
    setTurnout(OUT_TO_IN_TO_ID, OutToInState);
  }

  if ( ! (data & OUT_TO_IN_COMM_BIT) && OutToInState == TO_DIVERGE) {  // OutToIn is thrown at the other side
    OutToInState = TO_NORMAL;
    setTurnout(OUT_TO_IN_TO_ID, OutToInState);
  }

  if ((data & IN_TO_OUT_COMM_BIT) && InToOutState == TO_NORMAL) {         // InToOut is thrown at the other side
    InToOutState = TO_DIVERGE;
    setTurnout(IN_TO_OUT_TO_ID, InToOutState);
  }

  if ( ! (data & IN_TO_OUT_COMM_BIT) && InToOutState == TO_DIVERGE) {  // InToOut is normal at the other side
    InToOutState = TO_NORMAL;
    setTurnout(IN_TO_OUT_TO_ID, InToOutState);
  }

  if ((data & SWIFTYS_SW_COMM_BIT) && SwiftysState == TO_NORMAL) {           // Swiftys is thrown at the other side
    SwiftysState = TO_DIVERGE;
    setTurnout(SWIFTYS_TO_ID, SwiftysState);
  }

  if ( ! (data & SWIFTYS_SW_COMM_BIT) && SwiftysState == TO_DIVERGE) {  // Swiftys is normal at the other side
    SwiftysState = TO_NORMAL;
    setTurnout(SWIFTYS_TO_ID, SwiftysState);
  }
}
