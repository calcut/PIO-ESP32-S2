#define SerialDB Serial0 //debug messages and control

#include <Arduino.h>
       
// char arg1[numChars];
// char arg2[numChars];



void setup(){
    const int numChars = 9;
    char receivedChars[numChars] = "ab cd ef";
    char cmd[numChars];
    char arg1[numChars];
    char arg2[numChars];

    SerialDB.begin(115200);
        while(!SerialDB)

    // strcpy(receivedChars, "abc def");
    // delay(1);
    // SerialDB.printf("rec= %s\n", receivedChars);
    // SerialDB.printf("cmd= %s\n", cmd);

    sscanf(receivedChars, "%s %s %s", cmd, arg1, arg2);
    SerialDB.printf("rec= %s\n", receivedChars);
    SerialDB.printf("cmd= %s\n", cmd);
    SerialDB.printf("arg1= %s\n", arg1);
    SerialDB.printf("arg2= %s\n", arg2);
    // SerialDB.printf("arg2= %s\n", arg2);
}

void loop()
{
    delay(10000);
}
