/*
 * Visteon Stereo Fiat Stilo & Bravo button remapper
 * 
 * Microcontroller sollution to remap Stilo front panel when connected to Bravo mainboard
 * and vice versa.
 *
 * Code designed for Adafruit Pro Micro. For others may require change of pin numbers
 * and PORT manipulations.
 * 
 * Below constants shows which ribbon pin should be connected to which Arduino pin.
 */

const int OUT_COL_6  =  3;  //PD1
const int OUT_COL_17 =  2;  //PD0
const int OUT_COL_19 =  1;  //PD2
const int OUT_COL_20 =  0;  //PD3

const int OUT_ROW_18 =  4; 
const int OUT_ROW_16 =  5;
const int OUT_ROW_7  =  6;
const int OUT_ROW_5  =  7;
const int OUT_ROW_2  =  8;

const int INP_COL_6  =  A0; //PF7
const int INP_COL_17 =  A1; //PF6
const int INP_COL_19 =  A2; //PF5
const int INP_COL_20 =  A3; //PF4

const int INP_ROW_2  =  10; //PB6
const int INP_ROW_5  =  9;  //PB5
const int INP_ROW_7  =  14; //PB3
const int INP_ROW_16 =  16; //PB2
const int INP_ROW_18 =  15; //PB1 

//front BRAVO, back STILO
const uint8_t KEYTABLE[20] = { 10, 14, 1, 15, 4, 13, 5, 6, 12, 9, 0, 11, 8, 2, 18, 3, 7, 16, 17, 19 };
//TODO: front STILO, back BRAVO
//const uint8_t KEYTABLE[20] = { );

void setup()
{
  //Columns scanned by Visteon CPU
  pinMode(OUT_COL_6,  INPUT_PULLUP);
  pinMode(OUT_COL_17, INPUT_PULLUP);
  pinMode(OUT_COL_19, INPUT_PULLUP);   
  pinMode(OUT_COL_20, INPUT_PULLUP);
  
  //Rows going to Visteon CPU
  pinMode(OUT_ROW_2,  OUTPUT); 
  pinMode(OUT_ROW_5,  OUTPUT); 
  pinMode(OUT_ROW_7,  OUTPUT); 
  pinMode(OUT_ROW_16, OUTPUT); 
  pinMode(OUT_ROW_18, OUTPUT); 

  //Visteon excepts HIGH as non-pressed state
  //(there are pullup resistors onboard)
  digitalWrite(OUT_ROW_2,  HIGH);
  digitalWrite(OUT_ROW_5,  HIGH);
  digitalWrite(OUT_ROW_7,  HIGH);
  digitalWrite(OUT_ROW_16, HIGH);
  digitalWrite(OUT_ROW_18, HIGH);
  
  //Columns of keypad scanned by Arduino
  pinMode(INP_COL_6,  OUTPUT);
  pinMode(INP_COL_17, OUTPUT);
  pinMode(INP_COL_19, OUTPUT);   
  pinMode(INP_COL_20, OUTPUT);
  
  //Rows returing to Arduino
  pinMode(INP_ROW_2,  INPUT_PULLUP); 
  pinMode(INP_ROW_5,  INPUT_PULLUP); 
  pinMode(INP_ROW_7,  INPUT_PULLUP); 
  pinMode(INP_ROW_16, INPUT_PULLUP); 
  pinMode(INP_ROW_18, INPUT_PULLUP); 

  //Default all columns to HIGH
  //Same as in Visteon case, I used internal pullups for non-pressed state
  digitalWrite(INP_COL_6,  HIGH);
  digitalWrite(INP_COL_17, HIGH);
  digitalWrite(INP_COL_19, HIGH);   
  digitalWrite(INP_COL_20, HIGH);
  
  //debug serial
  Serial.begin(115200);
}

/*
 * Matrix keyboard simulation
 */
class KeyOutput
{
  byte current_row = 1;
  bool scanning_active = 0;
  bool current_state = 0;

  uint8_t status = 15;
  uint8_t status_prev = 15;

  unsigned long lastScanTime = 0; 

  public: static uint32_t keyMap;

  protected: 
  /*
   * Simulate keypresses on current row
   */
  void WriteRow()
  {
    //dirty way to convert 4 bit index into decimal
    uint8_t rownum = 3 - ((status >> 1) - (status >> 3));

    //shift data by row number and invert bits for direct use on outputs.
    uint8_t data = ~keyMap >> rownum * 5;
    
    digitalWrite(OUT_ROW_18, data & B1); 
    digitalWrite(OUT_ROW_16, data >> 1 & B1); 
    digitalWrite(OUT_ROW_7,  data >> 2 & B1); 
    digitalWrite(OUT_ROW_5,  data >> 3 & B1); 
    digitalWrite(OUT_ROW_2,  data >> 4 & B1);
  }
  
  public: 
  void Run()
  {
    //get time
    unsigned long currentTime = micros();

    //read state
    status = ~PIND & B1111;

    //start scanning
    if( KeyOutput::keyMap && !scanning_active )
    {
      //random write so Visteon will start scanning keymap
      //it could be any OUT_ROW.
      digitalWrite(OUT_ROW_18, LOW);  
      scanning_active = 1; 
      return;
    }

    //clear scanning flag if there was no scan for full cycle.
    if(scanning_active && !KeyOutput::keyMap && (currentTime - lastScanTime >= 4000))
    {
      scanning_active = 0;
      return;      
    }

    /* 
     * Check if status changed to next valid status
     *  
     * I often got invalid reads, where two scanned lines were visible as active.
     * This bitwise magic returns non-zero when theres only one or zero bits active.
     * Zero is invalid too, so it's get rid by last [&& status]
     * 
     */
    if(KeyOutput::keyMap && (status^status_prev) && !((status - 1) & status) && status)
    { 
      scanning_active = 1;
      status_prev = status;
      lastScanTime = currentTime;
      this->WriteRow();
    }
  }
};

uint32_t KeyOutput::keyMap = 0;


/*
 * Matrix keyboard implementation
 */
class KeyInput
{
  uint8_t scanned_row = 3;
  unsigned long lastScanTime = 0; 
  
  uint32_t buttons = 0;
  uint32_t buttons_decoded = 0;
  
  bool parse_needed = 0;

  public:
  void parseKey()
  {
    uint8_t keycode = ~PINB &B1101110;
    /* decode into bit indexes
     * algorithm: 5 rows (keys) in column
     * each key will have index <rownum> + 5*<colnum>; row/col numbers starting from 0
     * eg. col 0, row 4: 4, col 2, row 1: 10, col 3, row 0:, 15
     */

    //read state
    uint8_t rindex = scanned_row * 5;

    //bits to save -> B1101110 
    bitWrite(buttons, rindex    , (keycode & B10));
    bitWrite(buttons, rindex + 1, (keycode & B100));
    bitWrite(buttons, rindex + 2, (keycode & B1000));
    bitWrite(buttons, rindex + 3, (keycode & B100000));
    bitWrite(buttons, rindex + 4, (keycode & B1000000));

    //write into buttons_decoded at position from KEYTABLE value of button
    bitWrite(buttons_decoded, KEYTABLE[rindex]    , buttons & ( 1UL << rindex));
    bitWrite(buttons_decoded, KEYTABLE[rindex + 1], buttons & ( 2UL << rindex));
    bitWrite(buttons_decoded, KEYTABLE[rindex + 2], buttons & ( 4UL << rindex));
    bitWrite(buttons_decoded, KEYTABLE[rindex + 3], buttons & ( 8UL << rindex));
    bitWrite(buttons_decoded, KEYTABLE[rindex + 4], buttons & (16UL << rindex));

    /*
    // some debug prints over serial
    if(buttons & ( 1UL << rindex)) Serial.println(rindex);
    if(buttons & ( 2UL << rindex)) Serial.println(rindex +1 );
    if(buttons & ( 4UL << rindex)) Serial.println(rindex +2 );
    if(buttons & ( 8UL << rindex)) Serial.println(rindex +3 );
    if(buttons & (16UL << rindex)) Serial.println(rindex +4 ); 
    if(keycode) Serial.println(rindex);
    if(buttons & ( 1UL << rindex)) Serial.println(KEYTABLE[rindex]);
    if(buttons & ( 2UL << rindex)) Serial.println(KEYTABLE[rindex + 1]);
    if(buttons & ( 4UL << rindex)) Serial.println(KEYTABLE[rindex + 2]);
    if(buttons & ( 8UL << rindex)) Serial.println(KEYTABLE[rindex + 3]);
    if(buttons & (16UL << rindex)) Serial.println(KEYTABLE[rindex + 4]); 
    if(keycode && buttons_decoded) Serial.println(buttons_decoded, BIN); //*/

    KeyOutput::keyMap = buttons_decoded;
  }

  void Run()
  {
    unsigned long currentTime = micros();

    //time to change 
    if(currentTime - lastScanTime >= 1000)
    {
      //update clock signal - 4 MSB bits of PORTF
      if(scanned_row == 3)  
        scanned_row = 0; //reset
      else
        scanned_row++;

      //we don't really care about 4 LSB of PORTF
      PORTF = B11101111 << scanned_row;
      
      //Serial.println(PORTF, BIN);
      lastScanTime = currentTime;
      parse_needed = 1;
      
      return; //skip decoding in run that updated signal
    }

    //parse key matrix at the middle of scan signal
    if(parse_needed && (currentTime - lastScanTime >= 500))
    {
      this->parseKey();
      parse_needed = 0;
    }
  }
  
};

KeyOutput ko;
KeyInput ki;

void loop()
{
  ko.Run();
  ki.Run();
}
