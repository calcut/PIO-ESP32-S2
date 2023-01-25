//29/12/21 Working for repeat transfer - source of this version not clear
// Code below taken from https://gist.github.com/zonque/0ae2dc8cedbcdbd9b933
// file xymodem-mini.c

// MarkD modifcations
// - uses 128 byte packets for low RAM devices
// - supports batch upload of several files at once

//StuartsProjects - hard coded for Serial2
//080222 - added timeouts
#include "FS.h"
#include <Arduino.h>
#include <SD.h>



#define X_SOH 0x01
#define X_STX 0x02
#define X_ACK 0x06
#define X_NAK 0x15
#define X_EOT 0x04

#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial

const uint32_t transfertimeoutmS = 5000;
const uint8_t ackerrorlimit = 16;

struct yModemPacket {
  uint8_t start;
  uint8_t block;
  uint8_t block_neg;
  uint8_t payload[128];
  uint16_t crc;
} __attribute__((packed));

#define CRC_POLY 0x1021

static uint16_t crc_update(uint16_t crc_in, int incr)
{
  uint16_t _xor = crc_in >> 15;
  uint16_t _out = crc_in << 1;

  if (incr)
    _out++;

  if (_xor)
    _out ^= CRC_POLY;

  return _out;
}

static uint16_t crc16(const uint8_t *data, uint16_t size)
{
  uint16_t crc, i;

  for (crc = 0; size > 0; size--, data++)
    for (i = 0x80; i; i >>= 1)
      crc = crc_update(crc, *data & i);

  for (i = 0; i < 16; i++)
    crc = crc_update(crc, 0);

  return crc;
}


static uint16_t swap16(uint16_t in)
{
  return (in >> 8) | ((in & 0xff) << 8);
}

// Main YModem code.
// filename is pointer to null terminated string
// set waitForReceiver to 1 so that the upload begins when TeraTerm is ready
// set batchMode to 0 when sending 1 file
// set batchMode to 1 for each file sent apart from the last one.

static int yModemSend(const char *filename, int waitForReceiver, int batchMode )
{
  uint32_t numBytesStillToSend = 0;
  uint16_t numBytesThisPacket = 0;
  uint8_t  skip_payload;
  uint8_t  doNextBlock;
  uint8_t  answer = 0;
  char     spfBuff[16];
  struct   yModemPacket yPacket;
  uint32_t filebytescopied = 0;
  uint32_t startmS;
  uint8_t ackerrors = 0;

  SerialDB.println("Opening file");

  File srcFile = SD.open(filename);
  SerialDB.println("Opened file");

  
  if (srcFile < 0)
  {
    SerialDB.println("Open error");
    return 0;
  }

  numBytesStillToSend = srcFile.size();
  
  if (numBytesStillToSend == 0)
  {
  SerialDB.println("SD file error");
  return 0;
  }
  
  //convert the size of the file to an ASCII representation for header packet
  sprintf(spfBuff, "%ld", numBytesStillToSend);

  // wait here for the receiving device to respond
  if ( waitForReceiver )
  {
    SerialDB.print("Waiting for receiver ping ... ");
    while ( SerialUSB.available() ) SerialUSB.read();
    startmS = millis();

    do
    {
      if ( SerialUSB.available() ) answer = SerialUSB.read();
    }
    while ((answer != 'C') && ((uint32_t) (millis() - startmS) < transfertimeoutmS));

    if (answer != 'C')
    {
      SerialDB.println("Timeout starting YModem transfer");
      return 0;
    }
    else
    {
      SerialDB.println("done");
    }
  }

  SerialDB.print("YModem Sending ");
  SerialDB.print(filename);
  SerialDB.print("  ");
  SerialDB.print(spfBuff);
  SerialDB.println(" bytes");

  yPacket.start = X_SOH;
  yPacket.block = 0;

  // copy the filename into the payload - fill remainder of payload with 0x00
  strncpy((char *) yPacket.payload, filename, sizeof(yPacket.payload));

  // insert the file size in bytes as ASCII after the NULL of the filename string
  strcpy( (char *)(yPacket.payload) + strlen(filename) + 1 , spfBuff );

  // first pass - don't read any file data as it will overwrite the file details packet
  skip_payload = 1;

  while (numBytesStillToSend > 0)
  {
    doNextBlock = 0;

    // if this isn't the 1st pass, then read a block of up to 128 bytes from the file
    if (skip_payload == 0)
    {
      numBytesThisPacket = min(numBytesStillToSend, sizeof(yPacket.payload));
      srcFile.read(yPacket.payload, numBytesThisPacket);

      filebytescopied = filebytescopied + numBytesThisPacket;

      if (numBytesThisPacket < sizeof(yPacket.payload)) {
        // pad out the rest of the payload block with 0x1A
        memset(yPacket.payload + numBytesThisPacket, 0x1A, sizeof(yPacket.payload) - numBytesThisPacket);
      }
    }

    yPacket.block_neg = 0xff - yPacket.block;

    // calculate and insert the CRC16 checksum into the packet
    yPacket.crc = swap16(crc16(yPacket.payload, sizeof(yPacket.payload)));

    // send the whole packet to the receiver - will block here
    startmS = millis();
    SerialUSB.write( (uint8_t*)&yPacket, sizeof(yPacket));

    // wait for the receiver to send back a response to the packet
    while ((!SerialUSB.available()) && ((uint32_t) (millis() - startmS) < transfertimeoutmS));

    if ( ((uint32_t) (millis() - startmS) >= transfertimeoutmS))
    {
      SerialDB.println("Timeout waiting YModem response");
      return 0;
    }

    answer = SerialUSB.read();
    switch (answer) {
      case X_NAK:
        // something went wrong - send the same packet again?
        SerialDB.print("N");
        ackerrors++;
        break;
      case X_ACK:
        // got ACK to move to the next block of data
        SerialDB.print(".");
        doNextBlock = 1;
        break;
      default:
        // unknown response
        SerialDB.print("?");
        ackerrors++;
        break;
    }

    if (ackerrors >= ackerrorlimit)
    {
      SerialDB.println("Ack error limit reached");
      return 0;
    }

    // need to handle the 'C' response after the initial file details packet has been sent
    if (skip_payload == 1) {

      startmS = millis();
      while ((!SerialUSB.available()) && ((uint32_t) (millis() - startmS) < transfertimeoutmS));

      if ( ((uint32_t) (millis() - startmS) >= transfertimeoutmS))
      {
        SerialDB.println("Timeout waiting YModem response");
        return 0;
      }

      answer = SerialUSB.read();
      if (answer == 'C') {
        // good - start sending the data in the next transmission
        skip_payload = 0;
      } else {
        // error? send the file details packet again?
        doNextBlock = 0;
      }
    }

    // move on to the next block of data
    if (doNextBlock == 1) {
      yPacket.block++;
      numBytesStillToSend = numBytesStillToSend - numBytesThisPacket;
    }
  }

  // all done - send the end of transmission code
  SerialUSB.write( X_EOT );

  // need to send EOT again for YMODEM
  startmS = millis();
  while ((!SerialUSB.available()) && ((uint32_t) (millis() - startmS) < transfertimeoutmS));

  if ( ((uint32_t) (millis() - startmS) >= transfertimeoutmS))
  {
    SerialDB.println("Timeout waiting YModem response");
    return 0;
  }

  answer = SerialUSB.read();
  SerialUSB.write( X_EOT );

  if (batchMode == 0) {
    // and then a packet full of NULL seems to terminate the process
    // and make TeraTerm close the receive dialog box
    yPacket.block = 0;
    yPacket.block_neg = 0xff - yPacket.block;
    memset(yPacket.payload, 0x00, sizeof(yPacket.payload) );
    yPacket.crc = swap16(crc16(yPacket.payload, sizeof(yPacket.payload)));
    SerialUSB.write( (uint8_t*)&yPacket, sizeof(yPacket));
  }
  SerialDB.println("done");

  srcFile.close();
  return filebytescopied;
}
