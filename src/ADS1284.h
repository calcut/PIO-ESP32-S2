#ifndef ADS1284_h
#define ADS1284_h

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 2500 //10s worth at 250sps
#endif


#include "Arduino.h"
#include <RingBuf.h>
#include "SPI.h"


 struct ADCconf{
    
    byte SYNC = B0; 
        // Synchronization mode bit.
        // 0: Pulse-sync mode (default) 
        // 1: Continuous-sync mode
    
    byte MODE = B1;
        // Mode Control
        // 0: Low-power mode
        // 1: High-resolution mode (default)

    byte DR = B000; //Set to Non-default 250SPS
        //Data Rate
        // 000: 250 SPS
        // 001: 500 SPS
        // 010: 1000 SPS (default)
        // 011: 2000 SPS
        // 100: 4000 SPS

    byte PHASE = B0; 
        //FIR phase response bit.
        // 0: Linear phase (default)
        // 1: Minimum phase

    byte FILTR = B10;
        // Digital filter configuration bits.
        // 00: Reserved
        // 01: Sinc filter block only
        // 10: Sinc + LPF filter blocks (default) 
        // 11: Sinc + LPF + HPF filter blocks

    byte MUX = B000;
        // MUX select bits.
        // 000: AINP1 and AINN1 (default)
        // 001: AINP2 and AINN2
        // 010: Internal short through 400-Ω resistor
        // 011: AINP1 and AINN1 connected to AINP2 and AINN2 
        // 100: External short to AINN2
    
    byte CHOP = B1;
        // PGA chopping enable bit.
        // 0: PGA chopping disabled
        // 1: PGA chopping enabled (default)

    byte PGA = B000;
        // PGA gain select bits.
        // 000: G = 1 (default)
        // 001: G = 2
        // 010: G = 4
        // 011: G = 8
        // 100: G = 16
        // 101: G = 32
        // 110: G = 64

    // NB, High Pass Filter (HPF), Calibration and OFFSET registers not 
    // included, could be added if they are required.
  };

class ADS1284
{
  public:
    ADS1284(Stream *serialport, SPIClass *spi, int chipselect, int sync, int ADC, ADCconf conf);
    void configure(ADCconf conf);
    int readData();
    RingBuf<int, BUFFER_SIZE> data;

    // RingBufCPP<uint32_t, BUFFER_SIZE> microseconds;
    // CircularBuffer<int, BUFFER_SIZE> data;
    // CircularBuffer<uint32_t, BUFFER_SIZE> microseconds;
    byte readRegister(byte thisRegister);
    void writeRegister(byte thisRegister, byte thisValue);
    void writeCommand(byte thisCommand);
    void WAKEUP();
    void STANDBY();
    void SYNC();
    void RESET();
    void RDATAC();
    void SDATAC();
    void RDATA();
    void calibrateOffset();
    void calibrateGain();
    bool BufferCheck();
    void BufferPrint();
    bool bufferOK = true;
  private:
    int _chipselect;
    int _sync;
    int _ADC;
    int _fullscale_p = 0x7FFFFFFF;
    int _fullscale_n = 0x80000000;
    // int _samplecount = 0;

    // Store register value to be written/read 
    byte _config0; 
    byte _config1; 

    byte _config0_read;
    byte _config1_read;

    //uninitalised pointers to SPI and Serial objects
    SPIClass * _spi = NULL;
    Stream * _serialport = NULL;
    
    static const int spiClk = 1000000; // 1 MHz
};

#endif
