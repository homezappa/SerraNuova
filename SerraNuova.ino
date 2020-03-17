/*
Serra Riscaldata e Illuminata 1.0 
  
@author:  Andrea Zappaterra (thanks to Daniele Tartaglia & Mauro Alfieri)
@site:    www.casazappa.it
  
@Licence: CC Attribution Noncommercial Share Alike
@link:    https://creativecommons.org/licenses/by-nc-sa/3.0/it/
*/
  
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_PCF8574.h>
#include <SimpleDHT.h>
#include <Keypad.h>
#include <EEPROM.h>

/*********************************************************************/
#define MY_TAG      30546  
#define PIN_DHT11   10

#define NOT_USED    -1
/*
 * Defines for pins driving releais. Analogic or digital, don't care
 * Define only connected ones ! (the others set to NOT_USED)
 */
#define PIN_Relais0   A1
#define PIN_Relais1   A2
#define PIN_Relais2   A3
#define PIN_Relais3   NOT_USED
#define PIN_Relais4   NOT_USED
#define PIN_Relais5   NOT_USED
#define PIN_Relais6   NOT_USED
#define PIN_Relais7   NOT_USED

// STATUS and booleans

#define LOOPING     0
#define IN_MENU     1

#define MANUAL      1
#define AUTO        2

#define ON          0
#define OFF         1

#define BACKLIGHT_ON  1
#define BACKLIGHT_OFF 0

/************ VARS *************/

/*
 * ActionType:  0 = do nothing
 *              1 = ON when Temperature below Data[0], Data[1] is sign: 0=positive, 1=negative
 *              2 = ON when Temperature above Data[0], Data[1] is sign: 0=positive, 1=negative
 *              3 = ON when Humidity below Data[0]
 *              4 = ON when Humidity above Data[0]
 *              5 = ON when time (HH:MM) between Data[0]:Data[1] and Data[2]:Data[3]
 *              6 = ON every Data[0] minutes (max 120) with duration Data[1] minutes (max 120) starting at Data[2]:Data[3]
 *              7 = ON every Data[0] hours (max 24) with duration Data[1] minutes (max 120) starting at Data[2]:Data[3]
 *              8 = ON every Data[0] hours (max 24) with duration Data[1] minutes (max 120) starting at Data[2]:Data[3] only if day of week is on in Data[4] (bitmask)
 */

struct ReleStruct {
  int IO_PORT;
  int ActionType;
  byte CurrentStatus;
  byte Data[5];
};
struct Config {
  int  TAG;           // integer to know if I saved before on eeprom
  byte STATUS;        // RUNNING, IN_MENU, etc ...
  byte MODE;          // MANUAL, AUTO
  ReleStruct Relais[8];
};

int PIN_Relais[8] = { PIN_Relais0, PIN_Relais1, PIN_Relais2, PIN_Relais3, PIN_Relais4, PIN_Relais5, PIN_Relais6, PIN_Relais7 };

const byte ROWS = 4; 
const byte COLS = 4; 

char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2}; 

/*********************************************************************/
 
char lettera;
char buffer[16];  
  
int previousSec = 0;
  
/*********************************************************************/

int STUFA[8] = {0x0E, 0x11, 0x1F, 0x11, 0x1F, 0x11, 0x1F, 0x11,};
int LUCE[8] = {0x0E, 0x1F, 0x1F, 0x1F, 0x0E, 0x0E, 0x0E, 0x04,};
int GRADI[8] = {0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00,};
  
/*********************************************************************/
// Variabili lettura temperatura DHT11
byte temperature    = 0;
byte humidity       = 0;
byte humidity2      = 0;
byte temperature2   = 0;
byte data[40]       = {0};

byte STATUS         = LOOPING;
byte STATO_STUFA    = OFF;
byte STATO_LUCE     = OFF;
byte STATO_LUCE_2     = OFF;
byte MANUALE        = OFF;
byte BACKLIGHT      = OFF;
byte TEMP_ST        = OFF;

byte OraStart       = 8;
byte MinutiStart    = 0;
byte OraStop        = 18;
byte MinutiStop     = 0;
byte OraStart2       = 17;
byte MinutiStart2    = 0;
byte OraStop2        = 6;
byte MinutiStop2     = 0;


byte TempLimit      = 0;
bool Negative       = false;

String DatoDigitato;


DateTime            now;
Config              MyConfig;

/*********************************************************************/
  
RTC_DS1307              RTC;
LiquidCrystal_PCF8574   lcd(0x27);
Keypad                  customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 
SimpleDHT11             dht11;
  


void setup() {
  int error;

  Serial.begin(9600);
  // wait on Serial to be available on Leonardo
  while (!Serial)
    ;
  Serial.println("Read Config");
  
  EEPROM.get(0, MyConfig);
  if( ! MyConfig.TAG == MY_TAG ) {        // if TAG not equal to mine, set defaults and write
    Serial.println("No memory: set defaults");
    MyConfig.TAG      = MY_TAG;
    MyConfig.STATUS   = LOOPING;
    MyConfig.MODE     = AUTO;
    for(int i=0;i<8;i++) {
      MyConfig.Relais[i].ActionType = 0;
      MyConfig.Relais[i].CurrentStatus = OFF;
      MyConfig.Relais[i].IO_PORT = PIN_Relais[i];
      for (int j=0;j<5;j++) {
        MyConfig.Relais[i].Data[j] = 0;
      }
    }
    EEPROM.put(0, MyConfig);
  } else {
    Serial.println("Got values from memory!");
  }
  
  Serial.println("check for LCD");
  // See http://playground.arduino.cc/Main/I2cScanner how to test for a I2C device.
  Wire.begin();
  Wire.beginTransmission(0x27);
  error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.print(error);
  if (error == 0) {
    Serial.println(": LCD found.");
    lcd.begin(16, 2); // initialize the lcd
  } else {
    Serial.println(": LCD not found.");
  }
  lcd.init();
  lcd.setBacklight(BACKLIGHT_ON); 
  lcd.createChar(0, STUFA);
  lcd.createChar(1, LUCE);
  lcd.createChar(2, LUCE);
  lcd.createChar(3, GRADI);
  
  RTC.begin();
  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
//   RTC.adjust(DateTime(__DATE__, __TIME__));

/* Rele */      
  pinMode(rele1, OUTPUT);
  pinMode(rele2, OUTPUT);
  pinMode(rele3, OUTPUT);
    
  digitalWrite(rele1, HIGH);
  digitalWrite(rele2, HIGH);
  digitalWrite(rele3, HIGH);
/* Valori iniziali e splash screen */     

  
  MANUALE     = MyConfig.MANUALE;
  STATO_LUCE  = MyConfig.STATO_LUCE;
  STATO_LUCE_2  = MyConfig.STATO_LUCE_2;
  STATO_STUFA = MyConfig.STATO_STUFA;
  Negative    = MyConfig.Negative;
  TempLimit   = MyConfig.TempLimit;
  OraStart    = MyConfig.OraStart;
  MinutiStart = MyConfig.MinutiStart;
  OraStop     = MyConfig.OraStop;
  MinutiStop  = MyConfig.MinutiStop;
  OraStart2    = MyConfig.OraStart2;
  MinutiStart2 = MyConfig.MinutiStart2;
  OraStop2     = MyConfig.OraStop2;
  MinutiStop2  = MyConfig.MinutiStop2;
  
  PrintSplash();  
  PrintHead();
  if (MANUALE==ON) {
    DriveLight(STATO_LUCE);
    DriveLight2(STATO_LUCE);
    DriveHeat(STATO_STUFA);
  }
  DisplayIcons();
}
/*********************************************************************/
   
void loop() {
  lettera = customKeypad.getKey();
  now = RTC.now();
  switch(STATUS) {
    case LOOPING:
        // Riceve comandi tastiera
        if(lettera) { 
          switch (lettera) {
            case '1':
                if (MANUALE==ON) {
                  ToggleHeat();
                  MyConfig.STATO_STUFA=STATO_STUFA;
                  EEPROM.put(0, MyConfig);
                }
              break;
              
            case '2':
                if (MANUALE==ON) {
                  ToggleLight();
                  MyConfig.STATO_LUCE=STATO_LUCE;
                  EEPROM.put(0, MyConfig);
                }
              break;

            case '3':
                if (MANUALE==ON) {
                  ToggleLight2();
                  MyConfig.STATO_LUCE_2=STATO_LUCE_2;
                  EEPROM.put(0, MyConfig);
                }
              break;

            case 'A':
                if(MANUALE == OFF) {
                  MANUALE = ON;
                }
                else {
                  MANUALE = OFF;
                }
                DisplayIcons();
                MyConfig.MANUALE=MANUALE;
                EEPROM.put(0, MyConfig);
              break;
          
            case 'D':
                lcd.clear();
                lcd.setBacklight(BACKLIGHT_ON); 
                STATUS=IN_MENU;
            break;

            case '*':
                if (BACKLIGHT == OFF) {
                   lcd.setBacklight(BACKLIGHT_ON);
                   BACKLIGHT=ON;
                } else {
                   lcd.setBacklight(BACKLIGHT_OFF);
                   BACKLIGHT=OFF;
                }
            break;

        }
    }
    if(now.second() != previousSec) {
        lcd.setCursor( 0,0 );
        sprintf(buffer,"%02d:%02d:%02d",now.hour(),now.minute(),now.second());
        lcd.print( buffer );
        if ((now.second()%2) == 0 ){
            DisplayTemp();
            TEMP_ST = CheckTime();
            if ((MANUALE==OFF) && (TEMP_ST != STATO_LUCE)) {
               STATO_LUCE=TEMP_ST;
               DriveLight(STATO_LUCE);
            }
            TEMP_ST = CheckTime2();
            if ((MANUALE==OFF) && (TEMP_ST != STATO_LUCE_2)) {
               STATO_LUCE_2=TEMP_ST;
               DriveLight2(STATO_LUCE_2);
            }
            DisplayIcons();
        }
        previousSec = now.second();
    }
    break;  
      case IN_MENU:
        PrintMenu();
        switch(lettera) {
            case 'D':
              PrintSplash();
              STATUS=LOOPING;
              break;
            case 'A':
              lcd.setBacklight(BACKLIGHT_ON); 
              DatoDigitato= getInput("Ora Accensione", "Luce 1? __:__");
              setMyTime(DatoDigitato, MyConfig.OraStart, MyConfig.MinutiStart);
              DatoDigitato= getInput("Ora Spegnimento", "Luce 1?__:__");
              setMyTime(DatoDigitato, MyConfig.OraStop, MyConfig.MinutiStop);
              OraStart    = MyConfig.OraStart;
              MinutiStart = MyConfig.MinutiStart;
              OraStop     = MyConfig.OraStop;
              MinutiStop  = MyConfig.MinutiStop;
              EEPROM.put(0, MyConfig);
              break;
            case 'B':
              lcd.setBacklight(BACKLIGHT_ON); 
              DatoDigitato= getInput("Ora Accensione", "Luce 2? __:__");
              setMyTime(DatoDigitato, MyConfig.OraStart2, MyConfig.MinutiStart2);
              DatoDigitato= getInput("Ora Spegnimento", "Luce 2?__:__");
              setMyTime(DatoDigitato, MyConfig.OraStop2, MyConfig.MinutiStop2);
              OraStart2    = MyConfig.OraStart2;
              MinutiStart2 = MyConfig.MinutiStart2;
              OraStop2     = MyConfig.OraStop2;
              MinutiStop2  = MyConfig.MinutiStop2;
              EEPROM.put(0, MyConfig);
              break;
            case 'C':
              DatoDigitato= getInput("Gradi accensione", "Stufa ? __");
              setMyTemp(DatoDigitato, MyConfig.TempLimit);
              TempLimit = MyConfig.TempLimit;
              EEPROM.put(0, MyConfig);
              break;
              
          }
    }    
}
/* ******************* FUNCTIONS ***************** */
void DisplayTime() {
  lcd.setCursor( 0,0 );
  sprintf(buffer,"%02d:%02d",now.hour(),now.minute());
  lcd.print( buffer );
}

byte CheckTime(int R) {
  char tstart[16] = {0};  
  char tstop[16] = {0};
  char moment[16] = {0};  
  
  sprintf(moment,"%02d%02d",now.hour(),now.minute());
  sprintf(tstart,"%02d%02d",MyConfig.Relais[R].Data[0],MyConfig.Relais[R].Data[1]);
  sprintf(tstop,"%02d%02d",MyConfig.Relais[R].Data[2],MyConfig.Relais[R].Data[3]);
  String smoment((char*)moment);
  String ststart((char*)tstart);
  String ststop((char*)tstop);
  if (ststop > ststart) {
    if((smoment>ststart) && (smoment<ststop))
      return ON;
    else
      return OFF;  
  } else {
    if((smoment>ststart) || (smoment<ststop))
      return ON;
    else
      return OFF;  
  }
}

byte CheckTemp( int R ) {
  byte temperatura;
  bool segnoNegativo = false;
  bool Negative;
  byte TempLimit;
  TempLimit = MyConfig.Relais[R].Data[0];
  Negative  = (MyConfig.Relais[R].Data[1] == 1) ? true : false;
  if (ReadTemp()) {
    if (temperature2 > 127) {
      segnoNegativo = true;
    }
    temperatura = temperature;
    if (Negative == segnoNegativo)  {
      if ( ((segnoNegativo == false) && (temperatura < TempLimit)) || ((segnoNegativo == true) && (temperatura > TempLimit)) ) {
        return ON;
      } else {
        if ( ((segnoNegativo == false) && (temperatura > TempLimit)) || ((segnoNegativo == true) && (temperatura < TempLimit)) ) {
          return OFF;
        }
      }
    } else {
      if ((Negative == false) && (segnoNegativo == true)) {
          return ON;
      } else {
        return OFF;
      }
    }
  }  
}



boolean ReadTemp() {
  if (!dht11.read(PIN_DHT11, &temperature, &humidity, data)) {
      humidity2 = b2byte(data + 8);
      temperature2 = b2byte(data + 24);
      return true;
  }
  return false;
}

void DisplayTemp() {
  byte Sign = '+';  
  if (ReadTemp()) {
    if (temperature2 > 127) {
      Sign = '-';
    }
    if (MANUALE == OFF) {
      STATO_STUFA = CheckTemp( temperature, ((temperature2 > 127) ? true : false) );
      // check orario per luce
    }
    DriveLight(STATO_LUCE);
    DriveLight2(STATO_LUCE_2);
    DriveHeat(STATO_STUFA);
    lcd.setCursor( 0,1 );
    sprintf(buffer,"T:%c%2d.%01d",Sign,temperature,temperature2 % 128);
    lcd.print( buffer );
    lcd.print("\x03");
    sprintf(buffer,"C H:%02d%s",humidity,"%");
    lcd.print( buffer );
  }
  else {
    lcd.setCursor( 0,1 );
    lcd.print("Temp read KO");
  }
}
void DisplayIcons() {
    PrintManuale(MANUALE);
    viewIcon( 1,STATO_STUFA );
    viewIcon( 2,STATO_LUCE );
    viewIcon( 3,STATO_LUCE_2 );
}

void ToggleLight() {
  if(STATO_LUCE == OFF) {
    digitalWrite(rele2, LOW); 
    viewIcon( 2,ON );
    STATO_LUCE = ON;
  }
  else {
    digitalWrite(rele2, HIGH); 
    viewIcon( 2,OFF );
    STATO_LUCE = OFF;
  }
}  
void ToggleLight2() {
  if(STATO_LUCE_2 == OFF) {
    digitalWrite(rele3, LOW); 
    viewIcon( 3,ON );
    STATO_LUCE_2 = ON;
  }
  else {
    digitalWrite(rele3, HIGH); 
    viewIcon( 3,OFF );
    STATO_LUCE_2 = OFF;
  }
}  
void ToggleHeat() {
  if(STATO_STUFA == OFF) {
    digitalWrite(rele1, LOW); 
    viewIcon( 1,ON );
    STATO_STUFA = ON;
  }
  else {
    digitalWrite(rele1, HIGH); 
    viewIcon( 1,OFF );
    STATO_STUFA = OFF;
  }
}  
void DriveLight(byte st) {
  if(st == ON) {
    digitalWrite(rele2, LOW); 
    viewIcon( 2,ON );
  }
  else {
    digitalWrite(rele2, HIGH); 
    viewIcon( 2,OFF );
  }
}  
void DriveLight2(byte st) {
  if(st == ON) {
    digitalWrite(rele3, LOW); 
    viewIcon( 3,ON );
  }
  else {
    digitalWrite(rele3, HIGH); 
    viewIcon( 3,OFF );
  }
}  
void DriveHeat(byte st) {
  if(st == ON) {
    digitalWrite(rele1, LOW); 
    viewIcon( 1,ON );
  }
  else {
    digitalWrite(rele1, HIGH); 
    viewIcon( 1,OFF );
  }
}

/********** Functions *****/

byte b2byte(byte data[8]) {
  byte v = 0;
  for (int i = 0; i < 8; i++) {
      v += data[i] << (7 - i);
  }
  return v;
}  
  
void PrintHead() {
  lcd.clear();
}
void PrintMenu() {
  lcd.setCursor( 0,0 );
  lcd.print("A=Luce1 B=Luce2 "); 
  lcd.setCursor( 0,1 );
  lcd.print("C=Temp. D=Uscita"); 
}
void viewIcon(byte num, byte status) {
  lcd.setCursor( ( 12+num ),0 );
  if ( status == ON ) { 
     lcd.print((char)(num-1));
  } else { 
    lcd.print(" ");
  }
}
void PrintManuale(byte status) {
  lcd.setCursor( 9 ,0 );
  if ( status == ON) { 
    lcd.print("Man.");
  } else { 
    lcd.print("Auto");
  }
  
}
void PrintSplash() {
  lcd.clear();
  lcd.setCursor ( 0,0 );
  lcd.print("Serra Zap v.1.0");
  lcd.setCursor ( 0,1 );
  lcd.print("Configurazione: ");
  delay(2000);
  lcd.clear();
  lcd.setCursor ( 0,0 );
  lcd.print("Lim. Temp:");
  if (MyConfig.Negative) lcd.print("-"); else lcd.print("+");
  sprintf(buffer,"%d",MyConfig.TempLimit);
  lcd.print(buffer);
  lcd.print("\x03");
  lcd.print("C");
  lcd.setCursor ( 0,1 );
  lcd.print("Stufa:");
  lcd.print( (MyConfig.STATO_STUFA == ON) ? "Si" : "No");
  lcd.print(" L:");
  lcd.print( (MyConfig.STATO_LUCE == ON) ? "Si" : "No");
  lcd.print("-");
  lcd.print( (MyConfig.STATO_LUCE_2 == ON) ? "Si" : "No");
  delay(3000);
  lcd.clear();
  lcd.setCursor ( 0,0 );
  sprintf(buffer,"L1 On  %02d:%02d",MyConfig.OraStart, MyConfig.MinutiStart);
  lcd.print(buffer);
  lcd.setCursor ( 0,1 );
  sprintf(buffer,"L1 Off %02d:%02d",MyConfig.OraStop, MyConfig.MinutiStop);
  lcd.print(buffer);
  delay(3000);
  lcd.setCursor ( 0,0 );
  sprintf(buffer,"L2 On  %02d:%02d",MyConfig.OraStart2, MyConfig.MinutiStart2);
  lcd.print(buffer);
  lcd.setCursor ( 0,1 );
  sprintf(buffer,"L2 Off %02d:%02d",MyConfig.OraStop2, MyConfig.MinutiStop2);
  lcd.print(buffer);
  delay(3000);
  lcd.clear();
  lcd.print("*****START*****");
  delay(2000);
  lcd.clear();
  BACKLIGHT = OFF;
  lcd.setBacklight(BACKLIGHT_OFF);
}
String getInput(String prompt, String mask) {
  int pos, pos1;
  char lettera;
  String retval = "";
  lcd.clear();
  lcd.setCursor ( 0,0 );
  lcd.print(prompt);
  lcd.setCursor ( 0,1 );
  lcd.print(mask);
  pos = mask.indexOf('_');
  lcd.setCursor ( pos,1 );
  lcd.blink();
  do {
    lettera = customKeypad.getKey();
    if ((lettera >= '0') && (lettera <= '9')) {
      Serial.print("Pos:");
      Serial.println(pos);
      lcd.print(lettera);
      pos1 = mask.indexOf('_',pos+1);
      if (pos1 != -1) {
        pos = pos1; 
      }
      lcd.setCursor ( pos,1 );
      Serial.print("Pos1");
      Serial.println(pos1);
      retval = retval + lettera;
    }
//  } while((lettera != '#') && (pos1 != -1));              // until not a digit
  } while(pos1 != -1); 
  delay(2000);
  lcd.clear();
  lcd.noBlink();
  lcd.noCursor();
  return retval;
}
void setMyTime(String val, byte &h, byte &m) {
  int ora=(int)(val.charAt(0)-'0') * 10 + (int)(val.charAt(1)-'0');
  int minuti=(int)(val.charAt(2)-'0') * 10 + (int)(val.charAt(3)-'0');
  h = (char) ora;
  m = (char) minuti;  
  sprintf(buffer,"Ora:%d - Minuti:%d",h,m);
  Serial.println(buffer);
}
void setMyTemp(String val, byte &t) {
  int temp=(int)(val.charAt(0)-'0') * 10 + (int)(val.charAt(1)-'0');
  t = (char) temp;
  sprintf(buffer,"Temp:%d",t);
  Serial.println(buffer);
}
  

  
