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
#define DO_DEBUG 1

#include "core_pins.h"
#if DO_DEBUG==0
  #include "usb_serial.h"
#endif
#include "Wire.h"

#define FSI 5// desired sampling frequency index
#define MAX_FSI 6
uint32_t fsamps[] = {8000, 16000, 32000, 44100, 48000, 96000, 192000, 220500, 240000};

float audio_srate = fsamps[FSI];
int isf = FSI;

/*
 * NOTE: changing frequency impacts the macros 
 *      AudioProcessorUsage and AudioProcessorUsageMax
 * defined in stock AudioStream.h
 */
#define NCH 1 // number of channels
#define SEL_LR 1  // record only a single channel (0 left, 1 right)

#if defined(__MK20DX256__)
  #define MQUEU (100/NCH) // number of buffers in aquisition queue
#elif defined(__MK64FX512__)
  #define MQUEU (200/NCH) // number of buffers in aquisition queue
#elif defined(__MK66FX1M0__)
  #define MQUEU (550/NCH) // number of buffers in aquisition queue
#else
  #define MQUEU 53 // number of buffers in aquisition queue
#endif
  

// definitions for logging
#define MAXBUF 200
#define BUFFERSIZE (8*1024)

// adapted from audio gui
  #include "input_i2s.h"
  AudioInputI2S        acq;

  #include "m_queue.h"
  mRecordQueue<MQUEU>  queue1;

  AudioConnection      patchCord1(acq,SEL_LR, queue1,0);

  #include "control_sgtl5000.h"
  AudioControlSGTL5000 audioShield;

// private 'library' included directly into sketch

#include "i2s_mods.h"
#include "logger_if.h"
#include "hibernate.h"

// utility for hibernating
const int32_t on = 60;
const int32_t off = 60;

uint32_t record_or_sleep(void)
{
  uint32_t tt = RTC_TSR;
  uint32_t dt = tt % (on+off);
  if(dt>=on) return (on+off-dt);
  return 0;
}

// utility for logger

#define WAV_HEADER

char * headerUpdate(void)
{
#ifdef WAV_HEADER
    sprintf(wav_hdr.rId,"RIFF");
//    wav_hdr.rLen=44-2*4;
    wav_hdr.rLen=512-2*4;
    sprintf(wav_hdr.wId,"WAVE");
    
    sprintf(wav_hdr.fId,"fmt ");
    wav_hdr.fLen=0x10;
    wav_hdr.nFormatTag=1;
    wav_hdr.nChannels=1;
    wav_hdr.nSamplesPerSec=audio_srate;
    wav_hdr.nAvgBytesPerSec=audio_srate*2;
    wav_hdr.nBlockAlign=2;
    wav_hdr.nBitsPerSamples=16;

    sprintf(wav_hdr.iId,"info");
    wav_hdr.iLen = 512 - 13*4;
    
    struct tm tx = seconds2tm(RTC_TSR);
    sprintf(&wav_hdr.info[0], "%04d_%02d_%02d_%02d_%02d_%02d", 
                tx.tm_year, tx.tm_mon, tx.tm_mday, tx.tm_hour, tx.tm_min, tx.tm_sec);
    //
    // add more info to header
    //
    sprintf(&wav_hdr.info[20],"%6d, %4d, %4d",fsamps[isf],on,off);
    sprintf(&wav_hdr.info[40],"end");

    sprintf(wav_hdr.dId,"data");
    wav_hdr.dLen = MAXBUF * BUFFERSIZE * 2;
    wav_hdr.rLen += wav_hdr.dLen;
  
   return (char *)&wav_hdr;

#else
  static char header[512];
  sprintf(&header[0], "WMXZ");
  
  struct tm tx = seconds2tm(RTC_TSR);
  sprintf(&header[5], "%04d_%02d_%02d_%02d_%02d_%02d", tx.tm_year, tx.tm_mon, tx.tm_mday, tx.tm_hour, tx.tm_min, tx.tm_sec);
  //
  // add more info to header
  //
  *(uint32_t*) &header[24] = fsamps[isf];
  *(int32_t*) &header[28] = on;
  *(int32_t*) &header[32] = off;
  return header;

#endif
}

// utility for acquisition
const int hydroPowPin = 2;
void acqInit(void)
{
  pinMode(hydroPowPin, OUTPUT);
  digitalWriteFast(hydroPowPin, HIGH);
}

void acqExit(void)
{
  digitalWriteFast(hydroPowPin, LOW);
  
  pinMode(9,INPUT_DISABLE);
  pinMode(11,INPUT_DISABLE);
  pinMode(13,INPUT_DISABLE);
  pinMode(23,INPUT_DISABLE);
}

// utility for non-acoustic data
#define ADC_RES 10
#define ADC_SCALE (float) (1<<ADC_RES)

void analogInit()
{
   analogReference(0); // external reference (3.3V)
   analogReadRes(ADC_RES);  // 10 bit resolution
}

float readVoltage(){
   float  voltage = 0;
   const int vSense = 21; 
   pinMode(vSense,INPUT);
   for(int n = 0; n<8; n++){
    voltage += (float) analogRead(vSense) / ADC_SCALE;
   }
   const float scl = 6.4f; // was 5.9f in original snap (should be 2*Vref or 2*3.3 = 6.6
   voltage = scl * voltage / 8.0f;   //fudging scaling based on actual measurements; shoud be max of 3.3V at 1023
   return voltage;
}

float readTemp(){
   float  voltage = 0;
  #if defined(__MK20DX256__)
     const int vTemp = 38;
  #elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
     const int vTemp = 70;
  #endif
   for(int n = 0; n<8; n++){
    voltage += (float) analogRead(vTemp) / ADC_SCALE;
   }
   voltage = voltage*3.3f/ADC_SCALE; // V
   voltage = 25.0f-((voltage-0.716f)*0.588f); // deg C

   return voltage;
}

void logAcq(void)
{
  #if USE_FS == SdFS
    FsFile file;
  #elif  USE_FS == SDo
    File file;
  #endif

  if (!file.open("acqLog.txt", O_CREAT | O_WRITE | O_APPEND)) {Serial.println("LOG"); while(1) asm volatile("wfi");}

  struct tm tx = seconds2tm(RTC_TSR);
  file.printf("%04d%02d%02d_%02d%02d%02d", 
                tx.tm_year,tx.tm_mon,tx.tm_mday,tx.tm_hour,tx.tm_min,tx.tm_sec);

  file.printf(": VIN %f; Temp %f \r\n",readVoltage(),readTemp());
  
  file.close();
}

// display menu
#include "display.h"

//__________________________General Arduino Routines_____________________________________
extern "C" void setup() {
  // put your setup code here, to run once:

  #if DO_DEBUG>0
    while(!Serial);
  #endif


  if(menuSetup()) {  menuLoop(); } menuExit();  

  uint32_t nsec = record_or_sleep();
  if(nsec>0)
  { 
    #if DO_DEBUG>0
      Serial.print("hibernating... "); Serial.print(nsec); Serial.println(" sec"); 
    #endif
    Wire.begin();
    SGTL5000_disable();
    Wire.end();
    I2S_stopClocks();
    delay(10);
    setWakeupCallandSleep(nsec);
  }

  acqInit();
  
  AudioMemory (MQUEU+6);
  audioShield.enable();
  audioShield.inputSelect(AUDIO_INPUT_LINEIN);  //AUDIO_INPUT_LINEIN or AUDIO_INPUT_MIC
   //
  I2S_modification(fsamps[isf],32);
  delay(1);
  SGTL5000_modification(isf); // must be called after I2S initialization stabilized
  
  #if DO_DEBUG>0
    Serial.println("start");
    Serial.print("Vin: "); Serial.println(readVoltage());
    Serial.print("Vtemp: "); Serial.println(readTemp());
  #endif

  uSD.init();
  logAcq();
  uSD.chDir(); 

  queue1.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  static int16_t state=0; // 0: open new file, -1: last file

  if(queue1.available())
  {  // have data on queue
    if(state==0)
    { // generate header before file is opened
       uint32_t *header=(uint32_t *) headerUpdate();
       uint32_t *ptr=(uint32_t *) outptr;
       // copy to disk buffer
       for(int ii=0;ii<128;ii++) ptr[ii] = header[ii];
       outptr+=256; //(512 bytes)
       state=1;
    }
    // fetch data from queue
    int32_t * data = (int32_t *)queue1.readBuffer(); // cast to int32 to speed-up following copy
    //
    // copy to disk buffer
    uint32_t *ptr=(uint32_t *) outptr;
    for(int ii=0;ii<64;ii++) ptr[ii] = data[ii];
    queue1.freeBuffer(); 
    //
    // advance buffer pointer
    outptr+=128; // (128 shorts)
    //
    // if necessary reset buffer pointer and write to disk
    // buffersize should be always a multiple of 512 bytes
    if(outptr == (diskBuffer+BUFFERSIZE))
    {
      outptr = diskBuffer;
 
      // write to disk ( this handles also opening of files)
      if(state>=0)
        state=uSD.write(diskBuffer,BUFFERSIZE); // this is blocking

      if(state==0)
      {
        uint32_t nsec = record_or_sleep();
        if(nsec>0) 
        { queue1.end();
          SGTL5000_disable();
          Wire.end();
          I2S_stopClocks();
          acqExit();
          delay(10);
          uSD.exit();
          setWakeupCallandSleep(nsec);      
        }
      }
    }
  }
  else
  {  // queue is empty
    // do nothing
  }

   #if DO_DEBUG>0
    // some statistics on progress
    static uint32_t loopCount=0;
    static uint32_t t0=0;
    loopCount++;
    if(millis()>t0+1000)
    {  Serial.printf("loop: %5d %4d; %4d",
             loopCount, uSD.getNbuf(),
             AudioMemoryUsageMax());
       Serial.println();
       AudioMemoryUsageMaxReset();
       t0=millis();
       loopCount=0;
    }
  #endif
  //
  asm volatile ("wfi"); // to save some power switch off idle cpu
}
