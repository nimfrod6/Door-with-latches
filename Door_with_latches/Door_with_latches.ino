#include "LiquidCrystal_I2C.h"
#include "Keypad.h"
#include "Wire.h"
#include "CapacitiveSensor.h"
#include <EEPROM.h>

typedef enum{
  LOCKED = 0,
  UNLOCKED
} lockStatusType;

typedef enum{
  LOCKED_MENU = 0,
  UNLOCKED_MENU,
  CODE_CHANGE_MENU
} menuType;

/******* KEYPAD *******************************/
const int ROW_NUM = 4; // four rows
const int COLUMN_NUM = 4; // four columns

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3', 'A'},
  {'4','5','6', 'B'},
  {'7','8','9', 'C'},
  {'*','0','#', 'D'}
};

byte pin_rows[ROW_NUM] = {6, 7, 8, 9}; //connect to the row pinouts of the keypad
byte pin_column[COLUMN_NUM] = {2, 3, 4, 5}; //connect to the column pinouts of the keypad
/******* END_KEYPAD ***************************/
/******* GLOBAL_VARIABLES *********************/
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);
lockStatusType _lockStatus = LOCKED;
menuType _menuType = LOCKED_MENU;
menuType* _menuTypePtr = &_menuType;

const unsigned long UNLOCK_ON_TIME = 3000;
const unsigned long UNLOCK_LATCHES_MAX_TIME = 10000;
const unsigned long AUTO_LOCK_TIME_RESET = 10000;      // resets unlock time back to 10 seconds
const unsigned long AUTO_LOCK_TIME_INCREMENT = 300000; // prolongs unlock time by 5 minutes
const unsigned long AUTO_LOCK_TIME_MAX = 7200000; // max 2 hours unlock time
const unsigned long AUTO_DISPLAY_OFF_TIME = 60000;
unsigned long AUTO_LOCK_TIME = 10000;
unsigned long _lastUnlockTime = 0;
unsigned long _lastKeyPressTime = 0;

char _passCode[] = {'1', '8', '0', '9'};
char _enteredCode[4] = {0};

int remainingTime[3] = {0, 0, 0}; //hh mm ss

int _enteredCodePosition = 0;
int _numOfBpresses = 0;

bool _unlockFlag = 0;
bool _changeCodeFlag = 0;
bool _lcdWrongCode = 0;

/******* END_GLOBAL_VARIABLES *****************/
/******* USED_PINS ****************************/
int _latchGatePin = 12;     // pin for gate of MOSFET transistor for latches
int _insideSwitchPin = 11;  // pin for handle capacitance
int _closedDoorSensor = A0; // switch for monitoring if the doors are closed, 0 for CLOSED
int SCA_PIN = A4;
int SCL_PIN = A5;
CapacitiveSensor   _insideSwitchSensor = CapacitiveSensor(10,_insideSwitchPin);
/******* END_USED_PINS ************************/
/******* SETUP() ******************************/
void setup() {
  pinMode( _latchGatePin, OUTPUT );
  digitalWrite( _latchGatePin, 0 );
  pinMode( _closedDoorSensor, INPUT_PULLUP );
  _insideSwitchSensor.set_CS_AutocaL_Millis(0xFFFFFFFF);
  Serial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  setCodeFromEEPROM();
}
/******* END_SETUP() **************************/
/******* MAIN() *******************************/
void loop() {
  char key = keypad.getKey();
  if( key )
  {
    _lastKeyPressTime = millis();
  }
  switch(_lockStatus)
  {
    
    case LOCKED:
    {
      if( isDigit(key))
      {
        _enteredCode[_enteredCodePosition++] = key; // inserts the pressed key into entered code array
        if( _enteredCodePosition >= 4 )
        {
          _enteredCodePosition = 0;
          CheckCode();
        }
        break;
      }
      switch(key)
      {
        case 'A':
        break;
        case 'B':
        ResetCode();
        break;
        case 'C':
        break;
        case 'D': 
        break;
      }
      _numOfBpresses = 0;
    }
    case UNLOCKED:
    {
      static unsigned long _lastDefaultTime;
      switch(key)
      {
        case 'A':
        _unlockFlag = 1;
        break;
        case 'B':
        _numOfBpresses++;
        if(_numOfBpresses > 4)
        {
          _numOfBpresses = 0;
          _changeCodeFlag = 1;
          _menuType = CODE_CHANGE_MENU;
        }
        break;
        break;
        case 'C':
        _lockStatus = LOCKED;
        _menuType = LOCKED_MENU;
        AUTO_LOCK_TIME = AUTO_LOCK_TIME_RESET;
        ResetCode();
        break;
        case 'D':
        if( AUTO_LOCK_TIME < AUTO_LOCK_TIME_MAX )
        {
          AUTO_LOCK_TIME += AUTO_LOCK_TIME_INCREMENT;
          if( AUTO_LOCK_TIME > AUTO_LOCK_TIME_MAX )
          {
            AUTO_LOCK_TIME = AUTO_LOCK_TIME_MAX;
          }
        }        
        break;
        default:
        break;
      }
      break;
    }
  }
  
  static unsigned long ISSonTime = 0;
  static unsigned long ISSoffTime = 0;
  if( _insideSwitchSensor.capacitiveSensor(30) > 60 )
  {
    if( millis() - ISSonTime > 250 )
    {
      _unlockFlag = 1;
    }
    if( ISSonTime - ISSoffTime < 200 )
    {
      _lockStatus = LOCKED;
      ResetCode();
      AUTO_LOCK_TIME = AUTO_LOCK_TIME_RESET;
    }
    ISSoffTime = millis();
  }
  else
  {
    ISSonTime = millis();
  }
  
  
  UnlockLatches();
  bool lockCheck = !digitalRead( _closedDoorSensor ); // 0 when switch closed, 1 when opened
  digitalWrite( 13, _lockStatus && lockCheck);
  AutoLock();
  printLCD(_menuTypePtr);
  if( Serial )
  {
    debug();
  }
}
/******* END_MAIN() ***************************/
/******* AUX_FUNCTIONS ************************/

void CheckCode()
{
  static int attempts = 0;

  for( int i = 0; i < 4; i++)
  {
    if( _passCode[i] != _enteredCode[i] )
    {
      attempts++;
      if( attempts > 2 )
      {
        delay( 1000*attempts ); // progressively delay if attempts fail
      }
      _menuType = LOCKED_MENU;
      return;
    }
  }
  attempts = 0;
  _unlockFlag = 1;
  _lastUnlockTime = millis();
  _lockStatus = UNLOCKED;
  _menuType = UNLOCKED_MENU;
}

void ResetCode()
{
  for( int i = 0; i < 4; i++)
  {
    _enteredCode[i] = 0;
  }
  _enteredCodePosition = 0;
}

bool UnlockLatches() // function for unlocking the latches and checking that power limit is not exceeded
{
  static unsigned long startTime = 100000;
  
  if( _unlockFlag == 1 )
  {
    startTime = millis();
    _unlockFlag = 0;
  }
  bool timeCondition = (millis() - startTime) < UNLOCK_ON_TIME;
  bool doorState = !digitalRead( _closedDoorSensor );
  return PowerLimit(timeCondition && doorState);
}

bool PowerLimit( bool condition)
{
  static unsigned long onTime = 0;
  static unsigned long lastCallTime = 0;
  unsigned long deltaTime = millis() - lastCallTime;
  static bool onCooldown = 0;

  if( condition && !onCooldown)
  {
    onTime += deltaTime;
  }
  else
  {
    if( (2*deltaTime) <= onTime)
    {
      onTime -= (2*deltaTime);
    }
    else
    {
      onTime = 0;
    }
  }
  
  if( onTime > UNLOCK_LATCHES_MAX_TIME )
  {
    onCooldown = 1;
  }
  if( onTime == 0 )
  {
    onCooldown = 0;
  }

  lastCallTime = millis();
  bool stateToReturn = (condition && !onCooldown);
  digitalWrite(_latchGatePin, stateToReturn);
  return stateToReturn;
}

unsigned long AutoLock() // locks the door after certain time has passed
{
  unsigned long passedTime = millis() - _lastUnlockTime;
  if( passedTime > AUTO_LOCK_TIME )
  {
    _lockStatus = LOCKED;
    _menuType = LOCKED_MENU;
    AUTO_LOCK_TIME = AUTO_LOCK_TIME_RESET;
  }
  return AUTO_LOCK_TIME - passedTime;
}

void debug()
{
  Serial.print("_lockStatus= " + String(_lockStatus) + "\t");
  Serial.print("latches: " + String(UnlockLatches())+ "\t");
  Serial.print("_enteredCode[]= ");
  for( int i = 0; i < 4; i++)
  {
    Serial.print(String(_enteredCode[i]));
  }
  Serial.print("\t");
  Serial.print("_passCode[]= ");
  for( int i = 0; i < 4; i++)
  {
    Serial.print(String(_passCode[i]));
  }
  Serial.print("\t");
  unsigned long remainingTime = ((millis() - _lastUnlockTime) > AUTO_LOCK_TIME)? 0 : (AUTO_LOCK_TIME - (millis() - _lastUnlockTime));
  Serial.print("timeToLock: " + String(remainingTime) + "\t");
  Serial.println("");
}

void LOG(String parameter, String parameterName)
{
  Serial.print(parameterName + "= " + parameter + "\t");
}

void printLCD(menuType* menu)
{
  String toPrintLCD[4] = {"LCD error, nothing  ","to display"};

  switch( *menu )
  {
    case LOCKED_MENU:
    LOG("", "LCDlocked");
    toPrintLCD[0] = "  VHOD V REZIDENCO  ";
    toPrintLCD[1] = "-----ZAKLENJENO-----";
    toPrintLCD[2] = "                    ";
    toPrintLCD[3] = "Geslo: ";
    for( int i = 0; i < 13; i++)
    {
      if( i < _enteredCodePosition )
      {
        toPrintLCD[3] += "*";
      }
      else
      {
        toPrintLCD[3] += " ";
      }
    }    
    break;
    case UNLOCKED_MENU:
    LOG("", "LCDunlocked");
    toPrintLCD[0] = "  VHOD V REZIDENCO  ";
    toPrintLCD[1] = "     ODKLENJENO     ";
    toPrintLCD[2] = "                    ";
    if( millis() - _lastUnlockTime < 1300 )
    {
      toPrintLCD[3] = "Geslo: ****         ";
    }
    else
    {
      RemainingTime(AutoLock());
      String toPrint;
      toPrint = remainingTime[0] ? (String(remainingTime[0]) + "h") : ("");
      toPrint += remainingTime[1] ? (String(remainingTime[1]) + "m") : ("");
      toPrint += remainingTime[2] ? (String(remainingTime[2]) + "s") : ("0s");
      while( toPrint.length() < 8)
      {
        toPrint = " " + toPrint;
      }
      toPrintLCD[3] = "Zaklep cez " + toPrint;
    }
    
    break;
    case CODE_CHANGE_MENU:
    LOG("", "LCDchangeMenu");
    int changeCodePos = 0;
    int changeCodeCorr = 0;
    char tempCode[4] = {0};
    while(_changeCodeFlag)
    {
      char changeCodeKey =  keypad.getKey();
      if( isDigit(changeCodeKey))
      {
        tempCode[changeCodePos++] = changeCodeKey;
        if( (changeCodePos >= 4) && (changeCodeCorr == 0))
        {
          for( int i = 0; i < 4; i++)
          {
            if( tempCode[i] != _passCode[i] )
            {
              _menuType = LOCKED_MENU;
              _lockStatus = LOCKED;
              return;
            }
          }
          changeCodePos = 0;
          changeCodeCorr = 1; // old pass was successfully entered
          tempCode[0],tempCode[1],tempCode[2],tempCode[3] = 0;
        }
        else if ( changeCodeCorr )
        {
          if( (changeCodePos >= 4) && (changeCodeCorr == 1))
          {
            for( int i = 0; i < 4; i++)
            {
              _passCode[i] = tempCode[i];
              _changeCodeFlag = 0;
              _menuType = UNLOCKED_MENU;
              _lockStatus = UNLOCKED;
            }
          }
        }
      }
      toPrintLCD[0] = "  SPREMEMBA GESLA   ";
      toPrintLCD[1] = "....................";
      toPrintLCD[2] = changeCodeCorr ? "Vnesi novo geslo    " : "Vnesi staro geslo   ";
      toPrintLCD[3] = "Geslo: ";
      for( int i = 0; i < 13; i++)
      {
        if( i < changeCodePos )
        {
          toPrintLCD[3] += "*";
        }
        else
        {
          toPrintLCD[3] += " ";
        }
      }
      printToLCD(toPrintLCD);
    }
    break;
    default:
    LOG("", "LCDdefault");
    break;
  }
  printToLCD(toPrintLCD);
}

void printToLCD(String toPrintLCD[4])
{
  for(int i = 0; i < 4; i++)
  {
    lcd.setCursor(0,i);
    lcd.print(toPrintLCD[i]);
  }
  if( millis() - _lastKeyPressTime > AUTO_DISPLAY_OFF_TIME)
  {
    lcd.noDisplay();
    lcd.noBacklight();
  }
  else
  {
    lcd.display();
    lcd.backlight();
  }
  
}

void RemainingTime(unsigned long millis)
{
  int inSeconds = millis / 1000;
  remainingTime[0] = inSeconds / 3600;
  remainingTime[1] = (inSeconds % 3600) / 60 ;
  remainingTime[2] = ((inSeconds % 3600) % 60);
}

void setCodeFromEEPROM()
{
  byte EEPROMcheck;
  EEPROM.get(0, EEPROMcheck);
  if( isDigit(EEPROMcheck) )
  {
    int address = 0;
    for( int i = 0; i < 4; i ++)
    {
      EEPROM.get(i, _passCode[i]);
    }
  }
}

void putCodeToEEPROM()
{
  
}