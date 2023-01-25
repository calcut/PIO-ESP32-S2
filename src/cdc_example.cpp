/**
 * Simple CDC device connect with putty to use it
 * author: chegewara
 * Serial - used only for logging
 * Serial1 - can be used to control GPS or any other device, may be replaced with Serial
 */
#include "cdcusb.h"
#include "flashdisk.h"
#include "ramdisk.h"

// #define AUTO_ALLOCATE_DISK 

#define BLOCK_COUNT 2 * 200
#define BLOCK_SIZE 512

#include "FS.h"
#include "SD.h"

CDCusb USBSerial;
// FlashUSB dev;
USBramdisk dev;

char *l1 = "ffat";


class MyUSBCallbacks : public CDCCallbacks {
    void onCodingChange(cdc_line_coding_t const* p_line_coding)
    {
        int bitrate = USBSerial.getBitrate();
        Serial0.printf("new bitrate: %d\n", bitrate);
    }

    bool onConnect(bool dtr, bool rts)
    {
        Serial0.printf("connection state changed, dtr: %d, rts: %d\n", dtr, rts);
        return true;  // allow to persist reset, when Arduino IDE is trying to enter bootloader mode
    }

    void onData()
    {
        int len = USBSerial.available();
        Serial0.printf("\nnew data, len %d\n", len);
        uint8_t buf[len] = {};
        USBSerial.read(buf, len);
        Serial0.write(buf, len);
    }

    void onWantedChar(char c)
    {
        Serial0.printf("wanted char: %c\n", c);
    }
};


void setup()
{
    Serial0.begin(115200);
    USBSerial.setCallbacks(new MyUSBCallbacks());
    USBSerial.setWantedChar('x');

    if (!USBSerial.begin())
        Serial0.println("Failed to start CDC USB stack");


    #ifndef AUTO_ALLOCATE_DISK
        uint8_t* disk = (uint8_t*)ps_calloc(BLOCK_COUNT, BLOCK_SIZE);
        dev.setDiskMemory(disk, true); // pass pointer to allocated ram disk and initialize it with demo FAT12 (true)
    #endif
    // dev.setCapacity(block_count, block_size);
    dev.setCapacity(BLOCK_COUNT, BLOCK_SIZE); // if PSRAM is enableb, then ramdisk will be initialized in it


    {
       if (dev.begin())
        {
        Serial0.println("MSC lun 1 begin");
        }
        else
        log_e("LUN 1 failed");
    }
}

void loop()
{
    while (Serial.available())
    {
        int len = Serial0.available();
        char buf1[len];
        Serial0.read(buf1, len);
        int a = USBSerial.write((uint8_t*)buf1, len);
    }
}

// #endif
