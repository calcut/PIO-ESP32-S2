
#include <Arduino.h>
#include <Wire.h>

#include <SPI.h>
#include <FS.h>
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

#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial

File root;
SPIClass spiSD(FSPI); // Use the 'FSPI' peripheral of ESP32-S2 for SD Card

void SDinit();
void SDwriteData();
void GetCRC(char FileName[]);

char workingDir[10] = "/data";


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
  SDwriteData();
  char FileName[] = "/demofile.txt";
  GetCRC(FileName);
//   char FileName2[] = "Photos-001";
  char FileName2[] = "/Photos-001.zip";
  GetCRC(FileName2);


}

void loop(void) {
  delay(10000);
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

void SDwriteData(){
   
    char FileName[] = "/demofile.txt";
    char demofile[] = "Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large language ocean. A small river named Duden flows by their place and supplies it with the necessary regelialia. It is a paradisematic country, in which roasted parts of sentences fly into your mouth. Even the all-powerful Pointing has no control about the blind texts it is an almost unorthographic life One day however a small line of blind text by the name of Lorem Ipsum decided to leave for the far World of Grammar. The Big Oxmox advised her not to do so, because there were thousands of bad Commas, wild Question Marks and devious Semikoli, but the Little Blind Text didn't listen. She packed her seven versalia, put her initial into the belt and made herself on the way. When she reached the first hills of the Italic Mountains, she had a last view back on the skyline of her hometown Bookmarksgrove, the headline of Alphabet Village and the subline of her own road, the Line Lane. Pityful a rethoric question ran over her cheek, then she continued her way. On her way she met a copy. The copy warned the Little Blind Text, that where it came from it would have been rewritten a thousand times and everything that was left from its origin would be the word and and the Little Blind Text should turn around and return to its own, safe country. But nothing the copy said could convince her and so it didn't take long until a few insidious Copy Writers ambushed her, made her drunk with Longe and Parole and dragged her into their agency, where they abused her for their projects again and again. And if she hasn't been rewritten, then they are still using her. Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large language ocean. A small riEND";
    // char demofile[] = "Hello World";
    uint32_t fileSize = strlen(demofile);

    SerialDB.printf("Writing\n");

    File file = SD.open(FileName, FILE_WRITE);
    
    if (file) {
        file.write((uint8_t *) &demofile, fileSize);
        SerialDB.printf("\nWrote %d bytes to %s\n", fileSize, FileName);

    } else SerialDB.print(F("SD Card: error on opening file"));
    file.close();

    // Write a large file to the SD card
    char FileName2[] = "/bigfile.txt";
    fileSize = 1024*1024 * 1; //1MB
    uint32_t bufsize = 2048;
    uint8_t buf[bufsize];
    uint32_t chunks = fileSize / bufsize;

    for (uint32_t i=0; i < sizeof(buf); i++)
        buf[i] = 49;
    file = SD.open(FileName2, FILE_WRITE);
    
    if (file) {
      for(uint32_t i=0; i<chunks; i++){
        if ((i & 0x001F) == 0x001F){
            SerialDB.print(".");
        }
        file.write(buf, bufsize);
        }
        SerialDB.printf("\nWrote %d bytes to %s\n", chunks*bufsize, FileName2);

    } else SerialDB.print(F("SD Card: error on opening file"));
    file.close();
}

void GetCRC(char FileName[]){

    crc.reset();
    uint32_t bufsize = 2048;
    uint8_t buf[bufsize];

    uint32_t start = millis();



    File file = SD.open(FileName);

    uint32_t fileSize = file.size();
    uint32_t dataLen = bufsize;
    // uint32_t loop = 0;
    SerialDB.printf("\nTransferring %s, (%d bytes)\n", FileName, fileSize);
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
    SerialUSB.printf("checksum = 0x%08X, filename = %s\n", checksum, FileName);
    SerialDB.printf("checksum = 0x%08X, filename = %s\n", checksum, FileName);
    SerialDB.printf("- Transfer took %u ms, %u KB/s\n", end, rate);

    file.close();
}
