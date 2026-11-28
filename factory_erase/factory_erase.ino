#include <SPI.h>
#include <EEPROM.h>

// W25Q64 connections from the schematic
#define W25_CS   PB12
#define W25_SCK  PB13
#define W25_MISO PB14
#define W25_MOSI PB15

SPIClass *w25SPI = nullptr;

void w25Select() {
  digitalWrite(W25_CS, LOW);
}

void w25Deselect() {
  digitalWrite(W25_CS, HIGH);
}

uint8_t w25ReadStatus() {
  w25SPI->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  w25Select();
  w25SPI->transfer(0x05);
  uint8_t status = w25SPI->transfer(0x00);
  w25Deselect();
  w25SPI->endTransaction();
  return status;
}

bool w25WaitReady(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (w25ReadStatus() & 0x01) {
    if (millis() - start >= timeoutMs) return false;
    delay(10);
  }
  return true;
}

uint32_t w25ReadJedecId() {
  uint32_t id = 0;
  w25SPI->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  w25Select();
  w25SPI->transfer(0x9F);
  id |= (uint32_t)w25SPI->transfer(0x00) << 16;
  id |= (uint32_t)w25SPI->transfer(0x00) << 8;
  id |= (uint32_t)w25SPI->transfer(0x00);
  w25Deselect();
  w25SPI->endTransaction();
  return id;
}

void w25WriteEnable() {
  w25SPI->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  w25Select();
  w25SPI->transfer(0x06);
  w25Deselect();
  w25SPI->endTransaction();
}

bool w25ChipErase() {
  w25WriteEnable();
  w25SPI->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  w25Select();
  w25SPI->transfer(0xC7);  // Chip Erase
  w25Deselect();
  w25SPI->endTransaction();
  return w25WaitReady(180000UL);
}

void clearEEPROM() {
  int length = EEPROM.length();
  Serial1.print("EEPROM bytes: ");
  Serial1.println(length);
  for (int address = 0; address < length; address++) {
    EEPROM.write(address, 0xFF);
    if ((address & 0x3F) == 0) Serial1.print('.');
  }
  Serial1.println();
}

void setup() {
  Serial1.begin(115200);
  delay(500);
  Serial1.println();
  Serial1.println("FACTORY ERASE START");

  clearEEPROM();
  Serial1.println("EEPROM ERASED");

  pinMode(W25_CS, OUTPUT);
  w25Deselect();
  w25SPI = new SPIClass(W25_MOSI, W25_MISO, W25_SCK);
  w25SPI->begin();

  uint32_t jedec = w25ReadJedecId();
  Serial1.print("W25 JEDEC: 0x");
  Serial1.println(jedec, HEX);

  if (jedec == 0xEF4017UL) {
    Serial1.println("W25 CHIP ERASE...");
    if (w25ChipErase()) Serial1.println("W25 ERASED");
    else Serial1.println("W25 ERASE TIMEOUT");
  } else {
    Serial1.println("W25 NOT ERASED: unexpected JEDEC ID");
  }

  Serial1.println("FACTORY ERASE DONE");
  Serial1.println("Now upload the backed-up main sketch.");
}

void loop() {
  delay(1000);
}
