
#include "Arduino.h"
#include "ADS1284.h"
#include <SPI.h>


ADS1284::ADS1284(Stream *serialport, SPIClass *spi, int chipselect, int sync, int ADC, ADCconf conf)
{
    
    _serialport = serialport;
    _chipselect = chipselect;
    _spi = spi;
    _sync = sync;
    _ADC = ADC;

    pinMode(_chipselect, OUTPUT);
    pinMode(sync, OUTPUT);

    // struct ADCconf default_conf;
    // configure(default_conf);
    configure(conf);
}

void ADS1284::configure(ADCconf conf) {

    //Unpack ADC config structure and combine into register values
    _config0 = ((conf.SYNC<<7)|(conf.MODE<<6)|(conf.DR<<3)|(conf.PHASE<<2)|conf.FILTR);
    _config1 = ((conf.MUX<<4)|(conf.CHOP<<3)|conf.PGA);

    digitalWrite(_chipselect, HIGH);
    delay(1);
    digitalWrite(_chipselect, LOW);
    delay(1);
    digitalWrite(_chipselect, HIGH);
    delay(1);

    _serialport->print("Configuring Registers on ADC");
    _serialport->println(_ADC);
  
    SDATAC(); // Stop continuous data before register config.
    delay(10); // probably not needed
    writeRegister(0x01, _config0);
    writeRegister(0x02, _config1);

    _config0_read = readRegister(0x01);
    _serialport->print("Config0 reads as: 0x");
    _serialport->println(_config0_read, HEX);
    _config1_read = readRegister(0x02);
    _serialport->print("Config1 reads as: 0x");
    _serialport->println(_config1_read, HEX);

    if (_config0_read != _config0) {
      _serialport->println("*** Error with Config0 register write ***");
      delay(5000);
    }
    if (_config1_read != _config1) {
      _serialport->println("*** Error with Config1 register write ***");
      delay(5000);
    }

    RDATAC(); // Resume continuous data after register write

    //Calibrate offset (ideally this should be done with inputs shorted)

    //For offset calibration, want to short ADC inputs together using the MUX
    byte _config1_cal = ((B010<<4)|(conf.CHOP<<3)|conf.PGA);
    writeRegister(0x02, _config1_cal);
    
    calibrateOffset();

    //Return the configuration to the previous (unshorted) settings.
    writeRegister(0x02, _config1);
}

byte ADS1284::readRegister(byte thisRegister) {

  byte result = 0;           // incoming byte from the SPI
  byte bytesToRead = 0x00; // Just read a single byte register

  _serialport->print("Register Read, ADC");
  _serialport->print(_ADC);
  _serialport->print(", Register 0x");
  _serialport->print(thisRegister, HEX);
  
  //From ADS1284 Datasheet, need 0x20 before register address to signal a read
  thisRegister = 0x20 | thisRegister;

  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
//   take the chip select low to select the device:
  digitalWrite(_chipselect, LOW);

  _spi->transfer(thisRegister);
  _spi->transfer(bytesToRead);
  
  // send a value of 0 to read the first byte returned:
  result = _spi->transfer(0x00);

  // take the chip select high to de-select:
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
  

  _serialport->print(", Value: 0x"); 
  _serialport->println(result, HEX);

  return (result);
}

void ADS1284::writeRegister(byte thisRegister, byte thisValue) {

  //Typially bytesToWrite will be 0x00, for a single register write
  byte bytesToWrite = 0x00;

  _serialport->print("Register Write, ADC");
  _serialport->print(_ADC);
  _serialport->print(", Register 0x");
  _serialport->print(thisRegister, HEX);
  _serialport->print(", Value: 0x"); 
  _serialport->println(thisValue, HEX);

  //From ADS1284 Datasheet, need 0x40 before register address to signal a write
  thisRegister = 0x40 | thisRegister;
  
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(thisRegister);
  _spi->transfer(bytesToWrite);
  _spi->transfer(thisValue);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

void ADS1284::WAKEUP() {
  _serialport->print("ADC");
  _serialport->print(_ADC);
  _serialport->println(" WAKEUP Command");
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(0x00);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

void ADS1284::STANDBY() {
  _serialport->print("ADC");
  _serialport->print(_ADC);
  _serialport->println(" STANDBY Command");
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(0x02);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

void ADS1284::SYNC() {
  _serialport->print("ADC");
  _serialport->print(_ADC);
  _serialport->println(" SYNC Command");
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(0x04);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

void ADS1284::RESET() {
  _serialport->print("ADC");
  _serialport->print(_ADC);
  _serialport->println(" RESET Command");
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(0x06);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

void ADS1284::RDATAC() {
  _serialport->print("ADC");
  _serialport->print(_ADC);
  _serialport->println(" RDATAC Command (Resume Continuous Data)");
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(0x10);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

void ADS1284::SDATAC() {
  _serialport->print("ADC");
  _serialport->print(_ADC);
  _serialport->println(" SDATAC Command (Stop Continuous Data)");
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(0x11);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

void ADS1284::RDATA() {
  _serialport->print("ADC");
  _serialport->print(_ADC);
  _serialport->println(" RDATA (Read data by command)");
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(0x12);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

void ADS1284::writeCommand(byte thisCommand) {
  _serialport->print("ADC");
  _serialport->print(_ADC);
  _serialport->print("SPI Command : ");
  _serialport->println(thisCommand, HEX);
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);
  _spi->transfer(thisCommand);
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();
}

int ADS1284::readData() {

  // a byte array to be used for SPI transaction buffer
  byte ADCraw[4] = {0x00, 0x00, 0x00, 0x00};

  //Prepare SPI transaction
  _spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(_chipselect, LOW);

  // Perform a 4 byte SPI transfer, result is saved in ADCraw
  _spi->transfer(ADCraw, 4);
  
  // Combine the bytes in correct order to form a 32bit signed integer
  int ADCdata = ((int32_t)((ADCraw[0]<<24)|(ADCraw[1]<<16)|(ADCraw[2]<<8)|(ADCraw[3])));
    
  // _samplecount +=1;
  // bufferOK = data.push(_samplecount); 
  bufferOK = data.push(ADCdata); 
  // bufferOK = data.add(ADCdata); 


  // take the chip select high to de-select:
  digitalWrite(_chipselect, HIGH);
  _spi->endTransaction();

  return bufferOK;

}

bool ADS1284::BufferCheck() {
  if (!bufferOK){
      _serialport->print("ADC");
      _serialport->print(_ADC);
      _serialport->println(" Warning, Buffer Overrun");
      // data.clear(); // for debug, to avoid lots of warnings // NEED TO do
      // something here
      bufferOK=true;  //to avoid repeated warnings if called rapidly
  }

  return bufferOK;
}

void ADS1284::BufferPrint() {

    // Only print the first sample... too much otherwise!
    for (int i = 0; i < 1; i++)
    // for (int i = 0; i < data.size(); i++)
    {
        //  int ADCsample = data[i];
        // float ADCsample_dB = (20 * log10(((float)abs(data[i]))/((float)_fullscale_p)));


        //Format the output for fixed with printing
        char textbuf[40];
        // sprintf(textbuf, "ADC %d   int %06d   %06.2f dB",
          // _ADC, data[i], ADCsample_dB);
        // _serialport->println(textbuf);
    }
}

void ADS1284::calibrateOffset() {

  digitalWrite(_sync, HIGH); // SYNC Pin must be high during calibration

  writeCommand(0x11);//Stop continuous data read (SDATAC)
  writeCommand(0x04); //Resynchronise ADC (SYNC)
  writeCommand(0x10); //Resume continuous data read (RDATAC)

  delay(400); // should really be until DRDY goes low, or 64 data periods

  writeCommand(0x11); //Stop continuous data read (SDATAC)
  writeCommand(0x60); //Offset Calibration Command (OFSCAL)
  writeCommand(0x10); //Resume continuous data read (RDATAC)

  delay(100); // should really be 16 data periods

  digitalWrite(_sync, LOW); // SYNC Pin must be high during calibration

}

void ADS1284::calibrateGain() {

    _serialport->println("Function not yet implemented!");

}
