#include <Arduino.h>

#include <SPI.h>
#include <FS.h>
#include <SD.h>

// 3rd party libs
#include <Adafruit_GPS.h> 
#include <CRC32.h>  // https://github.com/bakercp/CRC32
#include <TimeLib.h> // https://github.com/PaulStoffregen/Time
#include <RingBuf.h> // https://github.com/Locoduino/RingBuffer

//custom libraries
#include "ADS1284.h"
#include "IOexp.h"

// Uncomment to only use ADC0
// #define ADC_SINGLE
// NB This won't work properly on unmodified Geoduino Rev 1.00 boards
// It needs the DRDY resistors removed from ADC1 and ADC2 (R53 and R27 in KICAD schematic)

#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial
#define SerialGPS Serial1 //GPS module

// #define BUFFER_SIZE 2500 //10s worth at 250sps - how big can/should this be??
#define BUFFER_SIZE 7500 //30s worth at 250sps - how big can/should this be??
#define CpuFreqMHz 240

//I2C Pins
#define I2C_SDA  33
#define I2C_SCL  34

//GPS UART Pins 
// Make sure switch on shield is set to soft serial
#define GPS_TX  12 //Data to GPS module
#define GPS_RX  13 //Data from GPS module
#define GPS_PPS 14 //Pulse Per Second signal

// For METRO WROVER
#define SD_MISO  21
#define SD_MOSI  16
#define SD_SCK   42
#define SD_CS    15

//Geoduino SPI pins
#define GEO_MISO   37
#define GEO_MOSI   35
#define GEO_SCLK   36

#define GEO_CS0     9
#define GEO_CS1     8
#define GEO_CS2     7

//IO Expander Interrupt
#define IO_INTERRUPT 17

//Geoduino control pins 
#define DRDY  10
#define SYNCpin  11

struct Tstamp
{
  time_t unixtime = 0;
  uint32_t micro = 0;
};

void SDinit();
void SendCRC(char filename[]);
void readUSB();
void parseUSB();
void GEOinitSingle();
void GEOinit();
void ISR_DRDY();
void GPStimesync();
void GPSstatus();
void GPSinit();
void GPSservice();
void ISR_PPS();
void processData();
uint32_t GPSmicros();
void printSystemtime();

//Declare instances of classes

CRC32 crc;
SPIClass spiSD(FSPI); // Use the 'FSPI' peripheral of ESP32-S2 for SD Card
SPIClass spiGEO(HSPI); // Use the 'HSPI' peripheral of ESP32-S2 for Geophones
IOexp * IO = NULL;
Adafruit_GPS * GPS; 

ADS1284 * ADC0;
ADS1284 * ADC1;
ADS1284 * ADC2;
RingBuf<struct Tstamp, BUFFER_SIZE> timestamps;

uint32_t timer = millis();
uint32_t timeout = millis();
int buffer_low_size = 10;

ADCconf conf;

//temporary variables (may not be needed eventually)
volatile int drdy_count=0; //volatile because it will be motified by ISR
volatile int pps_count=0; //volatile because it will be motified by ISR

// for USB serial interface
boolean newData = false;
const byte numChars = 32; //in SDcard filenames
char receivedChars[numChars]; 

int log_stattime = 5; //seconds
int GPS_ontime = 3600; //seconds
int GPS_offtime = 60; //seconds
int GPS_synctime = 5; //seconds
uint32_t timerGPS = millis();
uint32_t timerGPSsync = millis();
uint32_t micros_offset = micros();
volatile bool pps_flag = false;
bool timesync_flag = false;
bool GPS_enabled = false;

int32_t sample0_mem = 0;
int32_t sample1_mem = 0;
int32_t sample2_mem = 0;
uint32_t micro_mem =0;
uint32_t unixtime_mem =0;
uint32_t micros_check = 0;

boolean debugfileA = true;


void setup(void) {
    SerialDB.begin(115200);
    while(!SerialDB)
  
    SerialUSB.begin(115200);

    setCpuFrequencyMhz(CpuFreqMHz);
    int CPUfreq = getCpuFrequencyMhz();
    SerialDB.print("CPUfreqMHz: ");
    SerialDB.println(CPUfreq);

    SerialDB.println(" Connected!");
    SerialDB.println("Initializing SD card...");

    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS); 
    if (SD.begin(SD_CS, spiSD, 20000000)) {
        SerialDB.println("SD Card initialised");
    } else SerialDB.println("Failed to init SD");

    SDinit();

    IO = new IOexp(&SerialDB, I2C_SDA, I2C_SCL, IO_INTERRUPT);
    IO->enable_all();
    SerialDB.println("IO Expander All Enabled");

    GEOinit();  // Setup 3x ADCs on Geoduino
    GPSinit();  // Setup GPS

}

void loop(void) {

    GPSservice();
    readUSB();
    parseUSB();
    processData();    

}

void processData(){
    if(timesync_flag){ // Only process data if system time is correct
        uint32_t samples_to_process = timestamps.maxSize()/2;

        if (timestamps.size() > samples_to_process) {
        // if (!timestamps.isEmpty()) {

            struct Tstamp stamp;
            int32_t sample0 = 0;
            int32_t sample1 = 0;
            int32_t sample2 = 0;
            char row[99]; 
            char filename[numChars];
            sprintf(filename, "/data_%04u-%02u-%02u-%02u%02u.txt", year(),month(),day(),hour(),minute() );
            if (debugfileA) {
                sprintf(filename, "/dataA.txt");
            }

            SerialDB.print("**** Writing to SD card: ");
            SerialDB.println(filename);
            // File file = SD.open(filename, FILE_WRITE);
            File file = SD.open(filename, FILE_APPEND);
            if(!file){
                SerialDB.println("Failed to open file for appending");
                return;
            }

            while (!timestamps.isEmpty()) {
                timestamps.lockedPop(stamp);
                ADC0->data.lockedPop(sample0);
                ADC1->data.lockedPop(sample1);
                ADC2->data.lockedPop(sample2);

                if (micros_check == 0 ){
                    micros_check = stamp.micro;

                }
                int32_t micros_diff = stamp.micro - micros_check;


                int32_t delta_time = (stamp.micro - micro_mem) + (stamp.unixtime - unixtime_mem)*1000000;
                
                if (delta_time < 0){
                    delta_time += 1000000;
                    if ((stamp.unixtime - unixtime_mem) == 0){
                        stamp.unixtime +=1;
                    }
                }
                
                int32_t delta_sample0 = sample0 - sample0_mem;
                micro_mem = stamp.micro;
                unixtime_mem = stamp.unixtime;
                sample0_mem = sample0;

                // sprintf(row, "%08X %08X %08X %010d.%06d",
                // sample0 ,sample1, sample2, stamp.unixtime, stamp.micro);

                sprintf(row, "%08X %08X %08X %010d.%06d %d %d %d %d",
                sample0 ,sample1, sample2, stamp.unixtime, stamp.micro, delta_time, micros_diff, micros_check);
                                        
                micros_check = micros_check + 4000;
                micros_check = micros_check % 1000000;

                file.println(row);
            }

            file.close();



        // Print as DEC so the Arduino Serial Plotter can parse it
        // sprintf(row, "%010d %010d %010d",
        //     sample0, sample1, sample2);
        // SerialDB.println(row);
        
        // Alternatively, print as HEX
        // sprintf(row, "%08X %08X %08X",
        //     sample0 ,sample1, sample2);

        // SerialDB.println(row);

        }
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

    SerialDB.printf("\nTransferring with CRC %s, %d bytes\n", filename, fileSize);
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

        char cmd[numChars];
        char arg[numChars];
        delay(1); //Not sure why this is required! maybe a bug. sscanf doesn't work without it.
        sscanf(receivedChars, "%s %s", cmd, arg);

        if (strcmp(cmd, "cp") == 0){
            SerialDB.printf("Transferring %s\n", arg);
            if (SD.exists(arg)){
                SendCRC(arg);
            }
            else {
                SerialDB.println("File not found!");
            }

        }

        if (strcmp(cmd, "rm") == 0){
            SerialDB.printf("Deleting %s\n", arg);
            if (SD.exists(arg)){
                SD.remove(arg);
            }
            else {
                SerialDB.println("File not found!");
            }

        }

        if (strcmp(cmd, "ls") == 0){
            SerialDB.println("Listing SD card");
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
                SerialDB.println(filename);
                SerialUSB.printf("%s\n", filename);
                file = root.openNextFile();
                }
        }
        newData = false;
    }
}

void ISR_DRDY() {
    struct Tstamp stamp;
    if(timesync_flag){

        stamp.micro = GPSmicros();
        stamp.unixtime = now();

        drdy_count += 1;
        ADC0->readData();
        ADC1->readData();
        ADC2->readData();

        timestamps.push(stamp);
    }
}

void GEOinit(){
    delay(100); // to give time for ADC to power up
    spiGEO.begin(GEO_SCLK, GEO_MISO, GEO_MOSI, GEO_CS0); 
    ADC0 = new ADS1284(&SerialDB, &spiGEO, GEO_CS0, SYNCpin, 0, conf);
    ADC1 = new ADS1284(&SerialDB, &spiGEO, GEO_CS1, SYNCpin, 1, conf);
    ADC2 = new ADS1284(&SerialDB, &spiGEO, GEO_CS2, SYNCpin, 2, conf);
    pinMode(DRDY, INPUT);
    attachInterrupt(DRDY,ISR_DRDY,FALLING); 

    digitalWrite(SYNCpin, LOW);
    delay(10);
    digitalWrite(SYNCpin, HIGH);
}

void GEOinitSingle(){
    delay(100); // to give time for ADCs to power up
    pinMode(GEO_CS1, OUTPUT);
    pinMode(GEO_CS2, OUTPUT);
    digitalWrite(GEO_CS1, HIGH);
    digitalWrite(GEO_CS2, HIGH);

    spiGEO.begin(GEO_SCLK, GEO_MISO, GEO_MOSI, GEO_CS0); 
    ADC0 = new ADS1284(&SerialDB, &spiGEO, GEO_CS0, SYNCpin, 0, conf);
    pinMode(DRDY, INPUT);
    attachInterrupt(DRDY,ISR_DRDY,FALLING); 

    digitalWrite(SYNCpin, LOW);
    delay(10);
    digitalWrite(SYNCpin, HIGH);
}

void GPSstatus(){
    // if (debug->print_gps) {
    if (true) {
        SerialDB.println("");
        SerialDB.println("---GPS STATUS---");
        SerialDB.print("Fix: "); SerialDB.print((int)GPS->fix);
        SerialDB.print(" quality: "); SerialDB.println((int)GPS->fixquality);
        SerialDB.print("Time [s] since last fix: ");
        SerialDB.println(GPS->secondsSinceFix(), 3);
        SerialDB.print("    since last GPS time: ");
        SerialDB.println(GPS->secondsSinceTime(), 3);
        if (GPS->fix) {
            SerialDB.print("Location: ");
            SerialDB.print(GPS->latitude, 4); SerialDB.print(GPS->lat);
            SerialDB.print(", ");
            SerialDB.print(GPS->longitude, 4); SerialDB.println(GPS->lon);
            //   SerialDB.print("Speed (knots): "); SerialDB.println(GPS->speed);
            //   SerialDB.print("Angle: "); SerialDB.println(GPS->angle);
            SerialDB.print("Altitude: "); SerialDB.println(GPS->altitude);
            SerialDB.print("Satellites: "); SerialDB.println((int)GPS->satellites);
        }
        SerialDB.println("");
    }

}

void GPSinit(){
    SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    GPS = new Adafruit_GPS(&SerialGPS);
    GPS->wakeup();  // Needed to make sure the GPS library knows it is awake
    GPS->sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    GPS->sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
    // GPS->sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
    // GPS->sendCommand(PMTK_SET_NMEA_UPDATE_100_MILLIHERTZ); 
    GPS->sendCommand(PGCMD_ANTENNA);
    delay(1000);
    // Ask for firmware version
    SerialGPS.println(PMTK_Q_RELEASE);

    pinMode(GPS_PPS, INPUT);
    attachInterrupt(GPS_PPS,ISR_PPS,RISING);
    GPS_enabled = true;
    GPStimesync();
    // GPS->standby();
}

void GPSservice() {
    if (GPS_enabled) {
        if (pps_flag){
            pps_flag = false;
            if (millis() - timerGPSsync > (GPS_synctime*1000)){
                timerGPSsync = millis();
                setTime(GPS->hour, GPS->minute, GPS->seconds, GPS->day, GPS->month, (GPS->year + 2000));
                SerialDB.println("Synchronising System time to GPS, via GPSservice");
            }
        }
        char c = GPS->read();

        if (GPS->newNMEAreceived()) {
            GPS->parse(GPS->lastNMEA());
        }

        // if(GPS->fix) {

        //     else {
        //         //If sysclk is not yet synchronised, we can skip the timer.
        //         //also check that year is positive, to make sure we've received
        //         //a NMEA message with the time
        //         if(!timesync_flag && GPS->year > 0) {
        //             GPStimesync();
        //         }
        //     }
        // }
            
        if (millis() - timerGPS > (GPS_ontime*1000)) {
            timerGPS = millis();
            IO->enable_ADCs();
            SerialDB.println("--------------------------------Disabling GPS---------------------------------");
            GPS_enabled = false;
        }
    }
    else {
        if (millis() - timerGPS > (GPS_offtime*1000)) {
            timerGPS = millis();
            SerialDB.println("--------------------------------Enabling GPS----------------------------------");
            IO->enable_all();
            GPS_enabled = true;
            
        }
    }


}

void GPStimesync() {

    if (GPS->year == 0){
        uint32_t timer_tsync = millis();
        SerialDB.println("Waiting up to 60s for GPS Fix before time sync");

        while(!GPS->fix || (GPS->year == 0)){
            GPSservice();
            if (millis() - timer_tsync > 60000) {
                SerialDB.println("Warning - No GPS fix, time not set");
                return;
            }
        }

        timer_tsync = millis(); 
        pps_flag = false;  
        while (!pps_flag) { // wait to sync time at exactly a PPS edge.
            delayMicroseconds(5);
            if (millis() - timer_tsync > 5000) {
                SerialDB.println("Warning - No PPS pulse detected for time sync");
                break;  // to prevent getting stuck here if no PPS signal arrives
            }
        }
    }
    SerialDB.println("Synchronising System time to GPS");
    setTime(GPS->hour, GPS->minute, GPS->seconds, GPS->day, GPS->month, (GPS->year + 2000));
    timesync_flag = true;

}

void ISR_PPS() {
    micros_offset = micros();
    // setTime(GPS->hour, GPS->minute, GPS->seconds, GPS->day, GPS->month, (GPS->year + 2000));
    pps_flag = true;
    pps_count += 1;
}
uint32_t GPSmicros() {
    uint32_t GPS_us = micros() - micros_offset;
    //Possible error here every ~71 minutes due to rollover!
    return GPS_us;
}

void printSystemtime(){
    // if (debug->print_DB) {
    if (true) {
        SerialDB.printf("System time: %04d-%02d-%02d %02d:%02d:%02d",
        year(), month(), day(), hour(), minute(), second());
    }
}