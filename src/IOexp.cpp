#include "Arduino.h"
#include "IOexp.h"
#include "Wire.h"

IOexp::IOexp(Stream *serialport, int SDA, int SCL, int interrupt){ 
  
  _serialport = serialport;

  Wire.begin(SDA, SCL);
  Wire.beginTransmission(0x20);  // setup out direction registers
  Wire.write(0x06);  // pointer
  Wire.write(0x00);  // DDR Port0 all output
  Wire.write(0xFF);  // DDR Port1 all input
  Wire.endTransmission(); 
  _serialport->println("IO Expander Configured");
}

void IOexp::enable_all()
{
  _serialport->println("Enabling everything via IO expander");
  Wire.beginTransmission(0x20);  // 
  Wire.write(0x02);  // pointer to Port0 Output
  Wire.write(0x3F);  // Enable all, including GPS
  Wire.endTransmission(); 
}

void IOexp::enable_ADCs()
{
  _serialport->println("Enabling ADCs but not GPS");
  Wire.beginTransmission(0x20);  // 
  Wire.write(0x02);  // pointer to Port0 Output
  Wire.write(0x7F);  // Enable all, including GPS
  Wire.endTransmission(); 
}

void IOexp::enable_ADC0()
{
  _serialport->println("Enabling ADC0 Only");
  Wire.beginTransmission(0x20);  // 
  Wire.write(0x02);  // pointer to Port0 Output
  Wire.write(0x4F);  // Enable LDO, CLK and ADC0
  Wire.endTransmission(); 
}

void IOexp::disable_all()
{
  _serialport->println("Disabling everything via IO expander");
  Wire.beginTransmission(0x20);  // 
  Wire.write(0x02);  // pointer to Port0 Output
  Wire.write(0x40);  // Disable all, including GPS
  Wire.endTransmission(); 
  }  

void IOexp::read_IOexpander()
{
  Wire.beginTransmission(0x20);  
  Wire.write(0);  // set data pointer
  Wire.endTransmission();
  Wire.requestFrom(0x20, 2);

  byte b0 = Wire.read();    // receive a byte as character
  byte b1 = Wire.read(); 
  delay(200);
  _serialport->print("Reading Port0: 0x");
  _serialport->println(b0, HEX);
  _serialport->print("Reading Port1: 0x");
  _serialport->println(b1, HEX);
}
