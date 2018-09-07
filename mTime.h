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
#ifndef MTIME_H
#define MTIME_H

#include "core_pins.h"
#include "usb_serial.h"

//_______________________________ For File Time settings _______________________
#include <time.h>
#define EPOCH_YEAR 1970 //T3 RTC
#define LEAP_YEAR(Y) (((EPOCH_YEAR+Y)>0) && !((EPOCH_YEAR+Y)%4) && ( ((EPOCH_YEAR+Y)%100) || !((EPOCH_YEAR+Y)%400) ) )
static  const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; 

/*  int  tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
*/

struct tm seconds2tm(uint32_t tt)
{ struct tm tx;
  tx.tm_sec   = tt % 60;    tt /= 60; // now it is minutes
  tx.tm_min   = tt % 60;    tt /= 60; // now it is hours
  tx.tm_hour  = tt % 24;    tt /= 24; // now it is days
  tx.tm_wday  = ((tt + 4) % 7) + 1;   // Sunday is day 1 (tbv)

  // tt is now days since EPOCH_Year (1970)
  uint32_t year = 0;  
  uint32_t days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= tt) year++;

  tx.tm_year = 1970+year; // year is NOT offset from 1970 

  // correct for last (actual) year
  days -= (LEAP_YEAR(year) ? 366 : 365);
  tt  -= days; // now tt is days in this year, starting at 0
  
  uint32_t mm=0;
  uint32_t monthLength=0;
  for (mm=0; mm<12; mm++) 
  { monthLength = monthDays[mm];
    if ((mm==1) & LEAP_YEAR(year)) monthLength++; 
    if (tt<monthLength) break;
    tt -= monthLength;
  }
  tx.tm_mon = mm + 1;   // jan is month 1  
  tx.tm_mday = tt + 1;     // day of month
  return tx;
}

uint32_t tm2seconds (struct tm *tx) 
{
  uint32_t tt;
  tt=tx->tm_sec+tx->tm_min*60+tx->tm_hour*3600;  

  // count days size epoch until previous midnight
  uint32_t days=tx->tm_mday-1;

  int32_t mm=0;
//  uint32_t monthLength=0;
  for (mm=0; mm<(tx->tm_mon-1); mm++) days+=monthDays[mm]; 
  if(tx->tm_mon>2 && LEAP_YEAR(tx->tm_year-1970)) days++;

  int32_t years=0;
  while(years++ < (tx->tm_year-1970)) days += (LEAP_YEAR(years) ? 366 : 365);
  //  
  tt+=(days*24*3600);
  return tt;
}
#endif
