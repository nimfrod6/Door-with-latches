#include "LiquidCrystal_I2C.h"
#include "Keypad.h"

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
Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );
/******* END_KEYPAD ***************************/
/******* GLOBAL_VARIABLES *********************/
lockStatusType _lockStatus = LOCKED;

const unsigned long UNLOCK_ON_TIME = 3000;
const unsigned long UNLOCK_LATCHES_MAX_TIME = 10000;
const unsigned long AUTO_LOCK_TIME_RESET = 10000;      // resets unlock time back to 10 seconds
const unsigned long AUTO_LOCK_TIME_INCREMENT = 300000; // prolongs unlock time by 5 minutes
const unsigned long AUTO_LOCK_TIME_MAX = 7200000; // max 2 hours unlock time
unsigned long AUTO_LOCK_TIME = 10000;
unsigned long _lastUnlockTime = 0;


char _passCode[] = {'1', '8', '0', '9'};
char _enteredCode[4] = {0};

int _enteredCodePosition = 0;

bool _unlockFlag = 0;

/******* END_GLOBAL_VARIABLES *****************/
/******* USED_PINS ****************************/
int _latchGatePin = 12;
int _insideSwitchPin = 11;
/******* END_USED_PINS ************************/
/******* SETUP() ******************************/
void setup() {
  pinMode( _latchGatePin, OUTPUT );
  digitalWrite( _latchGatePin, 0 );
  pinMode( _insideSwitchPin, INPUT );
  Serial.begin(9600);
}
/******* END_SETUP() **************************/
/******* MAIN() *******************************/
void loop() {
  char key = keypad.getKey();
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
      }
      break;
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
  if( digitalRead(_insideSwitchPin) )
  {
    _unlockFlag = 1;
  }
  UnlockLatches();
  digitalWrite( 13, _lockStatus);
  AutoLock();
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
  static unsigned long startTime = 0;
  
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
  digitalWrite(_latchGatePin, (condition && !onCooldown));
}

void AutoLock() // locks the door after certain time has passed
{
  if( millis() - _lastUnlockTime > AUTO_LOCK_TIME )
  {
    _lockStatus = LOCKED;
    AUTO_LOCK_TIME = AUTO_LOCK_TIME_RESET;
  }
}

void debug()
{
  Serial.print("_lockStatus= " + String(_lockStatus) + "\t");
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