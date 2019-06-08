/*
  
  I/O Selectric Serial Interface v1.32 2019-06-08
  
  (c) 2013-2019 L J Wilkinson
 
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.  
  
  ==

  This program for an Arduino Mega takes serial data in on Serial1
  and drives the I/O Selectric printer via digital outputs and inputs
  as defined below.
  
  The code uses the feedback contacts to maximise the rate at which
  printing occurs.
  
  This file does not attempt to explain how you drive a Selectric,
  for this please refer to the various Selectric and I/O Selectric
  manuals available on Bitsavers and its mirrors, including:
  241-5308_IO_SelectricRefMan
  S225-1726-7_IOseleMnt_Nov70
  
  To use this code you will need a suitable driver.  There is a one-to-one
  correspondence between Arduino outputs and Selectric solenoids.  Depending
  on how the I/O Selectric is wired, you may need a low-side or high-side driver.

  There is also a simple correspondence between inputs and Selectric contacts.
  The contacts can be wired so that they switch to ground, but a stronger
  pull-up (e.g. 20mA / 250R) is suggested in place of the internal pull-ups of
  the Arduino.
  
  V1.1
  Re-work to remove String objects
  Add Min/Max times to statistics
  Remove superfluous debugging printlns
  Fix various timeouts and stat calcs
  Add check for previous cycle complete at start of print operation
  
  V1.2
  Tidy up timeouts
  Fix up correspondence coding table
  Add cycle time stats
  Add XON/XOFF and CTS/RTS handshaking
  
  V1.3
  Add keyboard support

  V1.31
  Tidy up timeouts for real Selectric
  Select appropriate options for use with S/360 emulator
  Prevent CR from terminating too early
  Fix bug in CR stats, add CONTACT_ definitions
  Allow correct keyboard operation when LOCK not used
  Compact stats printout, fix printChars()
  
  V1.32 2019-06-08
  Assign I/O for PCB as designed and ordered
  
 */
#define vers "v1.32"
#define date "2019-06-08"

// Bit rate of Serial1 port (NOT the monitor port)
#define externalPortRate 9600

// Input buffer sizing
#define INPUT_BUFFER_SIZE 4000
#define HIGH_LIMIT 3800  // Send 'Stop' (XOFF / CTS off)
#define LOW_LIMIT  3700  // Send 'Go' (XON / CTS on)

// XON/XOFF handshaking
#define XONXOFF false
#define XON 0x11
#define XOFF 0x13
#define XONXOFFRATE 100  // ms between transmissions
// RATE must be at least buffer margin / transmission rate

// CTS/RTS handshaking (CTS for printing, RTS for sending)
#define CTSRTS false

// Whether to lock the keyboard while printing:
#define LOCK_KEYBOARD false

// Whether to send Monitor typing to the typewriter
#define LOCAL_MODE true

// Whether to send Monitor typing down the serial link
#define REMOTE_MODE true

// Whether to treat an incoming LF character as an EOL
#define TREAT_LF_AS_CR true

// What to send when CR is pressed (0x0D or 0x04)
#define CR_SEND_CHAR 0x0d

// Whether a long press on the status button should also print the stats
// summary on the typewriter (as well as on the monitor)
#define printStatsToTypewriter true

/*
  Arduino Mega 2560 Reserved I/O
  0-1 Serial
  13 LED  
  14,15 Serial3
  16,17 Serial2
  18,19 Serial1
  20-21 TWI
  50-53 SPI
 */
 
/*
  Output pin definitions:
  (Note: The letter after each output or input definition is the pin
          on the 50-way AMP connector into the typewriter)
          
  Note: The definition for SOL_TAB and SOL_BSP can be removed, which will remove support
        for the Backspace and Tab functions.  SOL_BSP can be used for overtyping, but
        SOL_TAB would probably not be used on a printer.
*/
#define  SOL_T2     7 // A
#define  SOL_CK     9 // B
#define  SOL_T1     8 // C
#define  SOL_R2A    3 // D
#define  SOL_R1     4 // E
#define  SOL_R2     5 // F
#define  SOL_R5     6 // H
#define  SOL_LOCK   2 // K
#define  SOL_TAB   10 // L
#define  SOL_SP    11 // M
#define  SOL_BSP   12 // N
#define  SOL_CR    13 // P
#define  SOL_INDEX 22 // R
#define  SOL_UC    23 // S
#define  SOL_LC    24 // T
// Output 17 is reserved for an additional solenoid (perhaps ribbon control)
#define  SOL_ON   HIGH
#define  SOL_OFF  LOW

/* Print & operation cycle timeouts, in milliseconds */
#define TO_CHAR_Start    50 // SOL to C2 opening
#define TO_CHAR_Finish  120 // SOL to C1 & C2 closing

#define TO_SP_Start   50 // SOL to C5 opening
#define TO_SP_Finish 120 // SOL to C5 closing

#define TO_CR_Start  100 // SOL to C6 opening
#define TO_CR_Finish 1000 // SOL to C6 closing

#define TO_INDEX_Start  100 // SOL to C6 closing
#define TO_INDEX_Finish 200 // SOL to C6 closing

#define TO_BSP_Start  100 // SOL to C5 opening
#define TO_BSP_Finish 200 // SOL to C5 closing

#define TO_TAB_Start   100 // SOL to C5 opening
#define TO_TAB_Finish 1000 // SOL to C5 closing

#define TO_UC_Start   50 // SOL to C3 opening
#define TO_UC_Finish 120 // SOL to C3 closing
#define TO_LC_Start  100 // SOL to C4 opening
#define TO_LC_Finish 200 // SOL to C4 closing

#define TO_LOCK  100  // SOL to Keyboard Mode closing
#define TO_UNLOCK 200 // SOL to Keyboard Mode opening

#define POST_SWITCH_DELAY 10  // Debounce delay after each switch activation
#define MIN_CYCLE_TIME 40  // Pad out print cycle to this minimum duration
#define SOLENOID_HOLD_DELAY 5  // Solenoids are kept powered after cycle starts

/*  
  Contact input definitions:
  (Note: Pin X,d & y are grounded, so switches go Low when closed)
*/

#define  C_C1_NC    26 // W
#define  C_CRTAB_NC 27 // Y
#define  C_LC       28 // Z
#define  C_C2_6_NC  29 // a
#define  C_C2_6_NO  30 // b
#define  C_CRTAB_NO 31 // c
#define  C_EOL_NC   32 // e
#define  C_EOL_NO   33 // f
#define  C_UC       34 // h
#define  C_LOCK_NC  35 // k
#define  C_LOCK_NO  36 // m
#define  C_C1_NO    37 // n

#define  C_OPEN    HIGH
#define  C_CLOSED  LOW

/* Contact numbers for individual functions */
#define  CONTACT_PRINT 2
#define  CONTACT_UC    3
#define  CONTACT_LC    4
#define  CONTACT_SP    5
#define  CONTACT_BSP   5
#define  CONTACT_TAB   5
#define  CONTACT_CR    6
#define  CONTACT_INDEX 6 

/*
  Keyboard input definitions:
  (These are not used in this version)
*/
#define  K_PAR   38 // p
#define  K_T2    39 // r
#define  K_CK    40 // s
#define  K_T1    41 // t
#define  K_R2A   42 // u
#define  K_R1    43 // v
#define  K_R2    44 // w
#define  K_R5    45 // x
#define  K_TAB   46 // z
#define  K_SP    47 // AA
#define  K_BSP   48 // BB
#define  K_CR    49 // CC
#define  K_LF    50 // DD
#define  K_UC    51 // EE
#define  K_LC    52 // FF

#define K_OPEN    HIGH
#define K_CLOSED  LOW

/*
  42 pins defined above are: ABCDEFHKLMNPRSTWYZabcefhkmnprstuvwxz AA BB CC DD EE FF
  8 other pins on the 50-way connector:
  Magnet common (0V)        J
  Ribbon Red (unused)       U
  Ribbon Black (unused)     V
  Switch common (0V)        X
  CR/TAB interlock in (0V)  d
  Ribbon mode (unused)      j
  Keyboard common (0V)      y
  Unused                    HH
 */


/*
  Other input definitions:
*/

// Differential current measurement
// ARef is internal 1.1V, resistance is 1.00ohm
// so can measure up to 1.1A with 1mA resolution
#define  ANA_CURRENT_H  1
#define  ANA_CURRENT_L  0
#define  ANA_REF  INTERNAL1V1
#define  CURRENT_VOLTAGE 1.1
#define  CURRENT_RESISTOR 1.00

// Hardware handshaking
#define  CTS  17  // Out
#define  RTS  16  // In

// A switch to use for debugging purposes
#define  SW_STATUS  25
#define  SW_OFF HIGH
#define  SW_ON  LOW

// A LED or other output
#define LED 53

/*
  Type Element definitions, ASCII to Tilt/Rotate (Correspondence code)
  
  For each character we define an 8 bit number: SHIFT-T2-CK-T1-R2A-R1-R2-R5
  SHIFT: 0=unshifted 1=shifted
  T1,T2 = Tilt
  CK = Check digit (parity)
  R1,R2,R2A,R5 = Rotate
  
  These values can be read directly from the table on p4-8 of the I/O Selectric Reference Manual
  0x00 means an unprintable character which is ignored

  Substitutions:
  ` => (degree) 0xbe
 
  Missing from the type element (and ignored):
  < > \ ^ _ { | }
 
*/
#define CORR_SHIFT 0x80
#define CORR_T2    0x40
#define CORR_CK    0x20
#define CORR_T1    0x10
#define CORR_R2A   0x08
#define CORR_R1    0x04
#define CORR_R2    0x02
#define CORR_R5    0x01

byte correspondence[128] =
{
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 00-0F
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // 10-1F
0x00,0xbe,0x9c,0x00,0xd8,0xa5,0xdd,0x1c,0xf0,0xd4,0xfa,0xff,0x29,0x20,0x1a,0xdb, // 20-2F  !"#$%&'()*+,-./
0x54,0x7f,0x7a,0x5b,0x75,0x7c,0x58,0x5d,0x79,0x70,0x8d,0x0d,0x00,0x2a,0x00,0xaa, // 30-3F 0123456789:;<=>?
0xf5,0x99,0xc0,0xc9,0xed,0xcc,0x8b,0xaf,0xe4,0xb8,0x8e,0xe8,0xc5,0x9f,0xca,0x95, // 40-4F @ABCDEFGHIJKLMNO
0xac,0x88,0xbd,0xb4,0xee,0xeb,0xbb,0x90,0xcf,0x84,0xde,0x00,0x00,0x00,0x00,0xa0, // 50-5F PQRSTUVWXYZ[\]^_
0x3e,0x19,0x40,0x49,0x6d,0x4c,0x0b,0x2f,0x64,0x38,0x0e,0x68,0x45,0x1f,0x4a,0x15, // 60-6F `abcdefghijklmno
0x2c,0x08,0x3d,0x34,0x6e,0x6b,0x3b,0x10,0x4f,0x04,0x5e,0x00,0x00,0x00,0x00,0x00  // 70-7F pqrstuvwxyz{|}~
};
byte keyb[256]; // This will be the inverse of the correspondence[] table

/*
  Serial input buffering 
*/
char inputBuffer[INPUT_BUFFER_SIZE];
int inputBufferInPtr = 0;
int inputBufferOutPtr = 0;

/*
  Statistics definitions
*/
#define numStats 7
#define stats_char  0
#define stats_sp    1
#define stats_lc    2
#define stats_uc    3
#define stats_cr    4
#define stats_lf    5
#define stats_lock  6

unsigned long stats_cycles[numStats];
unsigned long stats_startTimeouts[numStats];
unsigned long stats_finishTimeouts[numStats];
unsigned long stats_totalStartTime[numStats]; // milliseconds total
unsigned long stats_minStartTime[numStats]; // milliseconds
unsigned long stats_maxStartTime[numStats]; // milliseconds
unsigned long stats_totalFinishTime[numStats]; // milliseconds total
unsigned long stats_minFinishTime[numStats]; // milliseconds
unsigned long stats_maxFinishTime[numStats]; // milliseconds
unsigned long stats_totalCycleTime[numStats]; // milliseconds total
unsigned long stats_minCycleTime[numStats]; // milliseconds
unsigned long stats_maxCycleTime[numStats]; // milliseconds
float stats_averageCurrent[numStats];
float stats_averageCurrent_T2;
float stats_averageCurrent_CK;
float stats_averageCurrent_T1;
float stats_averageCurrent_R2A;
float stats_averageCurrent_R1;
float stats_averageCurrent_R2;
float stats_averageCurrent_R5;
unsigned long stats_cycles_T1;
unsigned long stats_cycles_T2;
unsigned long stats_cycles_CK;
unsigned long stats_cycles_R1;
unsigned long stats_cycles_R2;
unsigned long stats_cycles_R2A;
unsigned long stats_cycles_R5;

// For remembering where the print carriage is
boolean atLeftColumn = false;
boolean justDoneReturn = false;

// For remembering whether the keyboard is locked
boolean keyboardLocked = false;

// Handshaking
#if XONXOFF
unsigned long lastXonXoffSent;
#endif

// Status switch handling
boolean statusSwitchPressed = false;
unsigned long statusSwitchPressTime = 0;

void setup() {
  // initialize both serial ports:
  Serial.begin(9600); // Monitor port
  Serial1.begin(externalPortRate); // Port from/to CPU
  // initialise I/O
  pinMode(SOL_T1,OUTPUT);    digitalWrite(SOL_T1,SOL_OFF);
  pinMode(SOL_T2,OUTPUT);    digitalWrite(SOL_T2,SOL_OFF);
  pinMode(SOL_CK,OUTPUT);    digitalWrite(SOL_CK,SOL_OFF);
  pinMode(SOL_R1,OUTPUT);    digitalWrite(SOL_R1,SOL_OFF);
  pinMode(SOL_R2,OUTPUT);    digitalWrite(SOL_R2,SOL_OFF);
  pinMode(SOL_R2A,OUTPUT);   digitalWrite(SOL_R2A,SOL_OFF);
  pinMode(SOL_R5,OUTPUT);    digitalWrite(SOL_R5,SOL_OFF);
#ifdef SOL_TAB  
  pinMode(SOL_TAB,OUTPUT);   digitalWrite(SOL_TAB,SOL_OFF);
#endif  
  pinMode(SOL_SP,OUTPUT);    digitalWrite(SOL_SP,SOL_OFF);
#ifdef SOL_BSP  
  pinMode(SOL_BSP,OUTPUT);   digitalWrite(SOL_BSP,SOL_OFF);
#endif  
  pinMode(SOL_CR,OUTPUT);    digitalWrite(SOL_CR,SOL_OFF);
  pinMode(SOL_INDEX,OUTPUT); digitalWrite(SOL_INDEX,SOL_OFF);
  pinMode(SOL_UC,OUTPUT);    digitalWrite(SOL_UC,SOL_OFF);
  pinMode(SOL_LC,OUTPUT);    digitalWrite(SOL_LC,SOL_OFF);
  pinMode(SOL_LOCK,OUTPUT);  digitalWrite(SOL_LOCK,SOL_OFF);
  
  /*
    Define inputs to have pullups, but also fit an
    external 250R pullup/wetting on the C_ and K_
    inputs for more reliable contact operation
   */
  pinMode(C_C1_NC,INPUT_PULLUP);
  pinMode(C_CRTAB_NC,INPUT_PULLUP);
  pinMode(C_LC,INPUT_PULLUP);
  pinMode(C_C2_6_NC,INPUT_PULLUP);
  pinMode(C_C2_6_NO,INPUT_PULLUP);
  pinMode(C_CRTAB_NO,INPUT_PULLUP);
  pinMode(C_EOL_NC,INPUT_PULLUP);
  pinMode(C_EOL_NO,INPUT_PULLUP);
  pinMode(C_UC,INPUT_PULLUP);
  pinMode(C_LOCK_NC,INPUT_PULLUP);
  pinMode(C_LOCK_NO,INPUT_PULLUP);
  pinMode(C_C1_NO,INPUT_PULLUP);

  pinMode(K_PAR,INPUT_PULLUP);
  pinMode(K_T2,INPUT_PULLUP);
  pinMode(K_CK,INPUT_PULLUP);
  pinMode(K_T1,INPUT_PULLUP);
  pinMode(K_R2A,INPUT_PULLUP);
  pinMode(K_R1,INPUT_PULLUP);
  pinMode(K_R2,INPUT_PULLUP);
  pinMode(K_R5,INPUT_PULLUP);
  pinMode(K_TAB,INPUT_PULLUP);
  pinMode(K_SP,INPUT_PULLUP);
  pinMode(K_BSP,INPUT_PULLUP);
  pinMode(K_CR,INPUT_PULLUP);
  pinMode(K_LF,INPUT_PULLUP);
  pinMode(K_UC,INPUT_PULLUP);
  pinMode(K_LC,INPUT_PULLUP);

#ifdef CTS
  pinMode(CTS,OUTPUT);
  #ifdef CTSRTS
    digitalWrite(CTS,LOW);  // Stop for now
  #else
    digitalWrite(CTS,HIGH); // Leave enabled
  #endif
#endif
#ifdef RTS
  pinMode(RTS,INPUT_PULLUP);
#endif  
  
  pinMode(SW_STATUS,INPUT_PULLUP);
  
  analogReference(ANA_REF);
  
  for (int i=0;i<INPUT_BUFFER_SIZE;i++) inputBuffer[i]='!'; // Fill with guard characters
  
  for (int i=0;i<128;i++) keyb[correspondence[i]]=i; // Fill in inverse correspondence table

  Serial.print("I/O Selectric "); Serial.print(vers); Serial.print(" ");Serial.println(date);
}

void loop() {
  // Read a character from Port 1
  checkSerial();
  
  // Handle buffered data
  if (inputBufferContents()) {
    int ch = getFromInputBuffer() & 0x7f;
    // Echo to Port 0  
    Serial.write(ch); 
    // Print
    lockKeyboard();
    if (ch > 0x20)
      printCharacter(ch);
    else
      printControl(ch);
  }
  // Check for input from console
  else if (Serial.available()) {
    char ch = Serial.read();
    if (ch=='{') {
      displayInputs();
    }
    else if (ch=='}') {
      printStats();
    }
    else {
#if REMOTE_MODE    
      // Send to channel serial port
#if CTSRTS
      if (digitalRead(RTS)==HIGH) {
        Serial1.write(ch);
      }
#else
      Serial1.write(ch);
#endif   
#endif   
#if LOCAL_MODE  
      // Use console as a typewriter:
      lockKeyboard();
      if (ch > 0x20)
        printCharacter(ch);
      else
        printControl(ch);
#endif      
    }
  }
  else {
  // Nothing to do
    if (locked() && shifted()) {
      shiftDown();
    }
#if CTSRTS
    if (digitalRead(RTS)==HIGH) {
      unlockKeyboard();
    }
    else {
      lockKeyboard();
    }
#else
    unlockKeyboard();
#endif    
  }
  
  // Check for XON/XOFF transmission
  #if XONXOFF
  if (millis()>(lastXonXoffSent + XONXOFFRATE)) {
    if (inputBufferContents()>HIGH_LIMIT) {
      Serial1.write((char)XOFF);
    }
    if (inputBufferContents()<LOW_LIMIT) {
      Serial1.write((char)XON);
    }
    lastXonXoffSent = millis();
  }
  #endif
  
  // Check for CTS change
  #if CTSRTS
    if (inputBufferContents()>HIGH_LIMIT) {
      digitalWrite(CTS,LOW);
    }
    if (inputBufferContents()<LOW_LIMIT) {
      digitalWrite(CTS,HIGH);
    }
  #endif
  
  // Check for keyboard data
  if (!locked() && digitalRead(C_C1_NO)!=C_OPEN) {
    // Print cycle in progress - see what it is
    int ch;
    unsigned long TO_2 = millis() + TO_CHAR_Finish;

    // code is SHIFT T2 CK T1 R2A R1 R2 R5
    int code = shifted()?CORR_SHIFT:0;
    if (digitalRead(K_T2 )!=K_OPEN  ) code |= CORR_T2;
    if (digitalRead(K_CK )!=K_OPEN  ) code |= CORR_CK;
    if (digitalRead(K_T1 )!=K_OPEN  ) code |= CORR_T1;
    if (digitalRead(K_R2A)!=K_OPEN  ) code |= CORR_R2A;
    if (digitalRead(K_R1 )!=K_OPEN  ) code |= CORR_R1;
    if (digitalRead(K_R2 )!=K_OPEN  ) code |= CORR_R2;
    if (digitalRead(K_R5 )!=K_OPEN  ) code |= CORR_R5;
    if ((code != 0) && (ch = keyb[code])!=0) { 
      keyboard(ch);
      }
    // Wait for print cycle to finish
    while (digitalRead(C_C1_NO)!=C_OPEN) {
      checkSerial();
      if (millis()>TO_2) {
          Serial.print("Timeout waiting for C1 to close after keypress "); Serial.println(millis());
          break;
        }
      }
    checkDelay(POST_SWITCH_DELAY);
    }
  
  if (!locked() && digitalRead(C_C2_6_NO)!=C_OPEN) {
    // Print cycle in progress - see what it is
    unsigned long TO_2 = millis() + TO_CHAR_Finish;

    if (digitalRead(K_SP )==K_CLOSED) { keyboard(0x20); }
    else if (digitalRead(K_CR )==K_CLOSED) { keyboard(CR_SEND_CHAR); }
    else if (digitalRead(K_TAB)==K_CLOSED) { keyboard(0x09); }
    else if (digitalRead(K_BSP)==K_CLOSED) { keyboard(0x08); }
    else if (digitalRead(K_LF )==K_CLOSED) { keyboard(0x0a); }
    else return;
    // Cycle could also be Shift Up or Shift Down    
    // Wait for print cycle to finish
    while (digitalRead(C_C2_6_NO)!=C_OPEN) {
      checkSerial();
      if (millis()>TO_2) {
          Serial.print("Timeout waiting for C2-6 to close after operation "); Serial.println(millis());
          break;
        }
      }
    checkDelay(POST_SWITCH_DELAY);
    }

  // Check for status printout request
  if (digitalRead(SW_STATUS)==SW_ON) {
    if (!statusSwitchPressed) {
      statusSwitchPressed = true;
      statusSwitchPressTime = millis();
    }
  }
  else if (statusSwitchPressed) {
    if (millis()-statusSwitchPressTime < 250) {
      // Short press: Display input contact status
      displayInputs();
    }
    else {
      // Long press: Display statistics
      printStats();
    }
    statusSwitchPressed = false;
  }
}

/*
  See if the typewriter is indicating that it is at the end of the line
  If so we can only do a CR or LF
 */
boolean atEOL() {
  if (digitalRead(C_EOL_NO)==C_CLOSED) 
  {
    Serial.print("EOL @"); Serial.println(millis());
    return true;
  }
  return false;
}

/*
  Lock the keyboard if not already
 */
void lockKeyboard() {
  float current;
  int stats = stats_lock;
  unsigned long start = millis();
  unsigned long TO = start + TO_LOCK;
  keyboardLocked = true;
  if (locked() or !LOCK_KEYBOARD)
    return;
  digitalWrite(SOL_LOCK,SOL_ON);
  while (!locked()) {
    checkSerial();
    if (millis()>TO) {
      Serial.print("Lock timeout @"); Serial.println(millis());
      if (stats>=0) {
        stats_startTimeouts[stats]++;
        stats = -1;
      }
      break;
    }
  }
  if (stats>=0) {
    unsigned long time = millis()-start;
    current = solenoidCurrent();
    stats_averageCurrent[stats] = (stats_averageCurrent[stats]*stats_cycles[stats]+current)/(stats_cycles[stats]+1);
    if ((stats_cycles[stats]==0) || (time<stats_minStartTime[stats])) {
      stats_minStartTime[stats] = time;
    }
    if ((stats_cycles[stats]==0) || (time>stats_maxStartTime[stats])) {
      stats_maxStartTime[stats] = time;
    }
    stats_totalStartTime[stats] += time;
    stats_cycles[stats]++;
  }
  checkDelay(POST_SWITCH_DELAY);
}

/*
  Unlock the keyboard if not already
 */
void unlockKeyboard() {
  int stats = stats_lock;
  unsigned long start = millis();
  int TO = start + TO_UNLOCK;
  keyboardLocked = false;
  if (!locked() or !LOCK_KEYBOARD)
    return;
  digitalWrite(SOL_LOCK,SOL_OFF);
  while (locked()) {
    checkSerial();
    if (millis()>TO) {
      Serial.print("Unlock timeout @"); Serial.println(millis());
      if (stats>=0) {
        stats_finishTimeouts[stats]++;
        stats = -1;
      }
      break;
    }
  }
  if (stats>=0) {
    unsigned long time = millis()-start;
    // Note check for cycles <=1 to force min & max on first unlock
    if ((stats_cycles[stats]<=1) || (time<stats_minFinishTime[stats])) {
      stats_minFinishTime[stats] = time;
    }
    if ((stats_cycles[stats]<=1) || (time>stats_maxFinishTime[stats])) {
      stats_maxFinishTime[stats] = time;
    }
    stats_totalFinishTime[stats] += time;
  }
  checkDelay(POST_SWITCH_DELAY);
}

/*
  Check for keyboard locked
  by reading appropriate contact, or just using our internal status
 */
boolean locked() {
    return LOCK_KEYBOARD?(digitalRead(C_LOCK_NO)==C_CLOSED):keyboardLocked;
}

/*
  Measure solenoid current (excludes LOCK)
 */
float solenoidCurrent() {
  return (float)(analogRead(ANA_CURRENT_H)-analogRead(ANA_CURRENT_L)) * CURRENT_VOLTAGE / 1024 / CURRENT_RESISTOR * 1000;
}

/*
  Print a single character
 */
void printCharacter(char ch)
{
  float current;
  unsigned long start,TO_1,TO_2,cycleStart;
  unsigned long time;
  int stats;
  // Check if printable
  int code = correspondence[ch & 0x7f];
  if (code==0)
    return;
  // Check that we are allowed to print
  if (atEOL())
    return;
  // Check for shift change
  // "." and "," are repeated on both UC and LC so no need to check shift for these
  // This may not be the case for all type elements
  if ((code!=0x1a)|| (code!=0x29)) {
    if ((code & CORR_SHIFT)!=0) {
      shiftUp();
    }
    else {
      shiftDown();
    }
  }
  start = millis();
  TO_1 = start + TO_CHAR_Start;
  TO_2 = start + TO_CHAR_Finish;
  stats = stats_char;
  // Check for completion of previous cycle (C2 & C1 closed)
  if (digitalRead(C_C2_6_NC)==C_OPEN) {
    Serial.print("C2 not closed prior to printing @"); Serial.println(millis());
    while (digitalRead(C_C2_6_NC)==C_OPEN) {
      checkSerial();
      if (millis()>TO_1) {
        if (stats>=0) {
          Serial.print("Timeout waiting for C2 to close prior to printing @"); Serial.println(millis());
          if (stats>=0) {
            stats_finishTimeouts[stats]++;
            stats = -1;
          }
          break;
        }
      }
    }
    // Debounce delay
    checkDelay(POST_SWITCH_DELAY);
  }
  // Fire solenoids
  digitalWrite(SOL_T1,(code & CORR_T1)?SOL_ON:SOL_OFF);
  digitalWrite(SOL_T2,(code & CORR_T2)?SOL_ON:SOL_OFF);
  digitalWrite(SOL_CK,(code & CORR_CK)?SOL_ON:SOL_OFF);
  digitalWrite(SOL_R1,(code & CORR_R1)?SOL_ON:SOL_OFF);
  digitalWrite(SOL_R2,(code & CORR_R2)?SOL_ON:SOL_OFF);
  digitalWrite(SOL_R2A,(code & CORR_R2A)?SOL_ON:SOL_OFF);
  digitalWrite(SOL_R5,(code & CORR_R5)?SOL_ON:SOL_OFF);
  // No longer at left-hand column (if we were)
  atLeftColumn = false;

  // Wait for start (C2)
  while (digitalRead(C_C2_6_NC)==C_CLOSED) {
    checkSerial();
    if (millis()>TO_1) {
      Serial.print("Timed out waiting for C2 to open @"); Serial.println(millis());
      if (stats>=0) {
        stats_startTimeouts[stats]++;
        stats = -1;
      }
      break;
    }
    if (digitalRead(C_C2_6_NC)==C_OPEN) {
      checkDelay(5);
    }
  }
  
  cycleStart = millis();
  
  if (stats>=0) {
    time = millis()-start;
    if ((stats_cycles[stats]==0) || (time<stats_minStartTime[stats])) {
      stats_minStartTime[stats] = time;
    }
    if ((stats_cycles[stats]==0) || (time>stats_maxStartTime[stats])) {
      stats_maxStartTime[stats] = time;
    }
    stats_totalStartTime[stats] += time;
  }
  // Snapshot current in mA
  current = solenoidCurrent();
  // Correct for lock solenoid current
  if (locked()) {
    current -= stats_averageCurrent[stats_lock];
  }
  // Hold solenoids for a while
  checkDelay(SOLENOID_HOLD_DELAY);
  // Release solenoids
  digitalWrite(SOL_T1,SOL_OFF);
  digitalWrite(SOL_T2,SOL_OFF);
  digitalWrite(SOL_CK,SOL_OFF);
  digitalWrite(SOL_R1,SOL_OFF);
  digitalWrite(SOL_R2,SOL_OFF);
  digitalWrite(SOL_R2A,SOL_OFF);
  digitalWrite(SOL_R5,SOL_OFF);

  // Enforce minimum cycle time
  checkDelay(MIN_CYCLE_TIME-SOLENOID_HOLD_DELAY);

  // Wait for completion (C2 closed)
  while (digitalRead(C_C2_6_NC)==C_OPEN) {
    checkSerial();
    if (millis()>TO_2) {
      Serial.print("Timed out waiting for C2 to close @"); Serial.println(millis());
      Serial.print("start=");Serial.println(start);
      Serial.print("TO_2=");Serial.println(TO_2);
      if (stats>=0) {
        stats_finishTimeouts[stats]++;
        stats = -1;
      }
      while (true) ;
      break;
    }
    if (digitalRead(C_C2_6_NC)==C_CLOSED) {
      checkDelay(5);
    }
  }
  if (stats>=0) {
    time = millis()-start;
    unsigned long cycleTime = millis()-cycleStart;
    if ((stats_cycles[stats]==0) || (time<stats_minFinishTime[stats])) {
      stats_minFinishTime[stats] = time;
    }
    if ((stats_cycles[stats]==0) || (time>stats_maxFinishTime[stats])) {
      stats_maxFinishTime[stats] = time;
    }
    stats_totalFinishTime[stats] += time;
    if ((stats_cycles[stats]==0) || (cycleTime<stats_minCycleTime[stats])) {
      stats_minCycleTime[stats] = cycleTime;
    }
    if ((stats_cycles[stats]==0) || (cycleTime>stats_maxCycleTime[stats])) {
      stats_maxCycleTime[stats] = cycleTime;
    }
    stats_totalCycleTime[stats] += cycleTime;
    // Calculate average current per solenoid
    int numSolenoids =
            ((code & CORR_T1)?1:0) +
            ((code & CORR_T2)?1:0) +
            ((code & CORR_CK)?1:0) +
            ((code & CORR_R1)?1:0) +
            ((code & CORR_R2)?1:0) +
            ((code & CORR_R2A)?1:0) +
            ((code & CORR_R5)?1:0);
    current = current / numSolenoids;
    /* Update solenoid current stats */
    if (code & CORR_T1) {
      stats_averageCurrent_T1 = (stats_averageCurrent_T1*stats_cycles_T1+current)/(stats_cycles_T1+1);
      stats_cycles_T1++;
    }
    if (code & CORR_T2) {
      stats_averageCurrent_T2 = (stats_averageCurrent_T2*stats_cycles_T2+current)/(stats_cycles_T2+1);
      stats_cycles_T2++;
    }
    if (code & CORR_CK) {
      stats_averageCurrent_CK = (stats_averageCurrent_CK*stats_cycles_CK+current)/(stats_cycles_CK+1);
      stats_cycles_CK++;
    }
    if (code & CORR_R1) {
      stats_averageCurrent_R1 = (stats_averageCurrent_R1*stats_cycles_R1+current)/(stats_cycles_R1+1);
      stats_cycles_R1++;
    }
    if (code & CORR_R2) {
      stats_averageCurrent_R2 = (stats_averageCurrent_R2*stats_cycles_R2+current)/(stats_cycles_R2+1);
      stats_cycles_R2++;
    }
    if (code & CORR_R2A) {
      stats_averageCurrent_R2A = (stats_averageCurrent_R2A*stats_cycles_R2A+current)/(stats_cycles_R2A+1);
      stats_cycles_R2A++;
    }
    if (code & CORR_R5) {
      stats_averageCurrent_R5 = (stats_averageCurrent_R5*stats_cycles_R5+current)/(stats_cycles_R5+1);
      stats_cycles_R5++;
    }
    stats_cycles[stats]++;
  }
  // Debounce delay
  checkDelay(POST_SWITCH_DELAY);
}

boolean shifted()
{
  return (digitalRead(C_UC)==C_CLOSED);
}

void shiftUp()
{
  if (!shifted()) {
    waitForC(SOL_UC,4,stats_uc,TO_UC_Start,TO_UC_Finish);
  }
}

void shiftDown()
{
  if (shifted()) {
    waitForC(SOL_LC,3,stats_lc,TO_LC_Start,TO_LC_Finish);
  }
}

void printControl(int ch)
{
  if (TREAT_LF_AS_CR && (ch==0x0a)) {
    ch = 0x0d;
  }
  switch (ch) {
    case 0x20:
      atLeftColumn = false;
      // Fire SP solenoid
      if (!atEOL()) {
        waitForC(SOL_SP,CONTACT_SP,stats_sp,TO_SP_Start,TO_SP_Finish);
      }
      break;
    case 0x0d:
      if (!atLeftColumn) {
        // Fire CR solenoid
        waitForC(SOL_CR,CONTACT_CR,stats_cr,TO_CR_Start,TO_CR_Finish);
        atLeftColumn = true;
        justDoneReturn = true;
      }
      else {
        // Ensure a following LF will actually index
        justDoneReturn = false;
      }
      break;
    case 0x0a:
        // Fire LF (Index) solenoid
        // Return does an index also, so ignore the first LF after a CR
        if (!justDoneReturn) {
          waitForC(SOL_INDEX,CONTACT_INDEX,stats_lf,TO_INDEX_Start,TO_INDEX_Finish);
        }
        else {
          justDoneReturn = false;
        }
        
      break;
#ifdef SOL_BSP      
    case 0x08:
        // Fire BSP solenoid
        waitForC(SOL_BSP,CONTACT_BSP,-1,TO_BSP_Start,TO_BSP_Finish);
        break;
#endif
#ifdef SOL_TAB
    case 0x09:
        atLeftColumn = false;
        // Fire TAB solenoid
        if (!atEOL()) {
          waitForC(SOL_TAB,CONTACT_TAB,-1,TO_TAB_Start,TO_TAB_Finish);
        }
        break;
#endif        
    default:
      break;
  }        
}

/*
  waitForC fires a solenoid and waits for the operation cycle to complete
  The various operations have different contacts, but they are all connected in series
  and monitored here.
  C2  Printing
  C3  UC
  C4  LC
  C5  SP,BSP,TAB
  C6  CR,LF(INDEX)
  
  Parameters:
    solenoid
      The Arduino pin number of the output to be controlled
    stats
      The index into the statistics arrays, or -1 to ignore
    start_to
      The number of milliseconds after firing the solenoid by which the operation contact should have gone active
    finish_to
      The number of milliseconds after firing the solenoid by which the operation contact should have gone inactive
*/
int waitForC(int solenoid, int contact, int stats, unsigned long start_to, unsigned long finish_to)
{
  float current;
  unsigned long start = millis();
  unsigned long startCycle;
  unsigned long TO_1 = start + start_to;
  unsigned long TO_2 = start + finish_to;
  unsigned long time;
  
  // Wait for completion of previous cycle (C2-6 & C1 closed)
  while (digitalRead(C_C2_6_NC)==C_OPEN) {
    checkSerial();
    if (millis()>TO_1) {
      Serial.print("C2-6 not closed prior to printing @"); Serial.println(millis());
      if (stats>=0) {
        stats_finishTimeouts[stats]++;
        stats = -1;
      }
      break;
    }
  }

  digitalWrite(solenoid,SOL_ON);
  // Debounce delay
  checkDelay(POST_SWITCH_DELAY);
  
  // Wait for Control to start (C2-5)
  while (digitalRead(C_C2_6_NC)==C_CLOSED) {
    checkSerial();
    if (millis()>TO_1) {
      Serial.print("Timed out waiting for C");Serial.print(contact);Serial.print(" to open @"); Serial.println(millis());
      if (stats>=0) {
        stats_startTimeouts[stats]++;
        stats = -1;
      }
      break;
    }
    if (digitalRead(C_C2_6_NC)==C_OPEN) {
      checkDelay(5);
    }
  }
  startCycle = millis();
  if (stats>=0) {
    time = millis()-start;
    if ((stats_cycles[stats]==0) || (time<stats_minStartTime[stats])) {
      stats_minStartTime[stats] = time;
    }
    if ((stats_cycles[stats]==0) || (time>stats_maxStartTime[stats])) {
      stats_maxStartTime[stats] = time;
    }
    stats_totalStartTime[stats] += time;
  }
  // Snapshot current in mA
  current = solenoidCurrent();
  // Correct for lock solenoid current
  if (locked()) {
    current -= stats_averageCurrent[stats_lock];
  }
  // Keep solenoid activated for a while
  checkDelay(SOLENOID_HOLD_DELAY);
  // Turn off solenoid
  digitalWrite(solenoid,SOL_OFF);
  
  // Wait for Control to finish (C2-6)
  while (digitalRead(C_C2_6_NC)==C_OPEN) {
    checkSerial();
    if (millis()>TO_2) {
      Serial.print("Timed out waiting for C");Serial.print(contact);Serial.print(" to close @"); Serial.println(millis());
      if (stats>=0) {
        stats_finishTimeouts[stats]++;
        stats = -1;
      }
      break;
    }
    if (digitalRead(C_C2_6_NC)==C_CLOSED) {
      checkDelay(5);
    }
  }
  // Debounce delay
  checkDelay(POST_SWITCH_DELAY);
  // For CR and TAB, wait for interlock to close
#ifdef SOL_TAB
  if ((solenoid == SOL_CR) || (solenoid == SOL_TAB)) {
#else
  if (solenoid == SOL_CR) {
#endif    
    checkDelay(100); // Guarantee that movement has commenced
    while ((digitalRead(C_CRTAB_NC)==C_OPEN) || (digitalRead(C_CRTAB_NO)==C_CLOSED)) {
      checkSerial();
      if (millis()>TO_2) {
        Serial.print("Timed out waiting for Return or Tab @"); Serial.println(millis());
        if (stats>=0) {
          stats_finishTimeouts[stats]++;
          stats = -1;
        }
        break;
      }
      if (!((digitalRead(C_CRTAB_NC)==C_OPEN) || (digitalRead(C_CRTAB_NO)==C_CLOSED))) {
        checkDelay(5);
      }
    }
  }
  if (stats>=0) {
    time = millis()-start;
    unsigned long cycleTime = millis()-startCycle;
    if ((stats_cycles[stats]==0) || (time<stats_minFinishTime[stats])) {
      stats_minFinishTime[stats] = time;
    }
    if ((stats_cycles[stats]==0) || (time>stats_maxFinishTime[stats])) {
      stats_maxFinishTime[stats] = time;
    }
    stats_totalFinishTime[stats] += time;
    if ((stats_cycles[stats]==0) || (cycleTime<stats_minCycleTime[stats])) {
      stats_minCycleTime[stats] = cycleTime;
    }
    if ((stats_cycles[stats]==0) || (cycleTime>stats_maxCycleTime[stats])) {
      stats_maxCycleTime[stats] = cycleTime;
    }
    stats_totalCycleTime[stats] += cycleTime;
    if (locked()) {
      current -= stats_averageCurrent[stats_lock];
    }
    stats_averageCurrent[stats] = (stats_averageCurrent[stats]*stats_cycles[stats]+current)/(stats_cycles[stats]+1);
    stats_cycles[stats]++;
  }
}

/* Handle a character typed on the keyboard */
/* Normally send it to the Serial1 port */
void keyboard(int ch)
{
  Serial1.write(ch);
//  if (ch>=' ') Serial.write(ch);
//  else if (ch==CR_SEND_CHAR) Serial.println();
//  else if (ch==0x08) Serial.print("<BS>");
//  else if (ch==0x09) Serial.print("<TAB>");
//  else if (ch==0x0a) Serial.print("<LF>");
//  else {
    Serial.print("Char<");
    Serial.print(ch,HEX);
    Serial.print(">");
//  }
}

/*
  Print out a statistics summary showing the number of characters printed,
  number of CRs, LFs, shift and lock actuations
  Also the number of timeouts encountered
  Prints to console and (optionally) typewriter
 */
void printStats()
{
  #define maxStatsStringLength 130
  char str[maxStatsStringLength];
  char num[10];
  str[0]='\0';
  int cps10 = 10000 / (stats_totalFinishTime[stats_char]/stats_cycles[stats_char]);
  if (printStatsToTypewriter)
  {
    lockKeyboard();
    if (!atLeftColumn)
      printControl('\r');
  }

  // Print everything
  strcat(str,"!\"#$%&'()*+,-./0123456789:;=?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]`abcdefghijklmnopqrstuvwxyz~");
  Serial.println(str);
  if (printStatsToTypewriter)
    printChars(str);

  // Total characters printed, average start (ms), average finish (ms), average rate (cps): CH NNNN SSSms FFFms RRRcps
  strcpy(str,"CH:  ");
  stats1(str,stats_char);
  strcat(str," CPS=");
  if (stats_cycles[stats_char]>0) {
    sprintf(num,"%2i",cps10/10);
    strcat(str,num);
    strcat(str,".");
    sprintf(num,"%1i",cps10 % 10);
    strcat(str,num);
  }
  else
    strcat(str,"-");
  Serial.println(str);
  if (printStatsToTypewriter)
    printChars(str);

  strcpy(str,"          Iavg T2:");
  sprintf(num,"%3i",(int)stats_averageCurrent_T2);
  strcat(str,num);
  strcat(str," CK:");
  sprintf(num,"%3i",(int)stats_averageCurrent_CK);
  strcat(str,num);
  strcat(str," T1:");
  sprintf(num,"%3i",(int)stats_averageCurrent_T1);
  strcat(str,num);
  strcat(str," R2A:");
  sprintf(num,"%3i",(int)stats_averageCurrent_R2A);
  strcat(str,num);
  strcat(str," R1:");
  sprintf(num,"%3i",(int)stats_averageCurrent_R1);
  strcat(str,num);
  strcat(str," R2:");
  sprintf(num,"%3i",(int)stats_averageCurrent_R2);
  strcat(str,num);
  strcat(str," R5:");
  sprintf(num,"%3i",(int)stats_averageCurrent_R5);
  strcat(str,num);
  strcat(str,"mA");
  Serial.println(str);
  if (printStatsToTypewriter)
    printChars(str);

   // SP stats
  strcpy(str,"SP:  ");
  stats1(str,stats_sp);
  Serial.println(str);
  if (printStatsToTypewriter)
    printChars(str);
    
  // UC, LC shift stats
  strcpy(str,"UC:  ");
  stats1(str,stats_uc);
  Serial.println(str);
  if (printStatsToTypewriter)
    printChars(str);

  strcpy(str,"LC:  ");
  stats1(str,stats_lc);
  Serial.println(str);
  if (printStatsToTypewriter)
    printChars(str);

  // CR, LF stats
  strcpy(str,"CR:  ");
  stats1(str,stats_cr);
  Serial.println(str);
  if (printStatsToTypewriter)
  {
    printChars(str);
  }

  strcpy(str,"LF:  ");
  stats1(str,stats_lf);
  Serial.println(str);
  if (printStatsToTypewriter)
  {
    printChars(str);
  }
  
  // Total LOCK, average finish: LOCK NNNN SSSms
  strcpy(str,"LOCK:");
  stats1(str,stats_lock);
  Serial.println(str);
  if (printStatsToTypewriter)
  {
    printChars(str);
  }
  
  // Finished printing stats
  if (printStatsToTypewriter)
  {
    unlockKeyboard();
  }
}

char* stats1(char* str, int stat) {
  char num[10];
  sprintf(num,"%6i",stats_cycles[stat]);
  strcat(str,num);
  if (stats_cycles[stat]>0) {
    strcat(str," S:");
    sprintf(num,"%3i-",stats_minStartTime[stat]);
    strcat(str,num);
    sprintf(num,"%3i-",stats_totalStartTime[stat]/stats_cycles[stat]);
    strcat(str,num);
    sprintf(num,"%3i",stats_maxStartTime[stat]);
    strcat(str,num);
    strcat(str," F:");
    sprintf(num,"%3i-",stats_minFinishTime[stat]);
    strcat(str,num);
    sprintf(num,"%3i-",stats_totalFinishTime[stat]/stats_cycles[stat]);
    strcat(str,num);
    sprintf(num,"%3i",stats_maxFinishTime[stat]);
    strcat(str,num);
    strcat(str," C:");
    sprintf(num,"%3i-",stats_minCycleTime[stat]);
    strcat(str,num);
    sprintf(num,"%3i-",stats_totalCycleTime[stat]/stats_cycles[stat]);
    strcat(str,num);
    sprintf(num,"%3i",stats_maxCycleTime[stat]);
    strcat(str,num);
  }
  else {
    strcat(str," S:   -   -    F:   -   -    C:   -   -   ");
  }
  strcat(str," STO=");
  sprintf(num,"%3i",stats_startTimeouts[stat]);
  strcat(str,num);
  strcat(str," FTO=");
  sprintf(num,"%3i",stats_finishTimeouts[stat]);
  strcat(str,num);
  if (stat != stats_char) {
    strcat(str," Iavg=");
    sprintf(num,"%3i",(int)stats_averageCurrent[stat]);
    strcat(str,num);
    strcat(str,"mA");
  }
  return str;
}

/*
  Print a char* string on the typewriter
 */
void printChars(char* buff) {
  char * p = buff;
  while (*p!='\0') {
    if (*p<=' ') {
      printControl(*p++);
    }
    else {
      printCharacter(*p++);
    }
  }
  printControl('\r');
}

/*
  The following routines implement a simple input FIFO buffer
 */
int inputBufferContents() {
  noInterrupts();
  int len = inputBufferInPtr - inputBufferOutPtr;
  interrupts();
  if (len<0) len+=INPUT_BUFFER_SIZE;
  return len;
}

bool inputBufferFull() {
  return (inputBufferContents()>=(INPUT_BUFFER_SIZE-1));
}

boolean addToInputBuffer(char ch) {
  if (inputBufferFull()) return false;
  noInterrupts();
  inputBuffer[inputBufferInPtr++] = ch;
  if (inputBufferInPtr>=INPUT_BUFFER_SIZE) inputBufferInPtr-=INPUT_BUFFER_SIZE;
  interrupts();
  return true;
}

char getFromInputBuffer() {
  char ch;
  if (inputBufferOutPtr == inputBufferInPtr) return '\0';
  noInterrupts();
  ch = inputBuffer[inputBufferOutPtr++];
  if (inputBufferOutPtr>=INPUT_BUFFER_SIZE) inputBufferOutPtr-=INPUT_BUFFER_SIZE;
  interrupts();
  return ch;
}

/*
  Serial polling routine
  To be called in all wait loops
 */
void checkSerial() {
  while (Serial1.available() && !inputBufferFull()) {
    if (!addToInputBuffer(Serial1.read())) {
      Serial.println("Input buffer overrun");
    }
  }
}

/*
  To delay, while checking for serial data
 */
void checkDelay(int del)
{
  for (int i=del; i--; i>0) {
    delay(1);
    checkSerial();
  }
}

/*
  Display the state of all the inputs
 */
void displayInputs() {
  char str[410]; // Up to 15 per contact * 27 + final null = 406
  str[0]='\0';
  // 12 printer contacts
  strcat(str,"C1 NC  "); strcat(str,inputState(C_C1_NC));
  strcat(str,"C1 NO  "); strcat(str,inputState(C_C1_NO));
  strcat(str,"C2_6_NC"); strcat(str,inputState(C_C2_6_NC));
  strcat(str,"C2_6_NO"); strcat(str,inputState(C_C2_6_NO));
  strcat(str,"CRTABNC"); strcat(str,inputState(C_CRTAB_NC));
  strcat(str,"CRTABNO"); strcat(str,inputState(C_CRTAB_NO));
  strcat(str,"LC     "); strcat(str,inputState(C_LC));
  strcat(str,"UC     "); strcat(str,inputState(C_UC));
  strcat(str,"EOL_NC "); strcat(str,inputState(C_EOL_NC));
  strcat(str,"EOL_NC "); strcat(str,inputState(C_EOL_NO));
  strcat(str,"LOCK_NC"); strcat(str,inputState(C_LOCK_NC));
  strcat(str,"LOCK_NO"); strcat(str,inputState(C_LOCK_NO));
  
  // 15 keyboard contacts
  strcat(str,"K PAR  "); strcat(str,inputState(K_PAR));
  strcat(str,"K T2   "); strcat(str,inputState(K_T2));
  strcat(str,"K CK   "); strcat(str,inputState(K_CK));
  strcat(str,"K T1   "); strcat(str,inputState(K_T1));
  strcat(str,"K R2A  "); strcat(str,inputState(K_R2A));
  strcat(str,"K R1   "); strcat(str,inputState(K_R1));
  strcat(str,"K R2   "); strcat(str,inputState(K_R2));
  strcat(str,"K R5   "); strcat(str,inputState(K_R5));
  strcat(str,"K TAB  "); strcat(str,inputState(K_TAB));
  strcat(str,"K SP   "); strcat(str,inputState(K_SP));
  strcat(str,"K BSP  "); strcat(str,inputState(K_BSP));
  strcat(str,"K CR   "); strcat(str,inputState(K_CR));
  strcat(str,"K LF   "); strcat(str,inputState(K_LF));
  strcat(str,"K UC   "); strcat(str,inputState(K_UC));
  strcat(str,"K LC   "); strcat(str,inputState(K_LC));
  Serial.print(str);
}

char* inputState(int input) {
  if (digitalRead(input)==C_OPEN)
    return " OPEN\n";
  else
    return " CLOSED\n";
}


