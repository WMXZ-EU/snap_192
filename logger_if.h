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
 

#ifndef _LOGGER_IF_H
#define _LOGGER_IF_H

#include "kinetis.h"
#include "core_pins.h"

typedef struct {
    char    rId[4];
    unsigned int rLen;
    char    wId[4];
    char    fId[4];
    unsigned int    fLen;
    unsigned short nFormatTag;
    unsigned short nChannels;
    unsigned int nSamplesPerSec;
    unsigned int nAvgBytesPerSec;
    unsigned short nBlockAlign;
    unsigned short  nBitsPerSamples;
    char    iId[4];
    unsigned int  iLen;
    char    info[512-13*4]; // fill header to 512 bytes
    char    dId[4];
    unsigned int    dLen;
} HdrStruct;

/*
'IARL' ArchivalLocation 
'IART' Artist 
'ICMS' Commissioned 
'ICMT' Comment 
'ICOP' Copyright 
'ICRD' DateCreated 
'ICRP' Cropped 
'IDIM' Dimensions 
'IDPI' DotsPerInch 
'IENG' Engineer 
'IGNR' Genre 
'IKEY' Keywords 
'ILGT' Lightness 
'IMED' Medium 
'INAM' Title 
'IPLT' NumColors 
'IPRD' Product 
'ISBJ' Subject 
'ISFT' Software 
'ISHP' Sharpness 
'ISRC' Source 
'ISRF' SourceForm 
'ITCH' Technician 
*/
HdrStruct wav_hdr;

//==================== local uSD interface ========================================
// this implementation used SdFs from Bill Greiman
// which needs to be installed as local library 
//
uint32_t record_or_sleep(void);

#ifndef MAXFILE
  #define MAXFILE 100
#endif
#ifdef MAXBUF
  #define MAXBUF 200
#endif

#ifndef BUFFERSIZE
  #define BUFFERSIZE (8*1024)
#endif
int16_t diskBuffer[BUFFERSIZE];
int16_t *outptr = diskBuffer;

#include "mfs.h"

class c_uSD
{
  public:
    c_uSD(void): state(-1), closing(0) {;}
    void init(void);

    void chDir(void);
    
    int16_t write(int16_t * data, int32_t ndat);
    uint16_t getNbuf(void) {return nbuf;}
    void setClosing(void) {closing=1;}

    void exit(void);
    
  private:
    int16_t state; // 0 initialized; 1 file open; 2 data written; 3 to be closed
    int16_t nbuf;
    int16_t closing;

    c_mFS mFS;

};
c_uSD uSD;


/*
 *  Logging interface support / implementation functions 
 */

char *makeDirname(void)
{ static char dirname[40];

  struct tm tx = seconds2tm(RTC_TSR);
  sprintf(dirname, "%04d%02d%02d", tx.tm_year, tx.tm_mon, tx.tm_mday);
  
  #if DO_DEBUG>0
    Serial.println(dirname);
  #endif
  return dirname;  
}

char *makeFilename(void)
{ static char filename[40];

  struct tm tx = seconds2tm(RTC_TSR);
//  sprintf(filename, "WMXZ_%04d_%02d_%02d_%02d_%02d_%02d", tx.tm_year, tx.tm_mon, tx.tm_mday, tx.tm_hour, tx.tm_min, tx.tm_sec);
  sprintf(filename, "%02d_%02d_%02d.wav", tx.tm_hour, tx.tm_min, tx.tm_sec);
  
  #if DO_DEBUG>0
    Serial.println(filename);
  #endif
  return filename;  
}

//____________________________ FS Interface implementation______________________
void c_uSD::init(void)
{
  mFS.init();
  //
  nbuf=0;
  state=0;
}

void c_uSD::chDir(void)
{ char * dirName = makeDirname();
  mFS.mkDir(dirName);
  mFS.chDir(dirName);
}

void c_uSD::exit(void)
{ mFS.exit();
}


int16_t c_uSD::write(int16_t *data, int32_t ndat)
{
  if(state == 0)
  { // open file
    char *filename = makeFilename();
    if(!filename) {state=-1; return state;} // flag to do nothing anymore
    //
    mFS.open(filename);

    state=1; // flag that file is open
    nbuf=0;
  }
  
  if(state == 1 || state == 2)
  {  // write to disk
    state=2;
    mFS.write((unsigned char *) data, 2*ndat);
    //
    nbuf++;
//    if(nbuf==MAXBUF) state=3; // flag to close file
    //
    uint32_t nsec = record_or_sleep();  // check if record time is over
    if(nsec>0) state=3;
    //
    if(closing) {closing=0; state=3;}
  }
  
  if(state == 3)
  { // update Header
    uint32_t ndat = 512 + nbuf*BUFFERSIZE * 2;
    wav_hdr.dLen = ndat;
    wav_hdr.rLen = 512 + wav_hdr.dLen;
    mFS.writeHeader((char *) &wav_hdr,512); 

    // close file
    mFS.close();
    state=0;  // flag to open new file
  }
  return state;
}
#endif
