#include <Adafruit_NeoPixel.h>
#include <Arduino_CAN.h>
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

// Neopixels
#define PIN 3
#define NUMPIXELS 5
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Global variables
const int chipSelect = 10;
const int address = 0;
unsigned long lightDelay;
int fileNum = 0;
byte fileCount;

void setup() {
  Serial.begin(115200);

  if (!CAN.begin(CanBitRate::BR_1000k))
  {
    Serial.println("CAN.begin(...) failed.");
    for (;;) {}
  }

  if (!SD.begin(chipSelect))
  {
    Serial.println("SD.begin(...) failed.");
    for (;;) {}
  }

  // Reads and updates EEPROM
  //EEPROM.write(address, 1);
  fileCount = EEPROM.read(address);
  fileCount++;
  EEPROM.write(address, fileCount);

  lightDelay = millis();

  pixels.begin();
  pixels.setBrightness(50);
}

void loop() {
  
  // Sets PWR LED to green
  pixels.setPixelColor(0, pixels.Color(0, 150, 0));

  // Gets the current time
  unsigned long millisTime;
  millisTime = millis();

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

    if (dataFile) {
      // Print time (milliseconds)
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
  if(millisTime - lightDelay > 100)
  {
    pixels.show();
    lightDelay = millis();
  }
  Serial.println(millisTime);
}
