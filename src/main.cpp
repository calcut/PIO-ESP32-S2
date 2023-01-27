#include <Arduino.h>

#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <CRC32.h>



#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial

// For METRO WROVER
#define SD_MISO  21
#define SD_MOSI  16
#define SD_SCK   42
#define SD_CS    15

CRC32 crc;

SPIClass spiSD(FSPI); // Use the 'FSPI' peripheral of ESP32-S2 for SD Card

void SDinit();
void SendCRC(char filename[]);
void readUSB();
void parseUSB();


const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;

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

}


void loop(void) {
    readUSB();
    parseUSB();
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


void SendCRC(char filename[]){

    crc.reset();

    uint32_t checksum = 0;
    uint32_t bufsize = 2048;
    uint8_t buf[bufsize];

    uint32_t start = millis();
    File file = SD.open(filename);

    uint32_t fileSize = file.size();
    uint32_t dataLen = bufsize;

    // SerialDB.printf("\nTransferring with CRC %s, %d bytes\n", filename, fileSize);
    SerialUSB.printf("Incomingbytes = %u\n", fileSize);

    while (file.available()){

        if (file.available() > bufsize){
            dataLen = bufsize;
        }
        else{
            dataLen = file.available();
        }
        file.read(buf, dataLen);
        crc.update(buf,dataLen);
        SerialUSB.write(buf, dataLen);

    }
    SerialUSB.printf("\n");

    checksum = crc.finalize();
    uint32_t end = millis() - start;
    uint32_t rate = fileSize/end*1000/1024;
    SerialUSB.printf("checksum = 0x%08X, filename = %s\n", checksum, filename);
    // SerialDB.printf("- checksum = 0x%08X, filename = %s\n", checksum, filename);
    SerialDB.printf("- USB transfer in %u ms, %u KB/s\n", end, rate);

    file.close();
}

void readUSB() {
    static boolean recvInProgress = false;
    static uint8_t i = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;
 
    while (SerialUSB.available() > 0 && newData == false) {
        rc = SerialUSB.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[i] = rc;
                i++;
                if (i >= numChars) {
                    i = numChars - 1;
                }
            }
            else {
                receivedChars[i] = '\0'; // terminate the string
                recvInProgress = false;
                i = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }

}

void parseUSB() {

    if (newData == true) {
        SerialDB.print("Parsing USB Serial message: ");
        SerialDB.println(receivedChars);

        File root = SD.open("/");
        if(!root){
            SerialDB.println("Failed to open SD Card");
        }

        if(!root.isDirectory()){
            SerialDB.println("Not a directory");
            return;
        }

        char filename[numChars];
        File file = root.openNextFile();
        while(file){
            strcpy(filename, "/");
            strcat(filename, file.name());

            if (strcmp(receivedChars, "ls") == 0){
                // SerialDB.println("Listing directory");
                SerialDB.println(filename);
                SerialUSB.printf("%s\n", filename);
            }

            if (strcmp(receivedChars, filename) == 0){
                SerialDB.printf("Matched %s\n", filename);
                SendCRC(filename);
            }
            file = root.openNextFile();
        }
        newData = false;
    }
}