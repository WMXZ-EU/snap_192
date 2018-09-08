/* SGTL5000 Recorder for Teensy 3.X
 * Copyright (c) 2018, Walter Zimmer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
 /*
  * The menu implements the Loggerhead SNAP protocol
  */
  
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// RTC interface
uint32_t getRTC(void) { return RTC_TSR;}
void setRTC(uint32_t tt) {RTC_TSR = tt;}


/* DISPLAY FUNCTIONS
 *  
 */
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);
#define BOTTOM 55
 
const int UP = 4;
const int DOWN = 3;  // new board pin
const int SELECT = 8;

const int displayPow = 20;

//int32_t lhi_fsamps[7] = {8000, 16000, 32000, 44100, 48000, 96000, 192000};
//#define I_SAMP 5   // 0 is 8 kHz; 1 is 16 kHz; 2 is 32 kHz; 3 is 44.1 kHz; 4 is 48 kHz; 5 is 96 kHz; 6 is 192 kHz
//float audio_srate = lhi_fsamps[I_SAMP];
//int isf = I_SAMP;
//

int updateVal(long curVal, long minVal, long maxVal);
void cDisplay();
void displaySettings();
void displayClock(uint32_t t, int loc);
void printTime(uint32_t t);
void readEEPROM();
void writeEEPROM();

uint16_t haveDisplay;
int16_t menuSetup(void)
{
  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(SELECT, INPUT_PULLUP);

  haveDisplay=0;
  delay(10);
  if(digitalReadFast(UP)) return 0;

  haveDisplay=1;
  pinMode(displayPow, OUTPUT);
  digitalWriteFast(displayPow, HIGH);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  //initialize display

  delay(100);
  cDisplay();
  display.println("Snap_192");
  display.display();
  while(digitalReadFast(SELECT)) asm("wfi");
  while(!digitalReadFast(SELECT)) asm("wfi");
  return 1;
}

void menuExit(void)
{
  if(haveDisplay)
  {
    display.ssd1306_command(SSD1306_DISPLAYOFF); // turn off display during recording
    digitalWriteFast(displayPow, LOW);
  }
  
  pinMode(UP, INPUT_DISABLE);
  pinMode(DOWN, INPUT_DISABLE);
  pinMode(SELECT, INPUT_DISABLE);

}

//time_t autoStartTime;
uint32_t autoStartTime;

#define noSet 0
#define setRecDur 1
#define setRecSleep 2
#define setYear 3
#define setMonth 4
#define setDay 5
#define setHour 6
#define setMinute 7
#define setSecond 8
#define setMode 9
#define setStartHour 10
#define setStartMinute 11
#define setEndHour 12
#define setEndMinute 13
#define setFsamp 14

#define MODE_NORMAL 0
#define MODE_DIEL 1

int recMode = MODE_NORMAL;
long rec_dur = 10;
long rec_int = 30;
boolean settingsChanged = 0;

byte startHour, startMinute, endHour, endMinute; //used in Diel mode

void manualSettings(){
  boolean startRec = 0, startUp, startDown;
  readEEPROM();

  autoStartTime = getRTC();
    
  // make sure settings valid (if EEPROM corrupted or not set yet)
  if (rec_dur < 0 | rec_dur>100000) rec_dur = 60;
  if (rec_int<0 | rec_int>100000) rec_int = 60;
  if (startHour<0 | startHour>23) startHour = 0;
  if (startMinute<0 | startMinute>59) startMinute = 0;
  if (endHour<0 | endHour>23) endHour = 0;
  if (endMinute<0 | endMinute>59) endMinute = 0;
  if (recMode<0 | recMode>1) recMode = 0;
  if (isf<0 | isf>MAX_FSI) isf = FSI;
    
  while(startRec==0){
    static int curSetting = noSet;
    static int newYear, newMonth, newDay, newHour, newMinute, newSecond, oldYear, oldMonth, oldDay, oldHour, oldMinute, oldSecond;
    
    // Check for mode change
    boolean selectVal = digitalRead(SELECT);
    if(selectVal==0){
      curSetting += 1;
      while(digitalRead(SELECT)==0){ // wait until let go of button
        delay(10);
      }
      if(recMode==MODE_NORMAL & (curSetting>8) & (curSetting<14)) curSetting = 14;
      if(recMode==MODE_NORMAL & (curSetting>14)) curSetting = 0;
   }

    cDisplay();
    uint32_t tt = getRTC();
    struct tm tx = seconds2tm(tt);
    if (tt - autoStartTime > 600) startRec = 1; //autostart if no activity for 10 minutes
    
    switch (curSetting){
      case noSet:
        if (settingsChanged) {
          writeEEPROM();
          settingsChanged = 0;
          autoStartTime = tt;  //reset autoStartTime
        }
        display.print("UP+DN->Rec"); 
        // Check for start recording
        startUp = digitalRead(UP);
        startDown = digitalRead(DOWN);
        if(startUp==0 & startDown==0) {
          cDisplay();
          writeEEPROM(); //save settings
          display.print("Starting..");
          display.display();
          delay(1500);
          startRec = 1;  //start recording
        }
        break;
        
      case setRecDur:
        rec_dur = updateVal(rec_dur, 1, 3600);
        display.printf("Rec:%ds\r\n",rec_dur);
        break;
        
      case setRecSleep:
        rec_int = updateVal(rec_int, 0, 3600 * 24);
        display.printf("Slp:%ds\r\n",rec_int);
        break;
        //
      case setYear:
        oldYear = tx.tm_year;
        newYear = updateVal(oldYear,2000, 2100);
        
        if(oldYear!=newYear) {
          tx.tm_year=newYear;
          setRTC(tm2seconds (&tx));
        }
        display.printf("Year:%d\r\n",newYear);
        break;
        
      case setMonth:
        oldMonth = tx.tm_mon;
        newMonth = updateVal(oldMonth, 1, 12);
        if(oldMonth != newMonth) {
          tx.tm_mon=newMonth;
          setRTC(tm2seconds (&tx));
        }
        display.printf("Month:%d\r\n",newMonth);
        break;
        
      case setDay:
        oldDay = tx.tm_mday;
        newDay = updateVal(oldDay, 1, 31);
        if(oldDay!=newDay) {
          tx.tm_mday=newDay;
          setRTC(tm2seconds (&tx));
        }
        display.printf("Day:%d\r\n",newDay);
        break;
        
      case setHour:
        oldHour = tx.tm_hour;
        newHour = updateVal(oldHour, 0, 23);
        if(oldHour!=newHour) {
          tx.tm_hour=newHour;
          setRTC(tm2seconds (&tx));
        }
        display.printf("Hour:%d\r\n",newHour);
        break;
        
      case setMinute:
        oldMinute = tx.tm_min;
        newMinute = updateVal(oldMinute, 0, 59);
        if(oldMinute!=newMinute) {
          tx.tm_hour=newMinute;
          setRTC(tm2seconds (&tx));
        }
        display.printf("Minute:%d\r\n",newMinute);
        break;
        
      case setSecond:
        oldSecond = tx.tm_sec;
        newSecond = updateVal(oldSecond, 0, 59);
        if(oldSecond!=newSecond) {
          tx.tm_hour=newSecond;
          setRTC(tm2seconds (&tx));
        }
        display.printf("Second:%d\r\n",newSecond);
        break;
      case setFsamp:
        isf = updateVal(isf, 0, MAX_FSI);
        display.printf("SF: %.1f\r\n",fsamps[isf]/1000.0f);
        break;
    }
    displaySettings();
    displayClock(tt, BOTTOM);
    display.display();
    delay(10);
  }
}

int updateVal(long curVal, long minVal, long maxVal){
  boolean upVal = digitalRead(UP);
  boolean downVal = digitalRead(DOWN);
  static int heldDown = 0;
  static int heldUp = 0;

  if(upVal==0){
    settingsChanged = 1;
    if (heldUp < 20) delay(200);
      curVal += 1;
      heldUp += 1;
    }
    else heldUp = 0;
    
    if(downVal==0){
      settingsChanged = 1;
      if(heldDown < 20) delay(200);
      if(curVal < 10) { // going down to 0, go back to slow mode
        heldDown = 0;
      }
        curVal -= 1;
        heldDown += 1;
    }
    else heldDown = 0;

    if (curVal < minVal) curVal = maxVal;
    if (curVal > maxVal) curVal = minVal;
    return curVal;
}

void cDisplay(){
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0,0);
}

void displaySettings(){
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 18);
//  display.print("Mode:");
//  if (recMode==MODE_NORMAL) display.println("Normal");
//  if (recMode==MODE_DIEL) {
//    display.println("Diel");
//  }
  display.printf("Rec:   %d s\r\n",rec_dur);
  display.printf("Sleep: %d s\r\n",rec_int);
  if (recMode==MODE_DIEL) {
    display.printf("Active: %02d:%02d-%02d:%02d\r\n",startHour,startMinute,endHour,endMinute);
  }
  display.printf("Fsamp: %.1f kHz\n",fsamps[isf]/1000.0f);
}

char * timestamp(uint32_t tt)
{
  struct tm tx = seconds2tm(tt);
  static char text[40];
  sprintf(text,"%04d-%02d-%02d %02d:%02d:%02d",
                tx.tm_year,tx.tm_mon,tx.tm_mday,tx.tm_hour,tx.tm_min,tx.tm_sec);
  return text;  
}

void displayClock(uint32_t tt, int loc){
  display.setTextSize(1);
  display.setCursor(0,loc);
  display.print(timestamp(tt));
}

void printTime(uint32_t tt){ Serial.println(timestamp(tt));}

/*************** EEPROM interface ****************/
union {
  byte b[4];
  long lval;
}u;

long readEEPROMlong(int address){
  u.b[0] = EEPROM.read(address);
  u.b[1] = EEPROM.read(address + 1);
  u.b[2] = EEPROM.read(address + 2);
  u.b[3] = EEPROM.read(address + 3);
  return u.lval;
}

void writeEEPROMlong(int address, long val){
  u.lval = val;
  EEPROM.write(address, u.b[0]);
  EEPROM.write(address + 1, u.b[1]);
  EEPROM.write(address + 2, u.b[2]);
  EEPROM.write(address + 3, u.b[3]);
}

void readEEPROM(){
  rec_dur = readEEPROMlong(0);
  rec_int = readEEPROMlong(4);
  startHour = EEPROM.read(8);
  startMinute = EEPROM.read(9);
  endHour = EEPROM.read(10);
  endMinute = EEPROM.read(11);
  recMode = EEPROM.read(12);
  isf = EEPROM.read(13);
}

void writeEEPROM(){
  writeEEPROMlong(0, rec_dur);  //long
  writeEEPROMlong(4, rec_int);  //long
  EEPROM.write(8, startHour); //byte
  EEPROM.write(9, startMinute); //byte
  EEPROM.write(10, endHour); //byte
  EEPROM.write(11, endMinute); //byte
  EEPROM.write(12, recMode); //byte
  EEPROM.write(13, isf); //byte
}
