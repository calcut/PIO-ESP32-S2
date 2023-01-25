/*******************************************************************************************************
  Programs for Arduino - Copyright of the author Stuart Robinson - 14/03/22
  This program is supplied as is, it is up to the user of the program to decide if the program is
  suitable for the intended purpose and free from errors.
*******************************************************************************************************/

/*******************************************************************************************************
  Program Operation - This is a program that transfers a file from an Arduinos SD card to a folder on a
  PC that is connected to the Arduino via a Serial port. The transfer process uses the YModem protocol
  and the PC receives the file using the Tera Term Serial terminal program.
  This program was run on an Arduino DUE, with Serial2 used as the transfer port to the PC.
  Instructions for using the program are to be found here;
  https://stuartsprojects.github.io/2021/12/28/Arduino-to-PC-File-Transfers.html
  Serial monitor baud rate is set at 115200.
*******************************************************************************************************/

#include <Arduino.h>
#include <SPI.h>
#include "FS.h"
#include <SD.h>
#include "YModem.h"
#include "Wire.h"

#define TEST_SIZE 1024*1024 * 1 //1MB

//SD Card SPI pins
#define SD_MISO  21
#define SD_MOSI  16
#define SD_SCK   42
#define SD_CS    15

#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial

File root;
SPIClass spiSD(FSPI); // Use the 'FSPI' peripheral of ESP32-S2 for SD Card


const uint16_t SDCS = 30;
const uint16_t LED1 = 8;                    //LED indicator etc. on during transfers
char FileName[] = "/test.txt";            //file length ? bytes, file CRC ?
uint16_t transferNumber = 0;;
uint32_t filelength;
uint32_t bytestransfered;

void SDinit();
char workingDir[10] = "/data";


void setup()
{
  // pinMode(LED1, OUTPUT);                          //setup pin as output for indicator LED
  // led_Flash(2, 125);                              //two quick LED flashes to indicate program start

  SerialDB.begin(115200);
  SerialUSB.begin();                          //Serial port used for file transfers
  SerialDB.println();
  SerialDB.println(F(__FILE__));
  SerialDB.flush();
  SerialDB.println("Using Serial2 for YModem comms @ 115200 baud.");
  SerialUSB.println("Using Serial2 for YModem comms @ 115200 baud.");
  SerialDB.println("Initializing SD card...");

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS); 
  if (SD.begin(SD_CS, spiSD)) {
      SerialDB.println("SD Card initialised");
  } else SerialDB.println("Failed to init SD");

  SDinit();

    const int fileSize = 2000;
    char demofile[fileSize] = "Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large language ocean. A small river named Duden flows by their place and supplies it with the necessary regelialia. It is a paradisematic country, in which roasted parts of sentences fly into your mouth. Even the all-powerful Pointing has no control about the blind texts it is an almost unorthographic life One day however a small line of blind text by the name of Lorem Ipsum decided to leave for the far World of Grammar. The Big Oxmox advised her not to do so, because there were thousands of bad Commas, wild Question Marks and devious Semikoli, but the Little Blind Text didn’t listen. She packed her seven versalia, put her initial into the belt and made herself on the way. When she reached the first hills of the Italic Mountains, she had a last view back on the skyline of her hometown Bookmarksgrove, the headline of Alphabet Village and the subline of her own road, the Line Lane. Pityful a rethoric question ran over her cheek, then she continued her way. On her way she met a copy. The copy warned the Little Blind Text, that where it came from it would have been rewritten a thousand times and everything that was left from its origin would be the word and and the Little Blind Text should turn around and return to its own, safe country. But nothing the copy said could convince her and so it didn’t take long until a few insidious Copy Writers ambushed her, made her drunk with Longe and Parole and dragged her into their agency, where they abused her for their projects again and again. And if she hasn’t been rewritten, then they are still using her. Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large language ocean. A small riEND";

    SerialDB.printf("Writing");

    uint32_t bufsize = 2048;
    uint8_t buf[bufsize];

    for (uint32_t i=0; i < sizeof(buf); i++)
        buf[i] = 49;
    uint32_t chunks = TEST_SIZE / bufsize;

    File file = SD.open(FileName, FILE_WRITE);
    
    if (file) {

      // for(uint32_t i=0; i<chunks; i++){
      //   if ((i & 0x001F) == 0x001F){
      //   Serial0.print(".");
      //   }
      //   file.write(buf, bufsize);
      //   }

        file.write((uint8_t *) &demofile, fileSize);
        SerialDB.printf("Wrote %d bytes to %s\n", fileSize, FileName);

    } else SerialDB.print(F("SD Card: error on opening file"));
    file.close();
    delay(5000);


}
void loop()
{
  transferNumber++;
  digitalWrite(LED1, HIGH);
  SerialDB.print(F("Start transfer "));
  SerialDB.print(transferNumber);
  SerialDB.print(F("  "));
  SerialDB.println(FileName);
  SerialDB.flush();
  digitalWrite(LED1, HIGH);
  
  bytestransfered = yModemSend(FileName, 1, 1);     //transfer filename with waitForReceiver and batchMode options set
  
  if (bytestransfered > 0)
  {
  SerialDB.print(F("YModem transfer completed "));
  SerialDB.print(bytestransfered);
  SerialDB.println(F(" bytes sent"));
  }
  else
  {
  SerialDB.println(F("YModem transfer FAILED"));
  }
  SerialDB.println();
  
  digitalWrite(LED1, LOW);
  SerialDB.println();
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

