// Example 3 - Receive with start- and end-markers
#include <Arduino.h>

#define SerialDB Serial0 //debug messages and control
#define SerialUSB Serial


void readUSB();
void parseUSB();

const byte numChars = 32;
char receivedChars[numChars];

boolean newData = false;

void setup() {

    SerialDB.begin(115200);
      while(!SerialDB)
    // SerialDB.begin();
    SerialDB.println("<Arduino is ready>");
    SerialUSB.println("<Arduino is ready USB>");

}

void loop() {
    readUSB();
    parseUSB();
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
        SerialDB.print("This just in ... ");
        SerialDB.println(receivedChars);

        if (strcmp(receivedChars, "ls") == 0){
            SerialDB.println("Listing directory");
        }
        else{
            for 
        }
        if (strcmp(receivedChars, "2000") == 0){
            SerialDB.println("File Requested");
        }
        newData = false;
    }
}
