#include <Arduino_CAN.h>
#include <EEPROM.h>
#include <SdFat.h>
#include <RTC.h>
#include <SPI.h>

// CAN filter variables
static uint32_t const CAN_FILTER_MASK_STANDARD = 0x1FFC0000;
static uint32_t const CAN_FILTER_MASK_EXTENDED = 0x1FFFFFFF;

// Serial input variables
boolean newData = false;
const byte NUM_CHARS = 32;
char receivedChars[NUM_CHARS];

// White/blacklist variables
byte listState = 0;
byte filterState = 0;
const int NUM_ROWS = 5;
const int NUM_COLS = 4;
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
int fileCountIndex = 1;
int CANSpeedIndex = 2;
int listStateIndex = 3;
int whitelistIndex = 4;
int blacklistIndex = whitelistIndex + NUM_ROWS*NUM_COLS;

// CANSpeed variables
byte CANSpeed;
int CANSpeedArray[4] = {125000, 250000, 500000, 1000000};

// SD buffer variables
int bufferIndex = 0;
const int MSG_LENGTH = 100;
const int BUFFER_SIZE = 100;
char msgBuffer[BUFFER_SIZE][MSG_LENGTH];

void setup() {
  // Read & update values from the EEPROM
  filterState = EEPROM.read(filterIndex);
  fileCount = EEPROM.read(fileCountIndex) + 1;
  CANSpeed = EEPROM.read(CANSpeedIndex);
  listState = EEPROM.read(listStateIndex);
  EEPROM.update(fileCountIndex, fileCount);

  // Start the 'real time' clock + Serial, CAN, & SD communication
  RTC.begin();
  Serial.begin(115200);
  CAN.begin(CANSpeedArray[CANSpeed]);
  sd.begin(CHIP_SELECT, SD_SCK_MHZ(50));

  // Update white/blacklist from EEPROM
  updateList(whitelist, whitelistIndex);
  updateList(blacklist, blacklistIndex);

  // Update filter states from EEPROM
  if(listState == 0){
    clearFilterIDs();
  } else{
    updateFilterIDs();
  }
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
        if(listState == 0){
          clearFilterIDs();
        } else{
          updateFilterIDs();
        }
        EEPROM.update(listStateIndex, listState);
        EEPROM.update(filterIndex, filterState);
        break;
      case 'r':
        filterState = 0;
        clearFilterIDs();
        EEPROM.update(listStateIndex, listState);
        EEPROM.update(filterIndex, filterState);
        break;
      case 's':
        listState = 0;
        clearFilterIDs();
        EEPROM.update(listStateIndex, listState);
        EEPROM.update(filterIndex, filterState);
        break;
      case 't':
        listState = 1;
        updateFilterIDs();
        EEPROM.update(listStateIndex, listState);
        EEPROM.update(filterIndex, filterState);
        break;
      case 'u':
        readList(whitelist);
        updateFilterIDs();
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

// Update the filter IDs from the whitelist
void updateFilterIDs(){
  clearFilterIDs();
  CAN.setFilterMask_Extended(CAN_FILTER_MASK_EXTENDED);
  CAN.setFilterMask_Standard(CAN_FILTER_MASK_STANDARD);

  char standardArray[4];
  char extendedArray[5];

  for (int mailbox = 0; mailbox < R7FA4M1_CAN::CAN_MAX_NO_STANDARD_MAILBOXES; mailbox++){
    if(whitelist[mailbox][3] == '-'){
      snprintf(standardArray, sizeof(standardArray), "%c%c%c", whitelist[mailbox][0], whitelist[mailbox][1], whitelist[mailbox][2]);
      standardArray[3] = '\0'; // Null-terminate
      uint32_t canId = strtoul(standardArray, NULL, 16);
      CAN.setFilterId_Standard(mailbox, canId);
    } else {
      snprintf(extendedArray, sizeof(extendedArray), "%c%c%c%c", whitelist[mailbox][0], whitelist[mailbox][1], whitelist[mailbox][2], whitelist[mailbox][3]);
      extendedArray[4] = '\0'; // Null-terminate
      uint32_t canId = strtoul(extendedArray, NULL, 16);
      CAN.setFilterId_Extended(mailbox, canId);
    }
  }
}

// Clear the filter of IDs from the whitelist
void clearFilterIDs(){
  CAN.setFilterMask_Extended(0x0);
  CAN.setFilterMask_Standard(0x0);
  for (int mailbox = 0; mailbox < R7FA4M1_CAN::CAN_MAX_NO_EXTENDED_MAILBOXES; mailbox++) {
    CAN.setFilterId_Extended(mailbox, 0); // Unused mailboxes
  }
  for (int mailbox = 0; mailbox < R7FA4M1_CAN::CAN_MAX_NO_STANDARD_MAILBOXES; mailbox++) {
    CAN.setFilterId_Standard(mailbox, 0); // Unused mailboxes
  }
}

// Check if the CAN msg passes the blacklist
bool checkBlacklist(CanMsg msg){
  bool filterPass = true;
  char msgIdStr[NUM_COLS + 1];
  snprintf(msgIdStr, sizeof(msgIdStr), "%03X", msg.id);
  for(int i = 0; i < NUM_ROWS; i++) {
    bool match = true;
    if(blacklist[i][3] == '-'){
      for(int j = 0; j < NUM_COLS - 1; j++) {
        if(blacklist[i][j] != msgIdStr[j]) {
          match = false;
          break;
        }
      } 
    } else {
      for(int j = 0; j < NUM_COLS; j++) {
        if(blacklist[i][j] != msgIdStr[j]) {
          match = false;
          break;
        }
      }
    }
    if(match){
      filterPass = false;
      break;
    } else{
    }
  }
  return filterPass;
}

// Log new CAN message to the char buffer
void logCAN(){
  if (CAN.available()){
    CanMsg const MSG = CAN.read();

    bool filterPass = true;
    if(filterState == 1 && listState == 0){
      filterPass = checkBlacklist(MSG);
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
    dataFile = sd.open(tempFileName, O_WRONLY | O_CREAT | O_APPEND);
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