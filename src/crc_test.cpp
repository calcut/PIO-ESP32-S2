
#include <Arduino.h>
#include <Wire.h>

#include <SPI.h>
#include <FS.h>
#include "SPIFFS.h"
#include <PSRamFS.h>

#include <SD.h>

#include <CRC32.h>


CRC32 crc;

//SD Card SPI pins
// For WOKWI WROOM
// #define SD_MISO  37
// #define SD_MOSI  35
// #define SD_SCK   36
// #define SD_CS    34

// For METRO WROVER
#define SD_MISO  21
#define SD_MOSI  16
#define SD_SCK   42
#define SD_CS    15

#define FORMAT_SPIFFS_IF_FAILED true

#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial

File root;
SPIClass spiSD(FSPI); // Use the 'FSPI' peripheral of ESP32-S2 for SD Card

void SDinit();
// void SDwriteData();
void testFileIO(fs::FS &fs, uint32_t bufsize, uint32_t filesize);
void SendCRC(fs::FS &fs, char filename[]);

void setup(void) {
  SerialDB.begin(115200);
  while(!SerialDB)

  SerialDB.println(" Connected!");
  SerialDB.println("Initializing SD card...");

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS); 
  if (SD.begin(SD_CS, spiSD)) {
      SerialDB.println("SD Card initialised");
  } else SerialDB.println("Failed to init SD");

  SDinit();

//   Init SPIFFS
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
    SerialDB.println("SPIFFS Mount Failed");
  }

  if(!PSRamFS.begin()){
    SerialDB.println("PSRamFS Mount Failed");
  }

  uint32_t bufsize; //must be a factor of filesize
//   uint32_t filesize = 1024*1024 * 1; //1MB
  uint32_t filesize = 1024*512 * 1; //1MB

  SerialDB.printf("\nSD test");
  for (bufsize = 512; bufsize <=2048; bufsize = bufsize*2){
    SerialDB.printf("\nbufsize %u bytes", bufsize);
    testFileIO(SD, bufsize, filesize);
  }

  SerialDB.printf("\nSPIFFS test");
  for (bufsize = 512; bufsize <=2048; bufsize = bufsize*2){
    SerialDB.printf("\nbufsize %u bytes", bufsize);
    testFileIO(SPIFFS, bufsize, filesize);
  }

  SerialDB.printf("\nPSRAM test (unstable, may not be set up properly)");
  bufsize= 512;
  SerialDB.printf("\nbufsize %u bytes", bufsize);
  testFileIO(PSRamFS, bufsize, filesize);
}

void loop(void) {
  delay(10000);
}

void testFileIO(fs::FS &fs, uint32_t bufsize, uint32_t filesize){

    uint8_t buf[bufsize];
    uint32_t chunks = filesize / bufsize;
    char path[] = "/test.txt" ;

    // Fill buffer with ascii 1s
    for (uint32_t i=0; i < sizeof(buf); i++)
        buf[i] = 49;

// File Write Test
    SerialDB.print("\n- Writing");
    uint32_t start = millis();
    
    File file = fs.open(path, FILE_WRITE);
    
    if (file) {
      for(uint32_t i=0; i<chunks; i++){
        // if ((i & 0x001F) == 0x001F){
        //     SerialDB.print(".");
        // }
        file.write(buf, bufsize);
        }
    } else SerialDB.print(F("Error on opening file"));

    file.close();
    uint32_t end = millis() - start;

    uint32_t rate = filesize/end*1000/1024;
    SerialDB.printf("- %u bytes written in %u ms, ", chunks * bufsize, end);
    SerialDB.printf("- Write Rate = %u KB/s\n", rate);

// File Read Test
    uint32_t dataLen = bufsize;
    SerialDB.print("- Reading" );

    start = millis();
    file = fs.open(path);

    while (file.available()){
        if (file.available() > bufsize){
            dataLen = bufsize;
        }
        else{
            dataLen = file.available();
        }
        file.read(buf, dataLen);
    }
    end = millis() - start;
    rate = filesize/end*1000/1024;
    Serial0.printf("- %u bytes read in %u ms, ", filesize, end);
    Serial0.printf("- Read Rate = %u KB/s\n", rate);

    SendCRC(fs, path);
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
}

// void SDwriteData(){
   
//     char FileName[] = "/demofile.txt";
//     char demofile[] = "Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large language ocean. A small river named Duden flows by their place and supplies it with the necessary regelialia. It is a paradisematic country, in which roasted parts of sentences fly into your mouth. Even the all-powerful Pointing has no control about the blind texts it is an almost unorthographic life One day however a small line of blind text by the name of Lorem Ipsum decided to leave for the far World of Grammar. The Big Oxmox advised her not to do so, because there were thousands of bad Commas, wild Question Marks and devious Semikoli, but the Little Blind Text didn't listen. She packed her seven versalia, put her initial into the belt and made herself on the way. When she reached the first hills of the Italic Mountains, she had a last view back on the skyline of her hometown Bookmarksgrove, the headline of Alphabet Village and the subline of her own road, the Line Lane. Pityful a rethoric question ran over her cheek, then she continued her way. On her way she met a copy. The copy warned the Little Blind Text, that where it came from it would have been rewritten a thousand times and everything that was left from its origin would be the word and and the Little Blind Text should turn around and return to its own, safe country. But nothing the copy said could convince her and so it didn't take long until a few insidious Copy Writers ambushed her, made her drunk with Longe and Parole and dragged her into their agency, where they abused her for their projects again and again. And if she hasn't been rewritten, then they are still using her. Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large language ocean. A small riEND";
//     // char demofile[] = "Hello World";
//     uint32_t fileSize = strlen(demofile);

//     SerialDB.printf("Writing\n");

//     File file = SD.open(FileName, FILE_WRITE);
    
//     if (file) {
//         file.write((uint8_t *) &demofile, fileSize);
//         SerialDB.printf("\nWrote %d bytes to %s\n", fileSize, FileName);

//     } else SerialDB.print(F("SD Card: error on opening file"));
//     file.close();

//     // Write a large file to the SD card
//     char FileName2[] = "/bigfile.txt";
//     fileSize = 1024*1024 * 1; //1MB
//     uint32_t bufsize = 2048;
//     uint8_t buf[bufsize];
//     uint32_t chunks = fileSize / bufsize;

//     for (uint32_t i=0; i < sizeof(buf); i++)
//         buf[i] = 49;
//     file = SD.open(FileName2, FILE_WRITE);
    
//     if (file) {
//       for(uint32_t i=0; i<chunks; i++){
//         if ((i & 0x001F) == 0x001F){
//             SerialDB.print(".");
//         }
//         file.write(buf, bufsize);
//         }
//         SerialDB.printf("\nWrote %d bytes to %s\n", chunks*bufsize, FileName2);

//     } else SerialDB.print(F("SD Card: error on opening file"));
//     file.close();
// }


void SendCRC(fs::FS &fs, char filename[]){

    crc.reset();
    uint32_t bufsize = 2048;
    uint8_t buf[bufsize];

    uint32_t start = millis();
    File file = fs.open(filename);

    uint32_t fileSize = file.size();
    uint32_t dataLen = bufsize;

    // SerialDB.printf("\nTransferring with CRC %s, %d bytes\n", filename, fileSize);
    SerialUSB.printf("Incomingbytes = %u\n", fileSize);

    while (file.available()){
        // loop += 1;
        // SerialDB.println(loop);

        if (file.available() > bufsize){
            dataLen = bufsize;
        }
        else{
            dataLen = file.available();
        }
        file.read(buf, dataLen);
        crc.update(buf,dataLen);
        // SerialUSB.printf("%.*s", dataLen, buf);
        SerialUSB.write(buf, dataLen);
        // Serial.print()

    }
    SerialUSB.printf("\n");

    uint32_t checksum = crc.finalize();
    uint32_t end = millis() - start;
    uint32_t rate = fileSize/end*1000/1024;
    SerialUSB.printf("checksum = 0x%08X, filename = %s\n", checksum, filename);
    // SerialDB.printf("- checksum = 0x%08X, filename = %s\n", checksum, filename);
    SerialDB.printf("- USB transfer in %u ms, %u KB/s\n", end, rate);

    file.close();
}
