#ifndef IOexp_h
#define IOexp_h

#include "Arduino.h"

class IOexp
{
  public:
    IOexp(Stream *serialport, int SDA, int SCL, int interrupt);
//     void init(;
    void enable_all();
    void enable_ADC0();
    void enable_ADCs();
    void disable_all();
    void read_IOexpander();


  private:
    //uninitalised pointer and Serial object
    Stream * _serialport = NULL;
};


#endif
