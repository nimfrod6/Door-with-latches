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

char _passCode[] = {'1', '8', '0', '9'};
char _enteredCode[4] = {0};

int _enteredCodePosition = 0;

bool _unlockFlag = 0;

/******* END_GLOBAL_VARIABLES *****************/
/******* USED_PINS ****************************/
int _latchGatePin = A0;
/******* END_USED_PINS ************************/
/******* SETUP() ******************************/
void setup() {
  pinMode( _latchGatePin, OUTPUT );
  digitalWrite( _latchGatePin, 0 );
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
        break;
        case 'B':
        break;
        case 'C':
        _lockStatus = LOCKED;
        ResetCode();
        break;
        case 'D':
        break;
      }
      break;
    }
  }
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

void UnlockLatches() // left off here
{
  static double long startTime = 0;
  if( _unlockFlag == 1 )
  {
    startTime = millis();
    _unlockFlag = 0;
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
  Serial.println("");
}