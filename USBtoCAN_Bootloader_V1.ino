#include <EEPROM.h>

// White/blacklist variables
byte listState = 0;
byte filterState = 0;
const int NUM_ROWS = 5;
const int NUM_COLS = 4;
char whitelist[NUM_ROWS][NUM_COLS] = {};
char blacklist[NUM_ROWS][NUM_COLS] = {};

// SD card variables
int fileCount = 0;

// CANSpeed variables
byte CANSpeed = 0;

// EEPROM variables
int filterIndex = 0;
int fileCountIndex = 1;
int CANSpeedIndex = 2;
int listStateIndex = 3;
int whitelistIndex = 4;
int blacklistIndex = whitelistIndex + NUM_ROWS*NUM_COLS;

void setup() {
  // Fill whitelist & blacklist w/ '0's
  for(int i = 0; i < NUM_ROWS; i++){
    for(int j = 0; j < NUM_COLS; j++){
      whitelist[i][j] = '0';
      blacklist[i][j] = '0';
    }
  }

  // Write initial values to EEPROM
  EEPROM.update(filterIndex, filterState);
  EEPROM.update(fileCountIndex, fileCount);
  EEPROM.update(CANSpeedIndex, CANSpeed);
  EEPROM.update(listStateIndex, listState);

  // Burn initial whitelist & blacklist to EEPROM
  burnList(blacklist, blacklistIndex);
  burnList(whitelist, whitelistIndex);
}

void loop() {
  while(1){}
}

// Burn the white/blacklist to the EEPROM
void burnList(char list[NUM_ROWS][NUM_COLS], int index){
  for(int i = 0; i < NUM_ROWS; i++){
    for(int j = 0; j < NUM_COLS; j++){
      EEPROM.update(index, list[i][j]);
      index++;
    }
    index = index + NUM_COLS;
  } 
}
