/******************************************************************************

ESP32-CAM remote image access via FTP. Take pictures with ESP32 and upload it via FTP making it accessible for the outisde network. 
Leonardo Bispo
July - 2019
https://github.com/ldab/ESP32_FTPClient

Distributed as-is; no warranty is given.
******************************************************************************/
#include <Arduino.h>

#include <SPI.h>
#include <SD.h>

#include "Secrets.h"

#include "FS.h"
#include "SPIFFS.h"

#include "SerialTransfer.h"
#include "Wire.h"

//custom libraries
// #include "ADS1284.h"
#include "IOexp.h"

#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial

#define CpuFreqMHz 80

#define FORMAT_SPIFFS_IF_FAILED true
#define TEST_SIZE 1024*1024 * 1 //1MB

#define MAX_PACKET_SIZE 512 // for SerialTransfer

//I2C Pins
#define I2C_SDA  33
#define I2C_SCL  34

//IO Expander Interrupt
#define IO_INTERRUPT 17

//SD Card SPI pins
#define SD_MISO  21
#define SD_MOSI  16
#define SD_SCK   42
#define SD_CS    15


char workingDir[10] = "/data";

SerialTransfer myTransfer;
int i =0;

void SDinit();
void uart_send_file(const char *, File);

// SPIClass spiGEO(HSPI); // Use the 'HSPI' peripheral of ESP32-S2 for Geophones
SPIClass spiSD(FSPI); // Use the 'FSPI' peripheral of ESP32-S2 for SD Card

IOexp * IO = NULL;

void deleteFile(fs::FS &fs, const char * path){
    Serial0.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial0.println("- file deleted");
    } else {
        Serial0.println("- delete failed");
    }
}


void testFileIO(fs::FS &fs, const char * path, uint32_t bufsize){
    // Serial0.printf("Testing file I/O with %s\n", path);
   
    uint8_t buf[bufsize];

    for (uint32_t i=0; i < sizeof(buf); i++)
        buf[i] = 49;
    uint32_t chunks = TEST_SIZE / bufsize;

    const int fileSize = 2000;
    char demofile[fileSize] = "Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large language ocean. A small river named Duden flows by their place and supplies it with the necessary regelialia. It is a paradisematic country, in which roasted parts of sentences fly into your mouth. Even the all-powerful Pointing has no control about the blind texts it is an almost unorthographic life One day however a small line of blind text by the name of Lorem Ipsum decided to leave for the far World of Grammar. The Big Oxmox advised her not to do so, because there were thousands of bad Commas, wild Question Marks and devious Semikoli, but the Little Blind Text didn’t listen. She packed her seven versalia, put her initial into the belt and made herself on the way. When she reached the first hills of the Italic Mountains, she had a last view back on the skyline of her hometown Bookmarksgrove, the headline of Alphabet Village and the subline of her own road, the Line Lane. Pityful a rethoric question ran over her cheek, then she continued her way. On her way she met a copy. The copy warned the Little Blind Text, that where it came from it would have been rewritten a thousand times and everything that was left from its origin would be the word and and the Little Blind Text should turn around and return to its own, safe country. But nothing the copy said could convince her and so it didn’t take long until a few insidious Copy Writers ambushed her, made her drunk with Longe and Parole and dragged her into their agency, where they abused her for their projects again and again. And if she hasn’t been rewritten, then they are still using her. Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large language ocean. A small riEND";

    Serial0.printf("Writing");

    File file = fs.open(path, FILE_WRITE);
    uint32_t start = millis();
    


    if (file) {
        file.write((uint8_t *) &demofile, fileSize);

        // for(i=0; i<chunks; i++){
        //     if ((i & 0x001F) == 0x001F){
        //     Serial0.print(".");
        //     }
        //     file.write(buf, 512);
        // }
    } else Serial.print(F("SD Card: error on opening file"));

    
    file.close();

    uint32_t end = millis() - start;

    uint32_t rate = TEST_SIZE/end*1000/1024;
    Serial0.printf("- %u bytes written in %u ms, ", chunks * bufsize, end);
    Serial0.printf("- Write Rate = %u KB/s\n", rate);
    // file.close();




    // Serial0.printf("Reading");

    // size_t len = 0;
    // file = fs.open(path);
    // uint8_t buf[fileSize];

    // start = millis();
    // end = start;
    // // i = 0;
    // if(file && !file.isDirectory()){
    //     len = file.size();
    //     size_t flen = len;
    //     start = millis();
    //     // Serial0.print("- reading" );
    //     while (file.available()) {
    //         file.read(buf, bufsize);
    //         Serial0.printf("buf=%s\n\n", buf);
    //     }
    // }
    // file.close();



    //     end = millis() - start;
    //     rate = flen/end*1000/1024;
    //     // Serial0.printf("- %u bytes read in %u ms, ", flen, end);
    //     Serial0.printf("- Read Rate = %u KB/s\n", rate);
    //     file.close();
    // } else {
    //     Serial0.println("- failed to open file for reading");
    // }

    // Send over USB Serial
    file = fs.open(path);
    start = millis();
    end = start;

    // i = 0;
    uart_send_file("test.txt", file);

    // if(file && !file.isDirectory()){
    //     len = file.size();
    //     size_t flen = len;
    //     start = millis();
    //     // Serial0.print("- reading" );
    //     while (file.available()) {
    //         file.read(buf, bufsize);
    //         SerialUSB.printf("%s", buf);
    //         // SerialUSB.printf("block%d", i)
    //     }
    end = millis() - start;
    rate = file.size()/end*1000/1024;
    // // Serial0.printf("- %u bytes read in %u ms, ", flen, end);
    Serial0.printf("- Serial Write Rate = %u KB/s\n", rate);
    file.close();
    // } else {
    //     Serial0.println("- failed to open file for reading");
    // }
}

void uart_send_file(const char * fileName, File file){
 
  char fname[8] = "tet.txt";
//   char * fname = "tet.txt";
//   myTransfer.sendDatum(fileName); // Send filename
  myTransfer.sendDatum(fname); // Send filename
  size_t fileSize = file.size();
  uint16_t numPackets = fileSize / (MAX_PACKET_SIZE - 4); // Reserve 4 bytes for current file index
  
  if (fileSize % MAX_PACKET_SIZE){ // Add an extra transmission if needed
    SerialDB.println("EXTRA PACKET USED");
    numPackets++;
    }


  uint32_t dataLen = MAX_PACKET_SIZE - 4;
  uint8_t buf[dataLen];

  SerialDB.printf("Serial Transfer, numpackets = %d\n", numPackets);
  
  for (uint16_t i=0; i<numPackets; i++) // Send all data within the file across multiple packets
  {
    uint32_t fileIndex = i * (MAX_PACKET_SIZE - 4); // Determine the current file index
    
    if ((fileIndex + (MAX_PACKET_SIZE - 4)) > fileSize){ // Determine data length for the last packet if file length is not an exact multiple of MAX_PACKET_SIZE-2
    //   Serial0.printf("Last Packet!");
      dataLen = fileSize - fileIndex;
    }
    
    file.read(buf, dataLen);
    Serial0.printf("buf=%s\n\n", buf);

    uint32_t sendSize_index = myTransfer.txObj(fileIndex); // Stuff the current file index
    uint32_t sendSize_packet = myTransfer.txObj(buf[0], sendSize_index, dataLen); // Stuff the current file data
    Serial0.printf("loop=%d, fileindex=%d, sendsize_index=%d, sendsize_packet=%d, datalen=%d, fileSize=%d\n", i, fileIndex, sendSize_index, sendSize_packet, dataLen, fileSize);
    
    myTransfer.sendData(sendSize_packet, 1); // Send the current file index and data
  }
}

void SDinit() {
    // Display Card Size
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    SerialDB.printf("SD Card Size: %lluMB\n", cardSize);

    File root = SD.open("/");
    if(!root){
        SerialDB.println("Failed to open SD Card");
        SerialDB.println("Rebooting in 5 seconds");
        delay(5000);
        // A reboot sometimes allows the SD card to mount correctly, unsure why.
        // ESP.restart(); //This puts it into bootloader mode, so no use
        return;
    }
    if(!root.isDirectory()){
        SerialDB.println("Not a directory");
        return;
    }
    SerialDB.println("Listing root of SD Card:");
    File file = root.openNextFile();
    while(file){
        SerialDB.println(file.name());
        file = root.openNextFile();
    }

    if(SD.mkdir(workingDir)) {
        SerialDB.print("Created Empty Directory: ");
        SerialDB.println(workingDir);
    }
    else {
        SerialDB.print("Error creating Directory: ");
        SerialDB.println(workingDir);
    }


}

void setup()
{
  SerialDB.begin( 115200 );
  SerialUSB.begin();

  myTransfer.begin(SerialUSB);

  IO = new IOexp(&Serial0, I2C_SDA, I2C_SCL, IO_INTERRUPT);
  SerialDB.println("IO Expander All Disabled");
  IO->disable_all();  //Disable GPS and ADC


  setCpuFrequencyMhz(CpuFreqMHz);
  SerialDB.println("delay 5s to measure power at 80MHz CPU, before wifi");

  
  // Init SPIFFS
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
  Serial0.println("SPIFFS Mount Failed");

  }

  // Init SD Card
  SerialDB.println("Initializing SD card...");


  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS); 
  if (SD.begin(SD_CS, spiSD)) {
      SerialDB.println("SD Card initialised");
  } else SerialDB.println("Failed to init SD");

  SDinit();
    delay(5000);

    // int bufsize = 2048;
    // Serial0.printf("\nSD test at bufsize %u bytes", bufsize);
    // testFileIO(SD, "/test.txt", bufsize);

    // Serial0.printf("\nSPIFFS test at bufsize %u bytes", bufsize);
    // testFileIO(SPIFFS, "/test.txt", bufsize);

}
void loop()
{
    // int bufsize = 512;
    // testFileIO(SD, "/test.txt", bufsize); // char filename[40];
    int bufsize = 2048;
    Serial0.printf("\nSD test at bufsize %u bytes", bufsize);
    testFileIO(SD, "/test.txt", bufsize); // char filename[40];
    delay(5000);

    // Serial0.printf("\nSPIFFS test at bufsize %u bytes", bufsize);
    // testFileIO(SPIFFS, "/test.txt", bufsize); // char filename[40];



    SerialDB.println("looping");
    delay(5000);
    // sprintf(filename, "%s/data.txt", workingDir );
    // File file = SD.open(filename, FILE_APPEND);
    // file.printf("data%d\n", i);
    // SerialDB.printf("data%d\n", i);

    // i++;

}


