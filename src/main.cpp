/* ESP32 +TFT + Muses72323 Volume Controller
*********************

Author Geoff Webster

Initial version 1.0 July 2024
- SetVolume routine to display atten/gain (-111.75dB min to 0dB)

*/

#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <u8g2lib.h> // Hardware-specific library
#include <RC5.h>
#include <Muses72323.h> // Hardware-specific library
#include <ESP32RotaryEncoder.h>
#include <MCP23S08.h> // Hardware-specific library

// Current software
#define softTitle1 "ESP32/OLED"
#define softTitle2 "Muses72323 Controller"
// version number
#define VERSION_NUM "1.0"

/******* MACHINE STATES *******/
#define STATE_BALANCE 1 // when user adjusts balance
#define STATE_RUN 0     // normal run state
#define STATE_IO 1      // when user selects input/output
#define STATE_OFF 4     // when power down
#define ON LOW
#define OFF HIGH
#define STANDBY 0 // Standby
#define ACTIVE 1  // Active

// Preference modes
#define RW_MODE false
#define RO_MODE true

#define TIME_EXITSELECT 5 //** Time in seconds to exit I/O select mode when no activity

Preferences preferences;

// 23S08 Construct
MCP23S08 MCP(10); //  HW SPI address 0x00, CS GPIO10

//Display Construct
//TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
U8G2_SSD1322_NHD_256X64_1_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 5, /* dc=*/ 26, /* reset=*/ 15);	// Enable U8G2_16BIT in u8g2.h

// define IR input
unsigned int IR_PIN = 27;
// RC5 construct
RC5 rc5(IR_PIN);

// define preAmp control pins
const int s_select_72323 = 16;
//  The address wired into the muses chip (usually 0).
static const byte MUSES_ADDRESS = 0;

// preAmp construct
static Muses72323 Muses(MUSES_ADDRESS, s_select_72323); // muses chip address (usually 0), slave select pin0;

// define encoder pins
const uint8_t DI_ENCODER_A = 33;
const uint8_t DI_ENCODER_B = 32;
const int8_t DI_ENCODER_SW = 12;

// Rotary construct
RotaryEncoder rotaryEncoder(DI_ENCODER_A, DI_ENCODER_B, DI_ENCODER_SW);

/******* TIMING *******/
unsigned long milOnButton;  // Stores last time for switch press
unsigned long milOnAction;  // Stores last time of user input

/********* Global Variables *******************/

float atten;     // current attenuation, between 0 and -111.75
int16_t volume; // current volume, between 0 and -447
bool backlight;  // current backlight state
uint16_t counter = 0;
uint8_t source;        // current input channel
uint8_t oldsource = 1; // previous input channel
bool isMuted;          // current mute status
uint8_t state = 0;     // current machine state
uint8_t balanceState;  // current balance state
bool btnstate = 0;
bool oldbtnstate = 0;

/*System addresses and codes used here match RC-5 infra-red codes for amplifiers (and CDs)*/
uint16_t oldtoggle;
u_char oldaddress;
u_char oldcommand;
u_char toggle;
u_char address;
u_char command;

// Used to know when to fire an event when the knob is turned
volatile bool turnedRightFlag = false;
volatile bool turnedLeftFlag = false;

char buffer1[20] = "";

// Global Constants
//------------------
const char *inputName[] = {"  Phono ", "   Media  ", "     CD    ", "   Tuner  "}; // Elektor i/p board

// Function prototypes
void RC5Update(void);
void setIO();
void knobCallback(long value);
void buttonCallback(unsigned long duration);
void RotaryUpdate();
void volumeUpdate();
void setVolume();
void sourceUpdate();
void mute();
void unMute();
void toggleMute();

void knobCallback(long value)
{
  // See the note in the `loop()` function for
  // an explanation as to why we're setting
  // boolean values here instead of running
  // functions directly.

  // Don't do anything if either flag is set;
  // it means we haven't taken action yet
  if (turnedRightFlag || turnedLeftFlag)
    return;

  // Set a flag that we can look for in `loop()`
  // so that we know we have something to do
  switch (value)
  {
  case 1:
    turnedRightFlag = true;
    break;

  case -1:
    turnedLeftFlag = true;
    break;
  }

  // Override the tracked value back to 0 so that
  // we can continue tracking right/left events
  rotaryEncoder.setEncoderValue(0);
}

void buttonCallback(unsigned long duration)
{
  int _duration = duration;
  if (_duration > 50)
  {
    switch (state)
    {
    case STATE_RUN:
      state = STATE_IO;
      milOnButton = millis();
      break;
    default:
      break;
    }
  }
}

void volumeUpdate()
{
  if (turnedRightFlag)
  {
    if (isMuted)
    {
      unMute();
    }
    if (volume < 0)
    {
      volume = volume + 1;
      setVolume();
    }
    // Set flag back to false so we can watch for the next move
    turnedRightFlag = false;
  }
  else if (turnedLeftFlag)
  {
    if (isMuted)
    {
      unMute();
    }
    if (volume > -447)
    {
      volume = volume - 1;
      setVolume();
    }
    // Set flag back to false so we can watch for the next move
    turnedLeftFlag = false;
  }
}

void setVolume()
{
  //For debug
  /*
  Serial.println("");
  Serial.println(volume);
  */

  // set new volume setting
  Muses.setVolume(volume, volume);
  preferences.putInt("VOLUME", volume);
  // display volume setting
  float atten = ((float)volume / 4);
  sprintf(buffer1, "  %.2fdB  ", atten);
  u8g2.drawStr(0,15,buffer1);	// write something to the internal memory
  u8g2.sendBuffer();					// transfer internal memory to the display
}

void sourceUpdate()
{
  if (turnedRightFlag)
  {
    oldsource = source;
    milOnButton = millis();
    if (oldsource < 4)
    {
      source++;
    }
    else
    {
      source = 1;
    }
    setIO();
    // Set flag back to false so we can watch for the next move
    turnedRightFlag = false;
  }
  else if (turnedLeftFlag)
  {
    oldsource = source;
    milOnButton = millis();
    if (source > 1)
    {
      source--;
    }
    else
    {
      source = 4;
    }
    setIO();
    // Set flag back to false so we can watch for the next move
    turnedLeftFlag = false;
  }
}

void RC5Update()
{
  /*
  System addresses and codes used here match RC-5 infra-red codes for amplifiers (and CDs)
  */
  u_char toggle;
  u_char address;
  u_char command;
  // Poll for new RC5 command
  if (rc5.read(&toggle, &address, &command))
  {
    /* For Debug
    Serial.print("a:");
    Serial.print(address);
    Serial.print(" c:");
    Serial.print(command);
    Serial.print(" t:");
    Serial.println(toggle);*/
    
    if (address == 0x10) // standard system address for preamplifier
    {
      switch (command)
      {
      case 1:
        // Phono
        if ((oldtoggle != toggle))
        {
          oldsource = source;
          source = 1;
          setIO();
        }
        break;
      case 3:
        // Tuner
        if ((oldtoggle != toggle))
        {
          oldsource = source;
          source = 4;
          setIO();
        }
        break;
      case 7:
        // CD
        if ((oldtoggle != toggle))
        {
          oldsource = source;
          source = 3;
          setIO();
        }
        break;
      case 8:
        // Media
        if ((oldtoggle != toggle))
        {
          oldsource = source;
          source = 2;
          setIO();
        }
        break;
      case 13:
        // Mute
        if ((oldtoggle != toggle))
        {
          toggleMute();
        }
        break;
      case 16:
        // Increase Vol / reduce attenuation
        if (isMuted)
        {
          unMute();
        }
        if (volume < 0)
        {
          volume = volume + 1;
          setVolume();
        }
        break;
      case 17:
        // Reduce Vol / increase attenuation
        if (isMuted)
        {
          unMute();
        }
        if (volume > -447)
        {
          volume = volume - 1;
          setVolume();
        }
        break;
/*      case 59:
        // Display Toggle
        if ((oldtoggle != toggle))
        {
          if (backlight)
          {
            backlight = STANDBY;
            digitalWrite(TFT_BL, LOW); // Turn off backlight
            // mute();                    // mute output
          }
          else
          {
            backlight = ACTIVE;
            digitalWrite(TFT_BL, HIGH); // Turn on backlight
            // unMute(); // unmute output
          }
        }
        break;
        */
      default:
        break;
      }
    }
    else if (address == 0x14) // system address for CD
    {
      if ((oldtoggle != toggle))
      {
        if (command == 53) // Play
        {
          if (!backlight)
          {
            unMute(); // unmute output
          }
          oldsource = source;
          source = 3;
          setIO();
        }
      }
    }
    oldtoggle = toggle;
  }
}

void unMute()
{
  isMuted = 0;
  //  set volume
  setVolume();
  // set source
  setIO();
}

void mute()
{
  isMuted = 1;
  Muses.mute();
  u8g2.drawStr(0, 15,"    Muted    ");
  u8g2.sendBuffer();					// transfer internal memory to the display
}

void toggleMute()
{
  if (isMuted)
  {
    unMute();
  }
  else
  {
    mute();
  }
}

void RotaryUpdate()
{
  switch (state)
  {
  case STATE_RUN:
    volumeUpdate();
    break;
  case STATE_IO:
    sourceUpdate();
    if ((millis() - milOnButton) > TIME_EXITSELECT * 1000)
    {
      state = STATE_RUN;
    }
    break;
  default:
    break;
  }
}

void setIO()
{
  MCP.write1((oldsource - 1), LOW); // Reset source select to NONE
  MCP.write1((source - 1), HIGH);   // Set new source
  preferences.putUInt("SOURCE", source);
  if (isMuted)
  {
    // set volume
    setVolume();
  }
  u8g2.drawStr(0,45,inputName[source - 1]);
  u8g2.sendBuffer();
}

// This section of code runs only once at start-up.
void setup()
{
  Serial.begin(115200);

  // This tells the library that the encoder has no pull-up resistors and to use ESP32 internal ones
  rotaryEncoder.setEncoderType(EncoderType::FLOATING);

  // The encoder will only return -1, 0, or 1, and will not wrap around.
  rotaryEncoder.setBoundaries(-1, 1, false);

  // The function specified here will be called every time the knob is turned
  // and the current value will be passed to it
  rotaryEncoder.onTurned(&knobCallback);

  // The function specified here will be called every time the button is pushed and
  // the duration (in milliseconds) that the button was down will be passed to it
  rotaryEncoder.onPressed(&buttonCallback);

  // This is where the rotary inputs are configured and the interrupts get attached
  rotaryEncoder.begin();

  // Initialise the screen
  u8g2.begin();
  u8g2.clearBuffer();					// clear the internal memory
  u8g2.setFont(u8g2_font_ncenB14_tr);	// choose a suitable font
  // show software version briefly in display
  u8g2.drawStr(0,15,softTitle1);	// write something to the internal memory
  u8g2.drawStr(0,30,softTitle2);
  u8g2.drawStr(0, 45,"SW ver " VERSION_NUM);
  u8g2.sendBuffer();					// transfer internal memory to the display
  delay(2000);
  // Clear screen
  u8g2.clearBuffer();					// clear the internal memory
  u8g2.sendBuffer();					// transfer internal memory to the display

  // This initialises the Source select pins as outputs, all deselected (i.e. o/p=low)
  MCP.begin();
  MCP.pinMode8(0x00); //  0 = output , 1 = input

  // Initialize muses (SPI, pin modes)...
  Muses.begin();
  Muses.setExternalClock(false); // must be set!
  Muses.setZeroCrossingOn(true);
  Muses.mute();
  // Load saved settings (volume, balance, source)
  preferences.begin("settings", RW_MODE);
  source = preferences.getUInt("SOURCE", 1);
  volume = preferences.getInt("VOLUME", -447);
  if (volume>0)
    volume = -447;
  delay(10);
  // set startup volume
  setVolume();
  // set source
  setIO();
  // unmute
  isMuted = 0;
}

void loop()
{
  RC5Update();
  RotaryUpdate();
}
