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
 
#ifndef MFS_H
#define MFS_H

/* MSF API 
 *  init(void);
 *  void exit(void);
 *  void open(char * filename);
 *  void writeHeader(char * header, uint32_t ndat);
 *  void close(void);
 *  uint32_t write(uint8_t *buffer, uint32_t nbuf);
 *  uint32_t read(uint8_t *buffer, uint32_t nbuf);
 */
 #define SDo   1 // Stock SD library
 #define SdFS  2 // Greimans SD library
 #define uSDFS 3 // CHaN's SD library (does no work yet)
// note SdFS may need CHECK_PROGRAMMING set to 1 in SdSpiCard.cpp

#define USE_FS SdFS

// Preallocate 8MB file.
const uint64_t PRE_ALLOCATE_SIZE = 8ULL << 20;

/************************** File System Interface****************/
#if USE_FS == SdFS

#include "mTime.h"
#include "SdFs.h"

#if defined(__MK20DX256__)
  #define SD_CS 10
  #define SD_CONFIG SdSpiConfig(SD_CS, DEDICATED_SPI, SPI_FULL_SPEED)
  
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
  // Use FIFO SDIO or DMA_SDIO
  #define SD_CONFIG SdioConfig(FIFO_SDIO)
  //#define SD_CONFIG SdioConfig(DMA_SDIO)
#endif


// Call-back for file timestamps.  Only called for file create and sync().
void dateTime(uint16_t* date, uint16_t* time) {

  struct tm tx=seconds2tm(RTC_TSR);
    
  // Return date using FS_DATE macro to format fields.
  *date = FS_DATE(tx.tm_year, tx.tm_mon, tx.tm_mday);

  // Return time using FS_TIME macro to format fields.
  *time = FS_TIME(tx.tm_hour, tx.tm_min, tx.tm_sec);
}

class c_mFS
{
  private:
  SdFs sd;
  FsFile file;
  
  public:
    void init(void)
    { 
      #if DO_DEBUG>0
        Serial.println("Using SdFS");
      #endif
      #if defined(__MK20DX256__)
          // Initialize the SD card
        SPI.setMOSI(7);
        SPI.setSCK(14);
      #endif  
      if (!sd.begin(SD_CONFIG)) sd.errorHalt("sd.begin failed");
    
      // Set Time callback
      FsDateTime::callback = dateTime;
    }

    void mkDir(char * dirname)  { if(!sd.exists(dirname)) sd.mkdir(dirname); }
    void chDir(char * dirname)  { sd.chdir(dirname); }
    
    void exit(void)
    {
      delay(100);
      #if defined(__MK20DX256__)
        #define SD_CS 10
        digitalWriteFast(SD_CS,LOW);
          int ii;
          for(ii=0; SPI.transfer(0xff)!=0xFF && ii<30000; ii++) asm("wfi");
          #if DO_DEBUG>0
            Serial.println(ii); Serial.flush();
          #endif
        digitalWriteFast(SD_CS,HIGH);
        pinMode(SD_CS,INPUT_DISABLE);
  
        sd.end();
        SPI.end();
      #endif
    }
    
    void open(char * filename)
    {
      if (!file.open(filename, O_CREAT | O_TRUNC |O_RDWR)) {
        Serial.println(filename);
        sd.errorHalt("file.open failed");
      }
      if (!file.preAllocate(PRE_ALLOCATE_SIZE)) {
        sd.errorHalt("file.preAllocate failed");    
      }
    }

    void writeHeader(char * header, uint32_t ndat)
    {
      uint32_t fpos = file.curPosition();
      file.seek(0);
      file.write(header,ndat);
      file.seek(fpos);
      
    }
    void close(void)
    {
      if(!file.truncate()) Serial.println("file.truncate failed");
      if(!file.close()) Serial.println("file.close failed");
    }

    uint32_t write(uint8_t *buffer, uint32_t nbuf)
    {
      if (nbuf != file.write(buffer, nbuf)) sd.errorHalt("write failed");
      return nbuf;
    }

    uint32_t read(uint8_t *buffer, uint32_t nbuf)
    {      
      if ((int)nbuf != file.read(buffer, nbuf)) sd.errorHalt("read failed");
      return nbuf;
    }
};

#elif  USE_FS == SDo
#include "mTime.h"
#include "SPI.h"
#include "SdFat.h"

#if defined(__MK20DX256__)
  #define SD_CS 10
  
#elif defined(__MK64FX512__) || defined(__MK66FX1M0__)
  #define SD_CS  BUILTIN_SDCARD

#endif

class c_mFS
{
  private:
  File file;
  SdFat sd;

  void die(char *txt) {Serial.println(txt); while(1) asm("wfi");}
  
  public:
    void init(void)
    { 
      #if DO_DEBUG>0
        Serial.println("Using SD");
      #endif
      #if defined(__MK20DX256__)
          // Initialize the SD card
        SPI.setMOSI(7);
        SPI.setSCK(14);
      #endif  
      if (!(sd.begin(SD_CS))) die("error sd.begin");
    }

    void mkDir(char * dirname)  { if(!sd.exists(dirname)) sd.mkdir(dirname); }
    void chDir(char * dirname)  { sd.chdir(dirname); }
    
    void open(char * filename) { file = sd.open(filename, FILE_WRITE);  }
    void close(void) { file.close(); }
    void exit(void){ }

    void writeHeader(char * header, uint32_t ndat) 
    { uint32_t fpos = file.curPosition();
      file.seek(0);
      file.write(header,ndat);
      file.seek(fpos);
    }

    uint32_t write(uint8_t *buffer, uint32_t nbuf)
    {
      file.write(buffer, nbuf);
      return nbuf;
    }

    uint32_t read(uint8_t *buffer, uint32_t nbuf)
    {
      if ((int)nbuf != file.read(buffer, nbuf)) {Serial.println("error file read"); while(1) asm("wfi");}
      return nbuf;
    }
};

#elif  USE_FS == uSDFS
#include "ff.h"
#include "ff_utils.h"

extern "C" uint32_t usd_getError(void);

class c_mFS
{
  private:
    FRESULT rc;     /* Result code */
    FATFS fatfs;    /* File system object */
    FIL fil;        /* File object */

    UINT wr;
    
    TCHAR wfilename[80];
  
    /* Stop with dying message */
    void die(char *str, FRESULT rc) 
    { Serial.printf("%s: Failed with rc=%u.\n\r", str, rc); while(1) asm("wfi");}
    
  public:
    void init(void)
    {
      #if DO_DEBUG>0
        Serial.println("Using uSDFS");
      #endif
      rc = f_mount (&fatfs, (TCHAR *)_T("1:/"), 0);      /* Mount/Unmount a logical drive */
      if (rc) die((char*)"mount", rc);
    }

    void mkDir(char * dirname)
    {
      
    }

    void exit(void)
    {
      
    }
    
    void open(char * filename)
    {
      char2tchar(filename,80,wfilename);
      //
      // check status of file
      rc =f_stat(wfilename,0);
      Serial.printf("stat %d %x\n",rc,fil.obj.sclust);

      rc = f_open(&fil, wfilename, FA_WRITE | FA_CREATE_ALWAYS);
      Serial.printf(" opened %d %x\n\r",rc,fil.obj.sclust);
      // check if file is Good
      if(rc == FR_INT_ERR)
      { // only option is to close file
        rc = f_close(&fil);
        if(rc == FR_INVALID_OBJECT)
        { Serial.println("unlinking file");
          rc = f_unlink(wfilename);
          if (rc) die((char*)"unlink", rc);
        }
        else
          die((char*)"close", rc);
        
      }
      // retry open file
      rc = f_open(&fil, wfilename, FA_WRITE | FA_CREATE_ALWAYS);
      if(rc) die((char*)"open", rc);
    }
    
    void writeHeader(char * header, uint32_t ndat) 
    {
      
    }
    
    void close(void)
    {
      rc = f_close(&fil);
      if (rc) die((char*)"close", rc);
    }
    
    uint32_t write( uint8_t *buffer, uint32_t nbuf)
    {
      rc = f_write(&fil, buffer, nbuf, &wr);
      if (rc== FR_DISK_ERR) // IO error
      { uint32_t usd_error = usd_getError();
        Serial.printf(" write FR_DISK_ERR : %x\n\r",usd_error);
        // only option is to close file
        // force closing file
        return 0;
      }
      else if(rc) die((char*)"write",rc);
      return nbuf;
    }
    
    uint32_t read(uint8_t *buffer, uint32_t nbuf)
    { return 0;
    }
};
#endif
#endif
