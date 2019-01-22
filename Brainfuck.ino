#include <EEPROM.h>
#include <LiquidCrystal.h>

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

//Constants
#define DEBUG 1

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

#define PRG_SIZE 128        //Size of memory for code
#define MEM_SIZE 128        //Size of memory for memory :-)
#define STACK_SIZE 32       //Size of stack for BF interpreter

#define BUTTON_DEBOUNCE 500
#define LONG_DELAY 3000 

#define MEM_DISPLAY_SIZE 4 

const byte MODE_MENU = 0;
const byte MODE_EDIT = 1;
const byte MODE_EXEC = 2;
const byte MODE_DBUG = 3;
const byte MODE_SAVE = 4;
const byte MODE_LOAD = 5;

const byte DEBUG_MEMORY = 0;
const byte DEBUG_CODE = 1;
const byte DEBUG_OUTPUT = 2;

byte symbolHeart[8] = {
  B00000,
  B01010,
  B11111,
  B11111,
  B01110,
  B00100,
  B00000,
  B00000,
};

byte symbolNone[8] = {
  B11111,
  B00000,
  B11111,
  B00000,
  B11111,
  B00000,
  B11111,
  B00000,
};

const char* menuItems[5] = {"1. Edit", "2. Execute", "3. Step Execute", "4. Save", "5. Load"};

const char ops[] = {'\0', '>', '<', '+', '-', '.', ',', '[', ']'};

//Variables
int lcd_key     = 0;
int lastButton  = btnNONE;
int adc_key_in  = 0;
int lastButtonSec = 0;
unsigned long lastButtonMillis = 0;

byte mode = 0;
int menuItem = 0;

//Brainfuck
char prg[PRG_SIZE] = {0};   //BF program
short ic;                    //Current position in ops array
short pc;                   //Current position in prg array

//Debugger
short  debugScreen = 0;              // debug screen mode
short memScreenPos = 0;
short prgScreenPos = 0;

byte isDebug = 0;

void setup()
{
  #ifdef DEBUG
  Serial.begin(9600);
  #endif
  startLCD();
}
 
void loop()
{
    switch(mode)
    {
        case MODE_MENU:
        {
          processMenu();
          break;
        }
        case MODE_EDIT:
        {
          processEdit();
          break;
        }
        case MODE_EXEC:
        {
          isDebug = 0;
          processExecution();
          break;
        }
        case MODE_DBUG:
        {
          isDebug = 1;
          processExecution();
          break;
        }
        case MODE_SAVE:
        {
          processSave();
          break;
        }
        case MODE_LOAD:
        {
          processLoad();
          break;
        }
    }
}

void processMenu()
{
    lcd.setCursor(0,0);
    lcd.print(F("I "));
    lcd.write(byte(1));
    lcd.print(F(" Brainfuck!"));

    if(readKeyboard())
    {
        switch(lastButton)
        {
            case btnLEFT:
            {
              menuItem--;
              if (menuItem < 0)
              {
                menuItem = 4;
              }
              break;
            }
            case btnRIGHT:
            {
              menuItem++;
              if (menuItem > 4)
              {
                menuItem = 0;
              }
              break;
            }
            case btnSELECT:
            {
              lcd.clear();
              mode = menuItem + 1;
              break;
            }
        }
    }

    lcdPrintString(1, menuItems[menuItem]);
}

void processEdit()
{
    lcd.setCursor(0,0);
    //lcd.print("            ");
    lcd.setCursor(12, 0);   //
    char buf[4];            //
    sprintf(buf, "%4d", pc);//
    lcd.print(buf);         //Print current position in program

    if(readKeyboard())
    {
        switch(lastButton)
        {
            case btnLEFT:
            {
              if(pc > 0)
              {
                  pc--;
                  ic = getCommandIndex(prg[pc], ic);
              }
              break;
            }
            case btnRIGHT:
            {
              if(pc < PRG_SIZE - 1)
              {
                  pc++;
                  ic = getCommandIndex(prg[pc], ic);
              }
              break;
            }
            case btnDOWN:
            {
              ic++;
              if(ic > 8)
              {
                  ic = 0;
              }
              prg[pc] = ops[ic];
              break;
            }
            case btnUP:
            {
              ic--;
              if(ic < 0)
              {
                  ic = 8;
              }
              prg[pc] = ops[ic];
              break;
            }
            case btnSELECT:
            {
              lcd.clear();
              mode = 0;
              return;
            }
        }
    }
    
    lcd.setCursor(0, 1);
    for(byte i = 0; i < 16 && (pc + i) < PRG_SIZE; i++)
    {
      lcd.print(prg[pc + i]);
    }
      
}

int getCommandIndex(char symbol, char defaultIndex)
{ 
  int el_size = sizeof(symbol);
  int ar_size = sizeof(ops);
  int len = ar_size/el_size;
  for(int i = 0; i < len; i++)
  {
    if(ops[i] == symbol)
    {
      return i;
    }
  }
  
  return defaultIndex;
}


void processExecution()
{
  memScreenPos = 0;
  prgScreenPos = 0;
  debugScreen  = 0;

  char mem[MEM_SIZE] = {0};     //init memory array
  short stack[STACK_SIZE] = {0};//init stack array
  pc = 0;                       //program position to start
  short memPos = 0;             //memory position
  byte stackPos = 0;            //stack position 
  byte outputPos = 0;           //output position
  char output[16] = {0};        //output buffer 1 line

  lcd.clear();
  
  #ifdef DEBUG
        Serial.print(F("isDebug "));
        Serial.println(isDebug);
  #endif 
  
  while(prg[pc])
  {
    #ifdef DEBUG
        Serial.print(F("step "));
        Serial.println(pc);
    #endif 
    
    
    if (isDebug)
    {
        processDebug(mem, memPos, pc, output, outputPos);
    }

    switch(prg[pc])
    {
      case '>':
        memPos++;  
        break;
         
      case '<':
        memPos--;
        break;
 
      case '+':
        mem[memPos]++;
        break;
       
      case '-':
        mem[memPos]--;
        break;
 
      case '.':

        output[outputPos] = mem[memPos];
        outputPos++;
        if (outputPos >= sizeof(output))
        {
          outputPos = 0;
        }

        #ifdef DEBUG
          Serial.print(F("output: "));
          Serial.println(output);
        #endif 

        break;
 
      case ',':
        mem[memPos] = readByteFromKeyboard();
        break;
      
      case '[':
        if(mem[memPos])
        {
          stackPos++;
          stack[stackPos] = pc;
        }
        else
        {
          while(prg[pc] != ']')
          {
            pc++;
          }
        }
        break;
 
      case ']':
        if(mem[memPos])
          pc = stack[stackPos];
        else
          stackPos--;
        break;
    }

    pc++;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("DONE. Output:"));
  lcd.setCursor(0, 1);
  lcd.print(output);

  #ifdef DEBUG
        Serial.println("Execution completed");
  #endif 
  
  mode = 0;
  pc = 0;
  delay(LONG_DELAY);

  byte isWaiting = true;

  while (isWaiting)
  {
    if (readKeyboard())
    {
      isWaiting = false;
    }
    delay(50);
  }

  lcd.clear();
}


void processDebug(char mem[], short memPos, short pc, char output[], byte outputPos)
{
  byte isCanStep = 0;
  while(isCanStep == 0)
  {
    switch (debugScreen)
    {
      case DEBUG_MEMORY:
      {
        isCanStep = processDebugMemory(mem, memPos, pc, output, outputPos);
        break;
      }
      case DEBUG_CODE:
      {
        isCanStep = processDebugCode(mem, memPos, pc, output, outputPos);
        break;
      }
      case DEBUG_OUTPUT:
      {
        isCanStep = processDebugOutput(mem, memPos, pc, output, outputPos);
        break;
      }
    }
  }

  isCanStep = 0;
}

byte processDebugCode(char mem[], short memPos, short pc, char output[], byte outputPos)
{
  lcd.setCursor(0,0);

  for (short i = prgScreenPos; i <= prgScreenPos + 16; i++)
  {
    lcd.print(prg[i]);
  }

  lcd.setCursor(0,1);
  for (short i = 0; i < 16; i++)
  {
    short curPrgPos = i + prgScreenPos;
    if (curPrgPos == pc)
    {
      lcd.print(F("^"));
    }
    else
    {
      lcd.print(F(" "));
    }
  }

  byte result = 0;

  if(readKeyboard())
    {
      switch(lastButton)
      {
          case btnLEFT:
          {
            prgScreenPos--;
            if (prgScreenPos <= 0)
            {
              prgScreenPos = 0;
            }
            break;
          }
          case btnRIGHT:
          {
            prgScreenPos++;
            if (prgScreenPos >= PRG_SIZE)
            {
              prgScreenPos = PRG_SIZE - 1;
            }
            break;
          }
          case btnUP:
          {
            debugScreen--;
            if (debugScreen < DEBUG_MEMORY)
            {
              debugScreen = DEBUG_OUTPUT;
            }
            break;
          }
          case btnDOWN:
          {
            debugScreen++;
            if (debugScreen > DEBUG_OUTPUT)
            {
              debugScreen = DEBUG_MEMORY;
            }
            break;
          }
          case btnSELECT:
          {
            result = 1;
            break;
          }
      }
    }

    return result;
}

byte processDebugOutput(char mem[], short memPos, short pc, char output[], byte outputPos)
{
  lcdPrintString(0,"Output:");

  lcd.setCursor(0,1);
  for (short i = 0; i < 16; i++)
  {
    if (i < outputPos)
    {
      lcd.print(output[i]);
    }
    else
    {
      lcd.print(F(" "));
    }
  }

  byte result = 0;

  if(readKeyboard())
    {
      switch(lastButton)
      {
          case btnLEFT:
          {
            break;
          }
          case btnRIGHT:
          {
            break;
          }
          case btnUP:
          {
            debugScreen--;
            if (debugScreen < DEBUG_MEMORY)
            {
              debugScreen = DEBUG_OUTPUT;
            }
            break;
          }
          case btnDOWN:
          {
            debugScreen++;
            if (debugScreen > DEBUG_OUTPUT)
            {
              debugScreen = DEBUG_MEMORY;
            }
            break;
          }
          case btnSELECT:
          {
            result = 1;
            break;
          }
      }
    }

    return result;
}

byte processDebugMemory(char mem[], short memPos, short pc, char output[], byte outputPos)
{
  lcd.setCursor(0,0);

  for (short i = memScreenPos; i <= memScreenPos + (4*MEM_DISPLAY_SIZE); i+=MEM_DISPLAY_SIZE)
  {
    char buf[3] = {0};
    sprintf(buf, "%03d", mem[i/MEM_DISPLAY_SIZE]);
    lcd.print(buf);
    lcd.print(F(" "));
  }

  for (short i = 0; i <= 4; i++)
  {
    lcd.setCursor(i * MEM_DISPLAY_SIZE, 1);

    short curMemPos = i + (memScreenPos / MEM_DISPLAY_SIZE);
    if (curMemPos == memPos)
    {
      lcd.print(F(" ^  "));
    }
    else
    {
      char buf[3] = {0};
      sprintf(buf, "%03d", curMemPos);
      lcd.print(buf);
      lcd.print(F(" "));
    }
  }

  byte result = 0;

  if(readKeyboard())
    {
      switch(lastButton)
      {
          case btnLEFT:
          {
            memScreenPos -= MEM_DISPLAY_SIZE;
            if (memScreenPos <= 0)
            {
              memScreenPos = 0;
            }
            break;
          }
          case btnRIGHT:
          {
            memScreenPos += MEM_DISPLAY_SIZE;
            if (memScreenPos > (MEM_SIZE * MEM_DISPLAY_SIZE) - MEM_DISPLAY_SIZE)
            {
              memScreenPos = (MEM_SIZE * MEM_DISPLAY_SIZE) - MEM_DISPLAY_SIZE;
            }
            break;
          }
          case btnUP:
          {
            debugScreen--;
            if (debugScreen < DEBUG_MEMORY)
            {
              debugScreen = DEBUG_OUTPUT;
            }
            break;
          }
          case btnDOWN:
          {
            debugScreen++;
            if (debugScreen > DEBUG_OUTPUT)
            {
              debugScreen = DEBUG_MEMORY;
            }
            break;
          }
          case btnSELECT:
          {
            result = 1;
            break;
          }
      }
    }

    return result;
}

void processSave()
{
  lcd.setCursor(0, 0);
  
  short i = 0;
  while(prg[i] && i < PRG_SIZE - 1)
  {
    EEPROM.update(i, prg[i]);
    i++;
  }
  EEPROM.update(i, '\0');
  lcd.print(F("Stored in EEPROM"));
  
  mode = 0;
  delay(LONG_DELAY);
  lcd.clear();
}

void processLoad()
{
  lcd.setCursor(0, 0);
  
  short i = -1;
  do
  {
    i++;
    prg[i] = EEPROM[i];
  } while (prg[i] && i < PRG_SIZE);
  
  lcd.print(F("Loaded from EEPROM"));
  
  mode = 0;
  delay(LONG_DELAY);
  lcd.clear();
}

byte readByteFromKeyboard()
{
  byte result = 0;
  byte isInput = true;

  lcd.clear();

  while(isInput)
  {
    lcd.setCursor(0, 0);
    lcd.print(F("Input:"));
    lcd.print(result);
    lcd.print(F("  "));

    if(readKeyboard())
    {
        switch(lastButton)
        {
            case btnLEFT:
            {
              result--;
              break;
            }
            case btnRIGHT:
            {
              result++;
              break;
            }
            case btnUP:
            {
              result+=10;
              break;
            }
            case btnDOWN:
            {
              result-=10;
              break;
            }
            case btnSELECT:
            {
              isInput = false;
              break;
            }
        }
    }
  }

  lcd.clear();
  return result;
}


void startLCD()
{
  lcd.createChar(byte(0), symbolNone);
  lcd.createChar(byte(1), symbolHeart);
  lcd.begin(16, 2);              // start the library
}

byte readKeyboard()
{
  unsigned long newMillis = millis();

  lcd_key = read_LCD_buttons();  // read the buttons

  if (lcd_key != btnNONE)
  {
    lastButtonMillis = newMillis;
    lastButtonSec = lastButtonMillis / 10;
    lastButton = lcd_key;
    return true;
  }

  return false;
}

// read the buttons
int read_LCD_buttons()
{

  unsigned long newMillis = millis();

  if ((newMillis - lastButtonMillis) < BUTTON_DEBOUNCE)
  {
    return btnNONE;
  }

 adc_key_in = analogRead(0);      // read the value from the sensor 
 
 // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
 // we add approx 50 to those values and check to see if we are close
 if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
 // For V1.1 us this threshold

 // For V1.0 comment the other threshold and use the one below:

 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 195)  return btnUP; 
 if (adc_key_in < 380)  return btnDOWN; 
 if (adc_key_in < 555)  return btnLEFT; 
 if (adc_key_in < 790)  return btnSELECT;   

 return btnNONE;  // when all others fail, return this...
}

void lcdPrintString(int line, const char str[])
{
    lcd.setCursor(0, line);
    lcd.print(str);
    lcd.print(F("                "));
}









