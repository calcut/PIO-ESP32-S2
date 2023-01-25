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

#include <WiFi.h> 
#include <WiFiClient.h> 
#include <ESP32_FTPClient.h>
#include "octocat.h" 
#include "Secrets.h"

#include "FS.h"
#include "SPIFFS.h"

//custom libraries
// #include "ADS1284.h"
#include "IOexp.h"

#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial //debug messages and control

#define CpuFreqMHz 80

#define FORMAT_SPIFFS_IF_FAILED true
#define TEST_SIZE 1024*1024 * 1 //1MB

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

//!!DONT FORGET TO UPDATE Secrets.h with your WIFI Credentials!!
char ftp_server[] = "192.168.1.151";
char ftp_user[]   = "esp32file";
char ftp_pass[]   = "esp32file";

void SDinit();
// void ftpTest(fs::FS&, const char*, ESP32_FTPClient);

// you can pass a FTP timeout and debbug mode on the last 2 arguments
ESP32_FTPClient ftp (ftp_server,ftp_user,ftp_pass, 5000, 0); // Disable Debug to increase Tx Speed

// SPIClass spiGEO(HSPI); // Use the 'HSPI' peripheral of ESP32-S2 for Geophones
SPIClass spiSD(FSPI); // Use the 'FSPI' peripheral of ESP32-S2 for SD Card

IOexp * IO = NULL;

char workingDir[10] = "/data";

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
  memset(buf, 'U', bufsize);
  uint32_t chunks = TEST_SIZE / bufsize;
  size_t len = 0;
  File file = fs.open(path, FILE_WRITE);
  if(!file){
      Serial0.println("- failed to open file for writing");
      return;
  }

  size_t i;
  // Serial0.print("- writing" );
  uint32_t start = millis();
  for(i=0; i<chunks; i++){
      // if ((i & 0x001F) == 0x001F){
      //   Serial0.print(".");
      // }
      file.write(buf, 512);
  }
  Serial0.println("");
  uint32_t end = millis() - start;
  uint32_t rate = (chunks * bufsize)/end*1000/1024;
  // Serial0.printf("- %u bytes written in %u ms, ", chunks * bufsize, end);
  Serial0.printf("- Write Rate = %u KB/s\n", rate);
  file.close();

  file = fs.open(path);
  start = millis();
  end = start;
  i = 0;
  if(file && !file.isDirectory()){
      len = file.size();
      size_t flen = len;
      start = millis();
      // Serial0.print("- reading" );
      while (file.available()) {
          file.read(buf, bufsize);
      }
      end = millis() - start;
      rate = flen/end*1000/1024;
      // Serial0.printf("- %u bytes read in %u ms, ", flen, end);
      Serial0.printf("- Read Rate = %u KB/s\n", rate);
      file.close();
  } else {
      Serial0.println("- failed to open file for reading");
  }


  // Send over USB Serial
  file = fs.open(path);
  start = millis();
  end = start;
  i = 0;
  if(file && !file.isDirectory()){
      len = file.size();
      size_t flen = len;
      start = millis();
      // Serial0.print("- reading" );
      while (file.available()) {
          file.read(buf, bufsize);
          SerialUSB.printf("%s", buf);
          // SerialUSB.printf("block%d", i)
      }
      end = millis() - start;
      rate = flen/end*1000/1024;
      // Serial0.printf("- %u bytes read in %u ms, ", flen, end);
      Serial0.printf("- Serial Write Rate = %u KB/s\n", rate);
      file.close();
  } else {
      Serial0.println("- failed to open file for reading");
  }
}

// ReadFile Example from ESP32 SD_MMC Library within Core\Libraries
// Changed to also write the output to an FTP Stream
void ftpTest(fs::FS& fs, const char* path, ESP32_FTPClient ftpClient, uint32_t bufsize) {
  ftpClient.InitFile("Type I");
  ftpClient.NewFile(path);

  String fullPath = "/";
  fullPath.concat(path);
  // SerialDB.printf("- ftpTest Sending...\n");

  File file = fs.open(fullPath);
  if (!file) {
      SerialDB.println("Failed to open file for reading");
      return;
  }

  uint32_t start = millis();
  size_t len = file.size();
  while (file.available()) {
      // Create and fill a buffer
      unsigned char buf[bufsize];
      int readVal = file.read(buf, sizeof(buf));
      ftpClient.WriteData(buf,sizeof(buf));
  }
  uint32_t end = millis() - start;
  ftpClient.CloseFile();
  file.close();

  uint32_t rate = len/end*1000/1024;
  // Serial0.printf("- %u bytes sent by FTP in %u ms, ", len, end);
  Serial0.printf("- FTP Rate = %u KB/s\n", rate);
} 

void ftpRAMTest(const char* path, ESP32_FTPClient ftpClient,  uint32_t bufsize) {
    ftpClient.InitFile("Type I");
    ftpClient.NewFile(path);

    uint8_t buf[bufsize];
    uint32_t chunks = TEST_SIZE / bufsize;
    uint32_t start = millis();

    size_t i;
    for(i=0; i<chunks; i++){
        // if ((i & 0x001F) == 0x001F){
        //   Serial0.print(".");
        // }
        ftpClient.WriteData(buf,sizeof(buf));
    }

    uint32_t end = millis() - start;
    ftpClient.CloseFile();

    uint32_t rate = TEST_SIZE/end*1000/1024;
    // Serial0.printf("\n- %u RAM bytes sent by FTP in %u ms, ", TEST_SIZE, end);
    Serial0.printf("\n- FTP Rate = %u KB/s\n", rate);
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

void setup()
{
  SerialDB.begin( 115200 );
  SerialUSB.begin();

  IO = new IOexp(&Serial0, I2C_SDA, I2C_SCL, IO_INTERRUPT);
  SerialDB.println("IO Expander All Disabled");
  IO->disable_all();  //Disable GPS and ADC

  setCpuFrequencyMhz(240);
  SerialDB.println("delay 5s to measure power at 240MHz CPU");
  delay(5000);


  // setCpuFrequencyMhz(CpuFreqMHz);
  // SerialDB.println("delay 5s to measure power at 80MHz CPU, before wifi");
  // delay(5000);

  // SerialDB.println("IO Expander ALl enabled");
  // IO->enable_all();  //Enable ADCs but not GPS
  // delay(5000);

  SerialDB.println("IO Expander ADCs enabled");
  IO->enable_ADCs();  //Enable ADCs but not GPS
  delay(5000);


  // SerialDB.println("Connecting Wifi...");

  // WiFi.begin( WIFI_SSID, WIFI_PASS );
  
  // while (WiFi.status() != WL_CONNECTED) {
  //     delay(500);
  //     SerialDB.print(".");
  // }
  // SerialDB.println("");
  // SerialDB.print("IP address: ");
  // SerialDB.println(WiFi.localIP());

  // SerialDB.println("delay 5s to measure power at 80MHz CPU, with wifi");
  // delay(5000);

  // Init SPIFFS
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
  Serial0.println("SPIFFS Mount Failed");
  return;
  }

  // Init SD Card
  SerialDB.println("Initializing SD card...");


  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS); 
  if (SD.begin(SD_CS, spiSD)) {
      SerialDB.println("SD Card initialised");
  } else SerialDB.println("Failed to init SD");

  SDinit();   // Prep SD card


  // ftp.OpenConnection();


  // ftp.MakeDir("my_new_dir");
  // ftp.ChangeWorkDir("my_new_dir");

  uint32_t bufsize = 512;
  Serial0.printf("\nTest size %u", TEST_SIZE);

for (bufsize = 512; bufsize <= 4096; bufsize=bufsize*2){
    Serial0.printf("\nRAM test at bufsize %u bytes", bufsize);
    // ftpRAMTest("RAMtest.txt", ftp, bufsize);
    
    Serial0.printf("\nSD test at bufsize %u bytes", bufsize);
    testFileIO(SD, "/test.txt", bufsize);                                                                    // ::BYTES::
    // ftpTest(SD, "test.txt", ftp, bufsize);
    
    Serial0.printf("\nSPIFFS test at bufsize %u bytes", bufsize);
    testFileIO(SPIFFS, "/test.txt", bufsize);
    // ftpTest(SPIFFS, "test.txt", ftp, bufsize);
}

  // ftp.CloseConnection();

  // WiFi.disconnect(true);
  // SerialDB.println("delay 5s to measure power with wifi disconnected");
  // delay(5000);



}

void loop()
{

}


