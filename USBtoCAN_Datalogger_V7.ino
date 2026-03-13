#include <Adafruit_NeoPixel.h>
#include <Arduino_CAN.h>
#include <EEPROM.h>
#include <RTC.h>
#include <SD.h>

// Neopixel variables
#define PIN 3
#define NUMPIXELS 5
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Serial input variables
boolean newData = false;
const byte numChars = 32;
char receivedChars[numChars];

// White/blacklist variables
bool filter = 0;
bool whitelistOn = 0;
const int numRows = 10;
const int numCols = 29;
char whitelist[numRows][numCols] = {};
char blacklist[numRows][numCols] = {};

// Timer variables
RTCTime currentTime;
unsigned long millisTime;
unsigned long delayTime = 0;

// SD card variables
byte fileCount;
int fileNum = 0;
const int chipSelect = 10;

// EEPROM variables
int fileNumIndex = 0;
int whitelistIndex = 1;
int blacklistIndex = 30;

void setup() {
  // Start serial, SD, and CAN communication
  Serial.begin(115200);
  SD.begin(chipSelect);
  CAN.begin(CanBitRate::BR_1000k);
  
  // Read and update EEPROM
  fileCount = EEPROM.read(fileNumIndex) + 1;
  EEPROM.write(fileNumIndex, fileCount);
  updateList(whitelist, whitelistIndex);
  updateList(blacklist, blacklistIndex);

  // Start the 'real time' clock
  RTC.begin();
  RTCTime startTime(01, Month::JANUARY, 2026, 10, 15, 00, DayOfWeek::THURSDAY, SaveLight::SAVING_TIME_ACTIVE);
  RTC.setTime(startTime);

  // Turn the NeoPixels on
  pixels.begin();
  pixels.setBrightness(25);
}

void loop() {

  logCAN();
  if(Serial.available()){
    char input = Serial.read();
    switch(input){
      case 'a':
        updateCANSpeed(125000);
        break;
      case 'b':
        updateCANSpeed(250000);
        break;
      case 'c':
        updateCANSpeed(500000);
        break;
      case 'd':
        updateCANSpeed(1000000);
        break;
      case 'e':
        filter = true;
        whitelistOn = false;
        break;
      case 'f':
        whitelistOn = false;
        break;
      case 'g':
        whitelistOn = true;
        break;
      case 'u':
        readList(whitelist);
        break;
      case 'v':
        writeList(whitelist);
        break;
      case 'w':
        burnList(whitelist, whitelistIndex);
        break;
      case 'x':
        readList(blacklist);
        break;
      case 'y':
        writeList(blacklist);
        break;
      case 'z':
        burnList(blacklist, blacklistIndex);
        break;
      case '1':
        updateList(whitelist, whitelistIndex);
        break;
      case '2':
        updateList(blacklist, blacklistIndex);
        break;
      default:
        break;
    }
  }

  // Get the current time
  RTC.getTime(currentTime);
  millisTime = millis();

  // Update the Neopixels
  if(millisTime - delayTime > 10000){
    pixels.show();
    delayTime = millis();
  }
}

// Update the CANBus speed
void updateCANSpeed(int speed){
  if (!CAN.begin(speed)){
    Serial.println("CAN.begin(...) failed.");
    for (;;) {}
  }
  Serial.print("CAN speed was set to ");
  Serial.println(speed);
}


// Read the white/blacklist from the serial monitor
void readList(char list[numRows][numCols]){
  int serialIndex = 0;
  recvWithStartEndMarkers();
  for(int i = 0; i < numRows; i++){
    for(int j = 0; j < numCols; j++){
      list[i][j] = receivedChars[serialIndex];
      serialIndex++;
    }
  }
}

// Write the white/blacklist to the serial monitor
void writeList(char list[numRows][numCols]){
  Serial.print("<");
  for(int i = 0; i < numRows; i++){
    for(int j = 0; j < numCols; j++){
      Serial.print(whitelist[i][j]);
    }
    Serial.print("*");
  }
  Serial.print(">");
  Serial.println("");
}

// Burn the white/blacklist to the EEPROM
void burnList(char list[numRows][numCols], int index){
  for(int i = 0; i < numRows; i++){
    for(int j = 0; j < numCols; j++){
      EEPROM.write(index, list[i][j]);
      index++;
    }
    index = index + numCols;
  } 
}

// Update the white/blacklist from the EEPROM
void updateList(char list[numRows][numCols], int index){
  for(int i = 0; i < numRows; i++){
    for(int j = 0; j < numCols; j++){
      list[i][j] = EEPROM.read(index);
      index++;
    }
    index = index + numCols;
  }
}

// Log CAN message to datalogger
void logCAN() {
  // Creates/updates the current file
  String tempFileName = String(fileCount) + "_" + String(fileNum) + ".txt";
  File dataFile = SD.open(tempFileName, FILE_WRITE);
  if(dataFile.size() == 0){
    dataFile.println("Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
  }

  // Adds the CAN message to the datalog
  if (CAN.available())
  {
    CanMsg const msg = CAN.read();
    Serial.print("t");
    Serial.print(msg.id, HEX);
    Serial.print(msg.data_length, HEX);

    for(int i = 0; i < byte(msg.data_length); i++){
      if(byte(msg.data[i]) < 0x10){
        Serial.print(0);
      }
      Serial.print(msg.data[i], HEX);
    }
    Serial.print("\r");

    // Checks whether the msg passes the filters
    bool whitelistPass = true;
    bool blacklistPass = true;

    if(filter){
      for(int i = 0; i < numRows; i++){
        for(int j = 0; j < numCols; j++){
          if(whitelist[i][j] != (msg.id, HEX)[j]){
            whitelistPass = false;
          }
          if(whitelist[i][j] == msg.id[j]){
            blacklistPass = false;
          }
        }
      }
      if(whitelistOn){
        blacklistPass = true;
      } else {
        whitelistPass = true;
      }
    }

    if (dataFile && blacklistPass && whitelistPass) {
      // Print time (HH/MM/SS)
      dataFile.print(currentTime.getUnixTime());
      dataFile.print(millisTime);
      dataFile.print(",");
      // Print ID
      dataFile.print(msg.id, HEX);
      dataFile.print(",");
      // Print Extended
      dataFile.print(msg.isExtendedId());
      dataFile.print(",");
      // Print Dir
      dataFile.print("Rx");
      dataFile.print(",");
      // Print Bus
      dataFile.print("0");
      dataFile.print(",");
      // Print Length
      dataFile.print(msg.data_length, HEX);
      dataFile.print(",");
      // Print Data
      for(int i = 0; i < byte(msg.data_length); i++){
        if(byte(msg.data[i]) < 0x10){
          dataFile.print(0);
        }
        dataFile.print(msg.data[i], HEX);
        dataFile.print(",");
      }
      if(dataFile.size() > 100000000)
      {
        fileNum++;
      }
      dataFile.print("\r");
      dataFile.print("\n");
      dataFile.close();
    } else {
      if (!SD.begin(chipSelect))
      {
        Serial.println("SD.begin(...) failed.");
        for (;;) {}
      }
    }
  }
}

// Receive data from the serial monitor bracketed by '<>'
// Credit: https://forum.arduino.cc/t/serial-input-basics-updated/382007
void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;
 
  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();
    if (recvInProgress == true) {
      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }
    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
  newData = false;
}