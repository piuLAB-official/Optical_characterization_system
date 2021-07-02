/*
 * LED Spectrometer test setup
 *
 * Control the test apparatus for the spectrum analysis of a LED
 * and the trasmission of the light trough a sample.
 *
 * The software is licended under GNU GPL v3.0 licence.
 * Developed by Andrea Mantelli.
 * Update 2021/04/12
 * Version: 1.0.3
 * 
*/

/*
 * Control of the shift register input have been taken from: https://playground.arduino.cc/Code/ShiftRegSN74HC165N/
 */

#include <Servo.h>
#include <LiquidCrystal_I2C.h>

// Create servo object to control a servo.
Servo myservo;

// use i2c scanner example sketch to search the address. Set the LCD address and insert the char and line numbers.
LiquidCrystal_I2C lcd(0x3F,16,2);

// Width of data (how many buttons used).
#define bytes_width 8

// Width of pulse to trigger the shift register to read and latch (usec).
#define pulse_delay 5

// Optional delay between shift register reads (msec).
#define reads_delay 10

// Create shift register bytes container.
#define BYTES_VAL_T unsigned int

BYTES_VAL_T pinValue;
BYTES_VAL_T oldpinValue;

// Shift register communication pin.
int ploadPin =   10;   // Connects to Parallel load pin of the shift register
int dataPin =    9;   // Connects to the Q7 pin of the shift register
int clockPin =    2;   // Connects to the Clock pin og the shift register

// LED control pin.
int relay_1 = 4;

// Servo control pin.
int out_6 = 6;

// Others pin available.
int in_2 = A2;
int in_3 = A3;
int relay_2 = 7;
int relay_3 = 8;
int relay_4 = 12;
int out_5 = 5;

// Variables to set the servo position.
const int start_pos = 1500;                           // start position 0° (1500 -> 0°) | boundaries [500->2500]
int pos = start_pos;                                  // variable to store the servo position
const int pos_step = 39;                              // microsecond needed to change position of 5°
const int pos_step_deg = 5;                           // step degrees value equivalent to pos_step in microseconds
const int pos_lim = pos_step*18;                      // maximum absolute angle respect to origin (0°)
int pos_deg = (pos-start_pos)/pos_step*pos_step_deg;  // position value in degree (°)

// Variables for interface control.
/*
 * 00[][][][][][] 10[][][][][][]
 * 01[][][][][][] 11[][][][][][]
 */
int lcd_row = 0;                  // lcd active row
int lcd_col = 0;                  // lcd active column
int lcd_pos = lcd_col+2*lcd_row;  // lcd active quadrant
unsigned long p_value = 0;        // pause value                             (keep it an unsigned long varaible)
unsigned long t_value = 1;        // test duration value                     (keep it an unsigned long varaible)
unsigned long value_ms = 0;       // pause or test duration in milliseconds  (keep it an unsigned long varaible)
int p_unit = 0;                   // 0: sec | 1: min | 2: msec
int t_unit = 0;                   // 0: sec | 1: min | 2: msec | 3: infinit
int p_digit = 1;                  // 0: first digit | 1: second digit | 2: third digit (p_value)
int t_digit = 1;                  // 0: first digit | 1: second digit | 2: third digit (t_value)
int t_status = 0;                 // 0: start | 1: testing

byte inf_1[] = {
  B00000,
  B00110,
  B01001,
  B10000,
  B10001,
  B01010,
  B00100,
  B00000,
};

byte inf_2[] = {
  B00000,
  B00100,
  B01010,
  B10001,
  B00001,
  B10010,
  B01100,
  B00000,
};

byte time_1[] = {
  B00000,
  B11101,
  B01001,
  B01001,
  B01001,
  B01001,
  B00000,
  B00000,
};

byte time_2[] = {
  B00000,
  B10001,
  B11011,
  B10101,
  B10001,
  B10001,
  B00000,
  B00000,
};

byte time_3[] = {
  B00000,
  B11100,
  B10000,
  B11000,
  B10000,
  B11100,
  B00000,
  B00000,
};


/* ################################################### */
void setup()
{
  //Serial.begin(9600);
  
  lcd.init();
  lcd.backlight();
  lcd.createChar(0,inf_1);
  lcd.createChar(1,inf_2);
  lcd.createChar(2,time_1);
  lcd.createChar(3,time_2);
  lcd.createChar(4,time_3);
  lcd.setCursor(0,0);
  lcd.print("LED Spectrometer");
  lcd.setCursor(0,1);
  lcd.print("   test setup   ");
  delay(2000);
  lcd.init();
  lcd.setCursor(0,0);
  lcd.print(">Angle  >Test");
  lcd.write(2);
  lcd.write(3);
  lcd.write(4);
  lcd.setCursor(0,1);
  lcd.print(">Pause  >Start");
  delay(3000);

  myservo.attach(out_6);  // attaches the servo on pin 9 to the servo object
  myservo.writeMicroseconds(start_pos);
  delay(500);
  
  pinMode(ploadPin,OUTPUT);
  pinMode(dataPin,INPUT);
  pinMode(clockPin,OUTPUT);
  pinMode(relay_1, OUTPUT);

  digitalWrite(relay_1, LOW);
  digitalWrite(clockPin, LOW);
  digitalWrite(ploadPin, HIGH);

  /* Read in and display the pin states at startup.
  */
  pinValue = read_shift_regs();
  oldpinValue = pinValue;

  t_status = 0;
  lcd_row = 0;
  lcd_col = 0;
  update_screen();
}
 
void loop()
{
  /* Read the state of all zones.
  */
  pinValue = read_shift_regs();
  //Serial.println(pinValue);

  /* If there was a chage in state, display which ones changed.
  */
  if(pinValue != oldpinValue) {
    switch (pinValue) {
      case 127:                                                         // decrease value key
        update_quadrant_value(-1);
        update_quadrant();
      break;
      case 191:                                                         // change digit key
        lcd_pos = lcd_col+2*lcd_row;
        switch (lcd_pos) {
          case 0:
          break;
          case 1:
            if (t_unit == 0 || t_unit == 1) {
              t_digit -= 1;
              if (t_digit < 0) {
                t_digit = 1;
              }
            }
            else if (t_unit == 2){
              t_digit -= 1;
              if (t_digit < 0) {
                t_digit = 2;
              }
            }
            else if (t_unit != 3) {
              error();
            }
          break;
          case 2:
            if (p_unit == 0 || p_unit == 1) {
              p_digit -= 1;
              if (p_digit < 0) {
                p_digit = 1;
              }
            }
            else if (p_unit == 2){
              p_digit -= 1;
              if (p_digit < 0) {
                p_digit = 2;
              }
            }
            else {
              error();
            }
          break;
          case 3:
          break;
        }
        update_quadrant();
      break;
      case 223:                                                         // increase value key
        update_quadrant_value(1);
        update_quadrant();
      break;
      case 251:                                                         // select key
        lcd_pos = lcd_col+2*lcd_row;
        switch (lcd_pos) {
          case 0:
          break;
          case 1:
            t_unit += 1;
            if (t_unit > 3) {
              t_unit = 0;
            }
            update_quadrant_value(0);
            update_quadrant();
          break;
          case 2:
            p_unit += 1;
            if (p_unit > 2) {
              p_unit = 0;
            }
            update_quadrant_value(0);
            update_quadrant();
          break;
          case 3:
            run_test();
            delay(100);
            update_screen();
          break;
        }
      break;
      case 247:                                                         // right arrow key
        if (lcd_col == 1) {
          break;
        }
        else if (lcd_col == 0) {
          lcd_col = 1;
          update_lcd_cursor();
        }
        else {
          error();
        }
        break;
      
      case 239:                                                         // up arrow key
        if (lcd_row == 0) {
          break;
        }
        else if (lcd_row == 1) {
          lcd_row = 0;
          update_lcd_cursor();
        }
        else {
          error();
        }
        break;
      case 254:                                                         // left arrow key
        if (lcd_col == 0) {
          break;
        }
        else if (lcd_col == 1) {
          lcd_col = 0;
          update_lcd_cursor();
        }
        else {
          error();
        }
        break;
      case 253:                                                         // down arrow key
        if (lcd_row == 1) {
          break;
        }
        else if (lcd_row == 0) {
          lcd_row = 1;
          update_lcd_cursor();
        }
        else {
          error();
        }
        break;
    }
    oldpinValue = pinValue;
  }
  delay(reads_delay);
}


/* ################################################### */

/* This function is essentially a "shift-in" routine reading the
 * serial Data from the shift register chips and representing
 * the state of those pins in an unsigned integer (or long).
*/
BYTES_VAL_T read_shift_regs()
{
  long bitVal;
  BYTES_VAL_T bytesVal = 0;
  
  /* Trigger a parallel Load to latch the state of the data lines,
  */
  digitalWrite(ploadPin, LOW);
  delayMicroseconds(pulse_delay);
  digitalWrite(ploadPin, HIGH);

  /* Loop to read each bit value from the serial out line
   * of the SN74HC165N.
  */
  for(int i = 0; i < bytes_width; i++) {
    bitVal = digitalRead(dataPin);

    /* Set the corresponding bit in bytesVal.
    */
    bytesVal |= (bitVal << ((bytes_width-1) - i));

    /* Pulse the Clock (rising edge shifts the next bit).
    */
    digitalWrite(clockPin, LOW);
    delayMicroseconds(pulse_delay);
    digitalWrite(clockPin, HIGH);
  }
  return(bytesVal);
}


/* ################################################### */

/* Update the lcd cursor position and active quadrant arrow.
*/
void update_lcd_cursor()
{
  lcd.setCursor(0,0);
  lcd.print(" ");
  lcd.setCursor(8,0);
  lcd.print(" ");
  lcd.setCursor(0,1);
  lcd.print(" ");
  lcd.setCursor(8,1);
  lcd.print(" ");
  lcd.setCursor(8*lcd_col, lcd_row);
  lcd.print(">");

  lcd_pos = lcd_col+2*lcd_row;
  switch (lcd_pos) {
    case 0:
      lcd.setCursor(6,0);
    break;
    case 1:
      if (t_unit == 0 || t_unit == 1) {
        lcd.setCursor(11+t_digit,0);
      }
      else if (t_unit == 2) {
        lcd.setCursor(11+t_digit,0);
      }
      else if (t_unit == 3) {
        lcd.setCursor(14,0);
      }
      else {
        error();
      }
    break;
    case 2:
      if (p_unit == 0 || p_unit == 1) {
        lcd.setCursor(3+p_digit,1);
      }
      else if (p_unit == 2) {
        lcd.setCursor(3+p_digit,1);
      }
      else {
        error();
      }
    break;
    case 3:
      lcd.setCursor(8,1);
    break;
  }
}


/* ################################################### */

/* Print and update pause or test values
*/
void update_run_value(unsigned long value, int unit)
{
  if (unit == 3) {
    lcd.setCursor(3,0);
    lcd.write(0);
    lcd.write(1);
  }
  else {
    if (value <= 999 && value > 99) {
      lcd.setCursor(2,0);
    }
    else if (value <= 99 && value > 9) {
      if (unit == 0 || unit == 1) {
        lcd.setCursor(2,0);
      }
      else {
        lcd.setCursor(3,0);
      }
    }
    else if (value <= 9) {
      if (unit == 0 || unit == 1) {
        lcd.setCursor(3,0);
      }
      else {
        lcd.setCursor(4,0);
      }
    }
    else {
      error();
    }
    lcd.print(value);
    switch (unit) {
      case 0:
       lcd.print("sec");
       value_ms = value*1000;
      break;
      case 1:
        lcd.print("min");
        value_ms = value*60*1000;
      break;
      case 2:
        lcd.print("ms");
        value_ms = value;
      break;
    }
  }
}
            


/* ################################################### */

/* Print error message
*/

void error()
{
  digitalWrite(relay_1, LOW);
  myservo.writeMicroseconds(start_pos);
  lcd.init();
  lcd.print("  ERROR!!");
  lcd.setCursor(0,1);
  lcd.print("  Reset...");
}


/* ################################################### */

/*  Update test settings screen
*/

void update_screen()
{
  lcd.init();       // initialize the lcd
  lcd.blink();
  lcd.setCursor(0,0);
  lcd.print(">A:");
  if (abs(pos_deg) <= 270 && abs(pos_deg) > 99) {
    if(pos_deg < 0) {
      lcd.setCursor(3,0);
    }
    else {
      lcd.setCursor(4,0);
    }
  }
  else if (abs(pos_deg) <= 99 && abs(pos_deg) > 9) {
    if(pos_deg < 0) {
      lcd.setCursor(4,0);
    }
    else {
      lcd.setCursor(5,0);
    }
  }
  else if (abs(pos_deg) <= 9) {
    if(pos_deg < 0) {
      lcd.setCursor(5,0);
    }
    else {
      lcd.setCursor(6,0);
    }
  }
  else {
    error();
  }
  lcd.print(pos_deg);
  lcd.print((char) 223);
  lcd.print(">T:");
  if (t_unit == 3) {
    lcd.setCursor(12,0);
    lcd.write(0);
    lcd.write(1);
  }
  else {
    if (t_value <= 999 && t_value > 99) {
      lcd.setCursor(11,0);
    }
    else if (t_value <= 99 && t_value > 9) {
      if (t_unit == 0 || t_unit == 1) {
        lcd.setCursor(11,0);
      }
      else {
        lcd.setCursor(12,0);
      }
    }
    else if (t_value <= 9) {
      if (t_unit == 0 || t_unit == 1) {
        lcd.setCursor(12,0);
      }
      else {
        lcd.setCursor(13,0);
      }
    }
    else {
      error();
    }
    lcd.print(t_value);
    switch (t_unit) {
      case 0:
       lcd.print("sec");
      break;
      case 1:
        lcd.print("min");
      break;
      case 2:
        lcd.print("ms");
      break;
    }
  }
  lcd.setCursor(0,1);
  lcd.print(">P:");
  if (p_value <= 999 && p_value > 99) {
    lcd.setCursor(3,1);
  }
  else if (p_value <= 99 && p_value > 9) {
    if (p_unit == 0 || p_unit == 1) {
      lcd.setCursor(3,1);
    }
    else {
      lcd.setCursor(4,1);
    }
  }
  else if (p_value <= 9) {
    if (p_unit == 0 || p_unit == 1) {
      lcd.setCursor(4,1);
    }
    else {
      lcd.setCursor(5,1);
    }
  }
  else {
    error();
  }
  lcd.print(p_value);
  switch (p_unit) {
    case 0:
     lcd.print("sec");
    break;
    case 1:
      lcd.print("min");
    break;
    case 2:
      lcd.print("ms");
    break;
  }
  lcd.print(">Start");
  update_lcd_cursor();
}


/* ################################################### */

/*  Update the active quadrant
*/

void update_quadrant()
{
  lcd_pos = lcd_col+2*lcd_row;
  switch (lcd_pos) {
    case 0:
      lcd.setCursor(3,0);
      lcd.print("    ");
      if (abs(pos_deg) <= 270 && abs(pos_deg) > 99) {
        if(pos_deg < 0) {
          lcd.setCursor(3,0);
        }
        else {
          lcd.setCursor(4,0);
        }
      }
      else if (abs(pos_deg) <= 99 && abs(pos_deg) > 9) {
        if(pos_deg < 0) {
          lcd.setCursor(4,0);
        }
        else {
          lcd.setCursor(5,0);
        }
      }
      else if (abs(pos_deg) <= 9) {
        if(pos_deg < 0) {
          lcd.setCursor(5,0);
        }
        else {
          lcd.setCursor(6,0);
        }
      }
      else {
        error();
      }
      lcd.print(pos_deg);
      lcd.setCursor(6,0);
    break;
    case 1:
      lcd.setCursor(11,0);
      lcd.print("     ");
      if (t_unit == 3) {
        lcd.setCursor(12,0);
        lcd.write(0);
        lcd.write(1);
      }
      else {
        if (t_value <= 999 && t_value > 99) {
          lcd.setCursor(11,0);
        }
        else if (t_value <= 99 && t_value > 9) {
          if (t_unit == 0 || t_unit == 1) {
            lcd.setCursor(11,0);
          }
          else {
            lcd.setCursor(12,0);
          }
        }
        else if (t_value <= 9) {
          if (t_unit == 0 || t_unit == 1) {
            lcd.setCursor(12,0);
          }
          else {
            lcd.setCursor(13,0);
          }
        }
        else {
          error();
        }
        lcd.print(t_value);
        switch (t_unit) {
          case 0:
           lcd.print("sec");
          break;
          case 1:
            lcd.print("min");
          break;
          case 2:
            lcd.print("ms");
          break;
        }
      }
      if (t_unit == 0 || t_unit == 1) {
        lcd.setCursor(11+t_digit,0);
      }
      else if (t_unit == 2) {
        lcd.setCursor(11+t_digit,0);
      }
      else if (t_unit == 3) {
        lcd.setCursor(14,0);
      }
      else {
        error();
      }
    break;
    case 2:
      lcd.setCursor(3,1);
      lcd.print("     ");
      if (p_value <= 999 && p_value > 99) {
        lcd.setCursor(3,1);
      }
      else if (p_value <= 99 && p_value > 9) {
        if (p_unit == 0 || p_unit == 1) {
          lcd.setCursor(3,1);
        }
        else {
          lcd.setCursor(4,1);
        }
      }
      else if (p_value <= 9) {
        if (p_unit == 0 || p_unit == 1) {
          lcd.setCursor(4,1);
        }
        else {
          lcd.setCursor(5,1);
        }
      }
      else {
        error();
      }
      lcd.print(p_value);
      switch (p_unit) {
        case 0:
         lcd.print("sec");
        break;
        case 1:
          lcd.print("min");
        break;
        case 2:
          lcd.print("ms");
        break;
      }
      if (p_unit == 0 || p_unit == 1) {
        lcd.setCursor(3+p_digit,1);
      }
      else if (p_unit == 2) {
        lcd.setCursor(3+p_digit,1);
      }
      else {
        error();
      }
    break;
    case 3:
    break;
  }
}


/* ################################################### */

/*  Update test settings of the active quadrant
*/

void update_quadrant_value(int sign)
{
  lcd_pos = lcd_col+2*lcd_row;
  switch (lcd_pos) {
    case 0:
      pos += sign*pos_step;
      if (pos < start_pos-pos_lim || pos > start_pos+pos_lim) {
        pos = start_pos+sign*pos_lim;
      }
      pos_deg = (pos-start_pos)/pos_step*pos_step_deg;
      myservo.writeMicroseconds(pos);
    break;
    case 1:
      switch (t_digit) {
        case 0:
          if (t_unit == 0 || t_unit == 1) {
            t_value += sign*10;
            if (t_value > 99) {
              t_value = 99;
            }
          }
          else {
            t_value += sign*100;
            if (t_value > 999) {
              t_value = 999;
            }
          }
        break;
        case 1:
          if (t_unit == 0 || t_unit == 1) {
            t_value += sign;
            if (t_value > 99) {
              t_value = 99;
            }
          }
          else {
            t_value += sign*10;
            if (t_value > 999) {
              t_value = 999;
            }
          }
        break;
        case 2:
          if (t_unit == 0 || t_unit == 1) {
            if (t_value > 99) {
              t_value = 1;
            }
            t_digit = 1;
          }
          else {
            t_value += sign;
            if (t_value > 999) {
              t_value = 999;
            }
          }
        break;
      }
    break;
    case 2:
      switch (p_digit) {
        case 0:
          if (p_unit == 0 || p_unit == 1) {
            p_value += sign*10;
            if (p_value > 99) {
              p_value = 99;
            }
          }
          else {
            p_value += sign*100;
            if (p_value > 999) {
              p_value = 999;
            }
          }
        break;
        case 1:
          if (p_unit == 0 || p_unit == 1) {
            p_value += sign;
            if (p_value > 99) {
              p_value = 99;
            }
          }
          else {
            p_value += sign*10;
            if (p_value > 999) {
              p_value = 999;
            }
          }
        break;
        case 2:
          if (p_unit == 0 || p_unit == 1) {
            if (p_value > 99) {
              p_value = 1;
            }
            p_digit = 1;
          }
          else {
            p_value += sign;
            if (p_value > 999) {
              p_value = 999;
            }
          }
        break;
      }
    break;
    case 3:
    break;
  }
}


/* ################################################### */

/*  run test
*/

void run_test() 
{
  if (t_status == 0) {
    lcd.init();
    lcd.blink();
    lcd.setCursor(0,0);
    lcd.print("P:");
    update_run_value(p_value, p_unit);
    lcd.print("->Waiting");
    lcd.setCursor(0,1);
    lcd.print("A:");
    if (abs(pos_deg) <= 270 && abs(pos_deg) > 99) {
      if(pos_deg < 0) {
        lcd.setCursor(2,1);
      }
      else {
        lcd.setCursor(3,1);
      }
    }
    else if (abs(pos_deg) <= 99 && abs(pos_deg) > 9) {
      if(pos_deg < 0) {
        lcd.setCursor(3,1);
      }
      else {
        lcd.setCursor(4,1);
      }
    }
    else if (abs(pos_deg) <= 9) {
      if(pos_deg < 0) {
        lcd.setCursor(4,1);
      }
      else {
        lcd.setCursor(5,1);
      }
    }
    else {
      error();
    }
    lcd.print(pos_deg);
    lcd.print((char) 223);
    lcd.print("->Stop?");
    lcd.setCursor(13,1);
    t_status = 1;
    int r = value_ms/reads_delay;
    int i = 0;
    
    while (t_status == 1 && i < r) {
      pinValue = read_shift_regs();
      if (pinValue == 251) {
        t_status = 0;
        break;
      }
      delay(reads_delay);
      i++;
    }

    if (t_status == 1) {
      digitalWrite(relay_1, HIGH);
      lcd.setCursor(0,0);
      lcd.print("T:     ");
      update_run_value(t_value, t_unit);
      lcd.print("->Testing");
      lcd.setCursor(13,1);
      r = value_ms/reads_delay;
      i = 0;
    }
    
    while (t_status == 1) {
      if (t_unit != 3) {
        while (i < r) {
          pinValue = read_shift_regs();
          if (pinValue == 251) {
            t_status = 0;
            digitalWrite(relay_1, LOW);
            break;
          }
          delay(reads_delay);
          i++;
        }
        t_status = 0;
        digitalWrite(relay_1, LOW);
        break;
      }
      else {
        pinValue = read_shift_regs();
        if (pinValue == 251) {
          t_status = 0;
          digitalWrite(relay_1, LOW);
          break;
        }
      }
      delay(reads_delay);
    }
    t_status = 0;
    digitalWrite(relay_1, LOW);
  }
}
