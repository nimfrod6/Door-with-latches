#include "LiquidCrystal_I2C.h"
#include "Keypad.h"
#include "Wire.h"
#include "CapacitiveSensor.h"

typedef enum{
  LOCKED = 0,
  UNLOCKED
} lockStatusType;

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

bool _unlockFlag = 0;
bool _lcdWrongCode = 0;

/******* END_GLOBAL_VARIABLES *****************/
/******* USED_PINS ****************************/
int _latchGatePin = 12;
int _insideSwitchPin = 11;
CapacitiveSensor   _insideSwitchSensor = CapacitiveSensor(10,_insideSwitchPin);
/******* END_USED_PINS ************************/
/******* SETUP() ******************************/
void setup() {
  pinMode( _latchGatePin, OUTPUT );
  digitalWrite( _latchGatePin, 0 );
  _insideSwitchSensor.set_CS_AutocaL_Millis(0xFFFFFFFF);
  Serial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  VHOD V REZIDENCO  ");
  lcd.setCursor(0,1);
  lcd.print("-----ZAKLENJENO-----");
  lcd.setCursor(0,3);
  lcd.print("Geslo: ");
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
      break;
      }
    }
    case UNLOCKED:
    {
      switch(key)
      {
        case 'A':
        _unlockFlag = 1;
        break;
        case 'B':
        break;
        case 'C':
        _lockStatus = LOCKED;
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
    }
    ISSoffTime = millis();
  }
  else
  {
    ISSonTime = millis();
  }
  
  
  UnlockLatches();
  digitalWrite( 13, _lockStatus);
  AutoLock();
  printLCD();
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
      _lcdWrongCode = 1;
      return;
    }
  }
  attempts = 0;
  _unlockFlag = 1;
  _lastUnlockTime = millis();
  _lockStatus = UNLOCKED;
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
  return PowerLimit(timeCondition);
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
  unsigned long remainingTime = ((millis() - _lastUnlockTime) > AUTO_LOCK_TIME)? 0 : (AUTO_LOCK_TIME - (millis() - _lastUnlockTime));
  Serial.print("timeToLock: " + String(remainingTime) + "\t");
  Serial.println("");
}

void LOG(String parameter, String parameterName)
{
  Serial.print(parameterName + "= " + parameter + "\t");
}

void printLCD()
{
  static bool _lockStatusToggle = 0;
  static bool cleared = 0;
  static int _enteredCodePositionPrev = 0;
  static double long statusChangeTime = 0;

  if( _lockStatus == LOCKED )
  {
    if(_lockStatusToggle != _lockStatus)
    {
      statusChangeTime = millis();
      lcd.setCursor(0, 0);
      lcd.print("  VHOD V REZIDENCO  ");
      lcd.setCursor(0,1);
      lcd.print("-----ZAKLENJENO-----");
      lcd.setCursor(0,3);
      lcd.print("Geslo:              ");
    }
    if( _lcdWrongCode )
    {
      _lcdWrongCode = 0;
      lcd.setCursor(0,3);
      lcd.print("Geslo:              ");
    }
    lcd.setCursor(7,3);
    for( int i = 0; i < 4; i++)
    {
      if( _enteredCodePosition > i) 
      {
        lcd.print("*");
      }
      else
      {
        lcd.print(" ");
      }
      
    }

  }
  else
  {
    if(_lockStatusToggle != _lockStatus)
    {
      cleared = 0;
      statusChangeTime = millis();
      lcd.setCursor(0, 0);
      lcd.print("  VHOD V REZIDENCO  ");
      lcd.setCursor(0,1);
      lcd.print("     ODKLENJENO     ");
      lcd.setCursor(0,3);
      lcd.print("Geslo: ****");
    }
    if( (millis() - statusChangeTime > 1500) )
    {
      if( !cleared )
      {
        lcd.setCursor(0,3);
        lcd.print("           ");
        cleared = 1;
      }
      lcd.setCursor(0,3);
      lcd.print("Zaklep cez "); // 9 characters remain for time XhXXmXXs
      RemainingTime(AutoLock());
      String toPrint;
      toPrint = remainingTime[0] ? (String(remainingTime[0]) + "h") : ("");
      toPrint += remainingTime[1] ? (String(remainingTime[1]) + "m") : ("");
      toPrint += remainingTime[2] ? (String(remainingTime[2]) + "s") : ("0s");
      while( toPrint.length() < 8)
      {
        toPrint = " " + toPrint;
      }
      lcd.print(toPrint);
    }
  }
  
  if( millis() - _lastKeyPressTime > AUTO_DISPLAY_OFF_TIME)
  {
    lcd.noBacklight();
    lcd.noDisplay();
  }
  else
  {
    lcd.backlight();
    lcd.display();
  }
  _lockStatusToggle = _lockStatus; 
  
}


void RemainingTime(unsigned long millis)
{
  int inSeconds = millis / 1000;
  remainingTime[0] = inSeconds / 3600;
  remainingTime[1] = (inSeconds % 3600) / 60 ;
  remainingTime[2] = ((inSeconds % 3600) % 60);
}