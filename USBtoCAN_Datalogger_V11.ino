#include <Arduino_CAN.h>
#include <EEPROM.h>
#include <SdFat.h>
#include <RTC.h>
#include <SPI.h>

// Serial input variables
boolean newData = false;
const byte NUM_CHARS = 32;
char receivedChars[NUM_CHARS];

// White/blacklist variables
byte listState = 0;
byte filterState = 0;
const int NUM_ROWS = 5;
const int NUM_COLS = 3;
char whitelist[NUM_ROWS][NUM_COLS] = {};
char blacklist[NUM_ROWS][NUM_COLS] = {};

// Timer variables
RTCTime currentTime;
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;

// SD card variables
SdFat sd;
int fileCount;
FsFile dataFile;
int fileNum = 0;
const int CHIP_SELECT = 10;

// EEPROM variables
int filterIndex = 0;
int fileNumIndex = 1;
int CANSpeedIndex = 2;
int listStateIndex = 3;
int whitelistIndex = 4;
int blacklistIndex = whitelistIndex + NUM_ROWS*NUM_COLS - 1;

// CANSpeed variables
byte CANSpeed;
int CANSpeedArray[4] = {125000, 250000, 500000, 1000000};

// SD buffer variables
int bufferIndex = 0;
const int MSG_LENGTH = 100;
const int BUFFER_SIZE = 100;
char msgBuffer[BUFFER_SIZE][MSG_LENGTH];

void setup() {
  // Read and update EEPROM
  fileCount = EEPROM.read(fileNumIndex) + 1;
  EEPROM.update(fileNumIndex, fileCount);
  CANSpeed = EEPROM.read(CANSpeedIndex);
  filterState = EEPROM.read(filterIndex);
  listState = EEPROM.read(listStateIndex);

  // Start the 'real time' clock + Serial, CAN, & SD communication
  RTC.begin();
  Serial.begin(115200);
  CAN.begin(1000000);
  sd.begin(CHIP_SELECT, SD_SCK_MHZ(50));

  // Update white/blacklist from EEPROM
  updateList(whitelist, whitelistIndex);
  updateList(blacklist, blacklistIndex);
}

void loop() {
  // Get the current time & log available CAN messages
  RTC.getTime(currentTime);
  currentMillis = millis();
  logCAN();

  // Check for serial communication from VS
  if(Serial.available()){
    char input = Serial.read();
    switch(input){
      case 'l':
        Serial.print(EEPROM.read(CANSpeedIndex));
        Serial.print(EEPROM.read(filterIndex));
        Serial.print(EEPROM.read(listStateIndex));
        Serial.print(">");
        break;
      case 'm':
        CANSpeed = 0;
        updateCANSpeed();
        break;
      case 'n':
        CANSpeed = 1;
        updateCANSpeed();
        break;
      case 'o':
        CANSpeed = 2;
        updateCANSpeed();
        break;
      case 'p':
        CANSpeed = 3;
        updateCANSpeed();
        break;
      case 'q':
        filterState = 1;
        EEPROM.update(filterIndex, filterState);
        break;
      case 'r':
        filterState = 0;
        EEPROM.update(filterIndex, filterState);
        break;
      case 's':
        listState = 0;
        EEPROM.update(listStateIndex, listState);
        break;
      case 't':
        listState = 1;
        EEPROM.update(listStateIndex, listState);
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
}

// Read the white/blacklist from the serial monitor
void readList(char list[NUM_ROWS][NUM_COLS]){
  int serialIndex = 0;
  recvWithStartEndMarkers();
  for(int i = 0; i < NUM_ROWS; i++){
    for(int j = 0; j < NUM_COLS; j++){
      list[i][j] = receivedChars[serialIndex];
      serialIndex++;
    }
  }
}

// Write the white/blacklist to the serial monitor
void writeList(char list[NUM_ROWS][NUM_COLS]){
  for(int i = 0; i < NUM_ROWS; i++){
    for(int j = 0; j < NUM_COLS; j++){
      Serial.print(list[i][j]);
    }
    Serial.print("*");
  }
  Serial.print(">");
}

// Burn the white/blacklist to the EEPROM
void burnList(char list[NUM_ROWS][NUM_COLS], int index){
  int originalIndex = index;
  for(int i = 0; i < NUM_ROWS; i++){
    for(int j = 0; j < NUM_COLS; j++){
      EEPROM.update(index, list[i][j]);
      index++;
    }
    index = index + NUM_COLS;
  } 
  updateList(list, originalIndex);
}

// Update the white/blacklist from the EEPROM
void updateList(char list[NUM_ROWS][NUM_COLS], int index){
  for(int i = 0; i < NUM_ROWS; i++){
    for(int j = 0; j < NUM_COLS; j++){
      list[i][j] = EEPROM.read(index);
      index++;
    }
    index = index + NUM_COLS;
  }
}

// Update the CANBus speed
void updateCANSpeed(){
  CAN.end();
  CAN.begin(CANSpeedArray[CANSpeed]);
  EEPROM.update(CANSpeedIndex, CANSpeed);
}

// Check if the CAN msg passes the filter
bool checkFilter(CanMsg msg){
  bool filterPass;
  char msgIdStr[NUM_COLS + 1];
  snprintf(msgIdStr, sizeof(msgIdStr), "%03X", msg.id); // Format ID as 3 hex chars
      
  if(listState == 1) { // whitelist mode
    filterPass = false;
    for(int i = 0; i < NUM_ROWS; i++) {
      bool match = true;
      for(int j = 0; j < NUM_COLS; j++) {
        if(whitelist[i][j] != msgIdStr[j]) {
          match = false;
          break;
        }
      }
      if(match) {
        filterPass = true;
        break;
      }
    }
  } else { // blacklist mode
    filterPass = true;
    for(int i = 0; i < NUM_ROWS; i++) {
      bool match = true;
      for(int j = 0; j < NUM_COLS; j++) {
        if(blacklist[i][j] != msgIdStr[j]) {
          match = false;
          break;
        }
      }
      if(match) {
        filterPass = false;
        break;
      }
    }
  }
  return filterPass;
}

// Log new CAN message to the char buffer
void logCAN(){
  if (CAN.available()){
    CanMsg const MSG = CAN.read();

    bool filterPass = true;
    if(filterState == 1){
      filterPass = checkFilter(MSG);
    }

    if(filterPass){
      char line[MSG_LENGTH];
      int len = snprintf(line, MSG_LENGTH, "%lu,%03X,%d,Rx,0,%d", currentMillis, MSG.id, MSG.isExtendedId(), MSG.data_length);
      for (int i = 0; i < byte(MSG.data_length); i++){
        if (i == 0) {
          len += snprintf(line + len, MSG_LENGTH - len, ",%02X", MSG.data[i]);
        } else {
          len += snprintf(line + len, MSG_LENGTH - len, ",%02X", MSG.data[i]);
        }
      }
      strncpy(msgBuffer[bufferIndex], line, MSG_LENGTH);
      msgBuffer[bufferIndex][MSG_LENGTH-1] = '\0';
      if (++bufferIndex >= BUFFER_SIZE){
        flushBufferToSD();
      }
    }
  }
}

// Flush the contents of the char buffer to the SD card
void flushBufferToSD() {
  // Create and open a new file
  String tempFileName = String(fileCount) + "_" + String(fileNum) + ".txt";
  if (!dataFile.isOpen()) {
    dataFile = sd.open("tempFileName", O_WRONLY | O_CREAT | O_APPEND);
  }
  // Print file header
  if(dataFile.size() == 0){
    dataFile.println("Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8");
  }
  // Flush buffer to the SD card
  for (int i = 0; i < bufferIndex; i++) {
    dataFile.println(msgBuffer[i]);
  }
  dataFile.sync();
  bufferIndex = 0;
  // Check if file size exceeds maximum
  if(dataFile.size() > 100000000){
    fileNum++;
  }
  dataFile.close();
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
        if (ndx >= NUM_CHARS) {
          ndx = NUM_CHARS - 1;
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