#include <Adafruit_NeoPixel.h>
#include <Arduino_CAN.h>
#include <EEPROM.h>
#include <RTC.h>
#include <SD.h>

// Neopixel variables
#define PIN 3
#define NUMPIXELS 5
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Global variables
const int chipSelect = 10;
const int address = 0;
unsigned long delayTime = 0;
unsigned long millisTime;
bool filter = false;
bool whitelist = true;
bool datalogger = false;
int fileNum = 0;
RTCTime currentTime;
byte fileCount;

// White/blacklist filters
int whitelistCount = 0;
int blacklistCount = 0;
char whitelistMembers[] = {};
char blacklistMembers[] = {};
String whitelistWords[] = {};
String blacklistWords[] = {};

void setup() {
  // Start serial, SD, and CAN communication
  Serial.begin(115200);
  SD.begin(chipSelect);
  CAN.begin(CanBitRate::BR_1000k);

  // Read and update EEPROM
  updateFilters();
  fileCount = EEPROM.read(address);
  fileCount++;
  EEPROM.write(address, fileCount);

  // Start the 'real time' clock
  RTC.begin();
  RTCTime startTime(22, Month::JANUARY, 2026, 10, 15, 00, DayOfWeek::THURSDAY, SaveLight::SAVING_TIME_ACTIVE);
  RTC.setTime(startTime);

  // Turn the NeoPixels on
  pixels.begin();
  pixels.setBrightness(50);
  // Set PWR LED to green
  pixels.setPixelColor(0, pixels.Color(0, 150, 0));
}

void loop() {
  // Get the current time
  RTC.getTime(currentTime);
  millisTime = millis();

  // Log new CAN message
  if(datalogger){
    logCAN();
  }

  // Update the Neopixels
  if(millisTime - delayTime > 10000)
  {
    pixels.show();
    delayTime = millis();
  }
}


// Update the CAN speed
void CANSpeed(char var){
  CAN.end();
  switch(var){
    case 'a':
      if (!CAN.begin(CanBitRate::BR_125k)){
        Serial.println("CAN.begin(...) failed.");
        for (;;) {}
      }
      break;
    case 'b':
      if (!CAN.begin(CanBitRate::BR_250k)){
        Serial.println("CAN.begin(...) failed.");
        for (;;) {}
      }
      break;
    case 'c':
      if (!CAN.begin(CanBitRate::BR_500k)){
        Serial.println("CAN.begin(...) failed.");
        for (;;) {}
      }
      break;
    case 'd':
      if (!CAN.begin(CanBitRate::BR_1000k)){
        Serial.println("CAN.begin(...) failed.");
        for (;;) {}
      }
      break;
  }
}

// Switch the filter on/off
void filterControl(char var){
  switch(var){
    case 'e':
      filter = true;
      break;
    case 'f':
      filter = false;
      break;
  }
}

// Switch the filter to white/blacklist
void filterSettings(char var){
  switch(var){
    case 'g':
      whitelist = false;
      break;
    case 'h':
      whitelist = true;
      break;
  }
}

// Turn the datalogger on/off
void dataloggerControl(char var){
  switch(var){
    case 'i':
      datalogger = true;
      break;
    case 'j':
      datalogger = false;
      break;
  }
}

// Update the whitelist from VS
void updateWhitelist() {
  int index = 0;
  while(Serial.available()){
    char incomingByte = Serial.read();
    whitelistMembers[index] = incomingByte;
    index++;
  }
  sortWhitelist();
}

// Update the blacklist from vs
void updateBlacklist() {
  int index = 0;
  while(Serial.available()){
    char incomingByte = Serial.read();
    blacklistMembers[index] = incomingByte;
    index++;
  }
  sortBlacklist();
}

// Send the whitelist to VS
void sendWhitelist() {
  for(int i = 0; i < whitelistCount; i++){
    Serial.print(whitelistMembers[i]);
  }
}

// Send the blacklist to VS
void sendBlacklist() {
  for(int i = 0; i < blacklistCount; i++){
    Serial.print(blacklistMembers[i]);
  }
}

// Burn the whitelist to EEPROM
void burnWhitelist() {
  int index = 3;
  for(int i = 0; i < whitelistCount; i++){
    EEPROM.write(index, whitelistMembers[i]);
    index = index + 2;
  }
  updateFilters();
}

// Burn the blacklist to EEPROM
void burnBlacklist() {
  int index = 4;
  for(int i = 0; i < blacklistCount; i++){
    EEPROM.write(index, blacklistMembers[i]);
    index = index + 2;
  }
  updateFilters();
}

// Split whitelist filters by '/n' characters
void sortWhitelist() {
  int index = 0;
  String newEntry = "";
  for(int i = 0; i < whitelistCount; i++){
    if(whitelistMembers[i] == '\n'){
      whitelistWords[index] = newEntry;
      newEntry = "";
    } else{
      newEntry += whitelistMembers[i];
    }
    index++;
  }
  EEPROM.write(1, index);
}

// Split blacklist filters by '/n' characters
void sortBlacklist() {
  int index = 0;
  String newEntry = "";
  for(int i = 0; i < blacklistCount; i++){
    if(blacklistMembers[i] == '\n'){
      blacklistWords[index] = newEntry;
      newEntry = "";
    } else{
      newEntry += blacklistMembers[i];
    }
    index++;
  }
  EEPROM.write(2, index);
}

// update whitelist & blacklist from EEPROM
void updateFilters() {
  whitelistCount = EEPROM.read(1);
  blacklistCount = EEPROM.read(2);
  int whiteIndex = 3;
  int blackIndex = 4;
  for(int i = 0; i < whitelistCount; i++){
    whitelistMembers[i] = EEPROM.read(whiteIndex);
    whiteIndex = whiteIndex + 2;
  }
  for(int i = 0; i < blacklistCount; i++){
    blacklistMembers[i] = EEPROM.read(blackIndex);
    blackIndex = blackIndex + 2;
  }
  sortWhitelist();
  sortBlacklist();
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
    bool whitelistPass = false;
    bool blacklistPass = true;
    if (filter) {
      if (whitelist) {
        for(int i = 0; i < whitelistCount; i++){
          if(whitelistWords[i].equals(String(msg.id))){
            whitelistPass = true;
          }
        }
      }
      if (!whitelist) {
        for(int i = 0; i < blacklistCount; i++){
          if(blacklistWords[i].equals(String(msg.id))){
            blacklistPass = false;
          }
        }
      }
    } else {
      blacklistPass = true;
      whitelistPass = true;
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

      // Sets SD LED to green
      pixels.setPixelColor(4, pixels.Color(0, 150, 0));
    } else {
      // Sets SD LED to red
      pixels.setPixelColor(4, pixels.Color(150, 0, 0));
      if (!SD.begin(chipSelect))
      {
        Serial.println("SD.begin(...) failed.");
        for (;;) {}
      }
    }
    // Sets RX LED to green
    pixels.setPixelColor(2, pixels.Color(0, 150, 0));
  } else
  {
    // Sets RX LED to red
    pixels.setPixelColor(2, pixels.Color(150, 0, 0));
  }
}