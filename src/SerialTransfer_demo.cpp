#include "SerialTransfer.h"
#include "Wire.h"

// #define TEST_SIZE 1024*1024 * 1 //1MB
#define TEST_SIZE 1024 * 1 //1MB
#define MAX_PACKET_SIZE 254

SerialTransfer myTransfer;

const int fileSize = TEST_SIZE;
char file[fileSize] = "Far far away, behind the word mountains, far from the countries Vokalia and Consonantia, there live the blind texts. Separated they live in Bookmarksgrove right at the coast of the Semantics, a large.";
char fileName[] = "test.txt";




void setup()
{
  memset(file, 'U', fileSize);
  Serial.begin();
  Serial0.begin(115200);

  Serial0.println("Setting up");
  myTransfer.begin(Serial);
}


void uart_send_file(){
  myTransfer.sendDatum(fileName); // Send filename
  
  uint16_t numPackets = fileSize / (MAX_PACKET_SIZE - 2); // Reserve two bytes for current file index
  
  if (fileSize % MAX_PACKET_SIZE) // Add an extra transmission if needed
    numPackets++;

  Serial0.printf("numpackets = %d ", numPackets);
  
  for (uint16_t i=0; i<numPackets; i++) // Send all data within the file across multiple packets
  {
    uint16_t fileIndex = i * MAX_PACKET_SIZE; // Determine the current file index
    uint8_t dataLen = MAX_PACKET_SIZE - 2;

    if ((fileIndex + (MAX_PACKET_SIZE - 2)) > fileSize) // Determine data length for the last packet if file length is not an exact multiple of MAX_PACKET_SIZE-2
      dataLen = fileSize - fileIndex;
    
    uint8_t sendSize = myTransfer.txObj(fileIndex); // Stuff the current file index
    sendSize = myTransfer.txObj(file[fileIndex], sendSize, dataLen); // Stuff the current file data
    
    myTransfer.sendData(sendSize, 1); // Send the current file index and data
}

void loop()
{
  myTransfer.sendDatum(fileName); // Send filename
  
  uint16_t numPackets = fileSize / (MAX_PACKET_SIZE - 2); // Reserve two bytes for current file index
  
  if (fileSize % MAX_PACKET_SIZE) // Add an extra transmission if needed
    numPackets++;

  Serial0.printf("numpackets = %d ", numPackets);
  Serial0.printf("max packet sixe = %d ",MAX_PACKET_SIZE);
  
  for (uint16_t i=0; i<numPackets; i++) // Send all data within the file across multiple packets
  {
    uint16_t fileIndex = i * MAX_PACKET_SIZE; // Determine the current file index
    uint8_t dataLen = MAX_PACKET_SIZE - 2;

    if ((fileIndex + (MAX_PACKET_SIZE - 2)) > fileSize) // Determine data length for the last packet if file length is not an exact multiple of MAX_PACKET_SIZE-2
      dataLen = fileSize - fileIndex;
    
    uint8_t sendSize = myTransfer.txObj(fileIndex); // Stuff the current file index
    sendSize = myTransfer.txObj(file[fileIndex], sendSize, dataLen); // Stuff the current file data
    
    myTransfer.sendData(sendSize, 1); // Send the current file index and data
    // Serial0.println("delay 100ms");
    // delay(100);
    // Serial0.println("sent data");
  }
  Serial0.println("delay 10s");
  delay(10000);
  

}
