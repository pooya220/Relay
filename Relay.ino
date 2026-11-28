#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <utility/w5100.h>
#include "stm32f1xx.h"


#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET    -1 
#define BTN1 PB0
#define BTN2 PB1
#define REMOTE_ACTIVE_LEVEL HIGH  // RX6 D0/D1 are active HIGH
#define EEPROM_SECURE_ADDR 50
#define EEPROM_REMOTE_ADDR 51
#define RELAY1 PA0
#define RELAY2 PA1
#define W5500_RST PA2
#define W5500_INT PA3
#define W5500_CS PA4
#define BATTERY_ADC_PIN PA8   
#define BATTERY_CHRG_PIN PB10
#define BATTERY_STDBY_PIN PB11
#define REMOTE_LED_PIN PB8
#define REMOTE_SW_PIN PB9
#define W25_CS   PB12
#define W25_SCK  PB13
#define W25_MISO PB14
#define W25_MOSI PB15
#define W25_WEB_HEADER_ADDR 0x000000UL
#define W25_WEB_DATA_ADDR   0x001000UL
#define W25_WEB_MAGIC       0x4D415850UL  // multi-page external web format
#define W25_WEB_VERSION     1UL
#define W25_WEB_ASSET_COUNT 4
#define W25_PAGE_HOME       0
#define W25_PAGE_LOGIN      1
#define W25_PAGE_RTC        2
#define W25_PAGE_LOGS       3
#define W25_WEB_MAX_LENGTH  0x0EF000UL  // keep the log banks out of web data
#define W25_LOG_BANK0_ADDR  0x0F0000UL
#define W25_LOG_BANK1_ADDR  0x0F4000UL
#define W25_LOG_BANK_SIZE   0x004000UL  // 16 KB per bank
#define W25_LOG_DATA_OFFSET 0x000100UL
#define W25_LOG_MAX_RECORDS 1000
#define W25_LOG_PAGE_SIZE   50
#define PENDING_LOG_MAX      8
#define W25_LOG_RECORD_MAGIC 0x4C4F4732UL
#define W25_LOG_HEADER_MAGIC 0x4C424B32UL
#define BTN_down PB3
#define BTN_up PB4
#define BTN_SELECT PB5
#define EEPROM_IP 0      
#define EEPROM_SUBNET 4  
#define EEPROM_GATEWAY 8 
#define EEPROM_USER 100
#define EEPROM_PASS 140
#define WEB_SESSION_TIMEOUT_MS 300000UL

// The schematic routes Vsource to PA8 through R38/R39 = 50K/50K.
// TP4056/B1 is a single-cell charger, so the default range is 3.0..4.2 V.
#define BATTERY_DIVIDER_TOP_OHMS    50000UL
#define BATTERY_DIVIDER_BOTTOM_OHMS 50000UL
#define BATTERY_ADC_VREF_MV         3300UL
#define BATTERY_ADC_MAX             4095UL
#define BATTERY_EMPTY_MV            3000UL
#define BATTERY_FULL_MV             4200UL
#define BATTERY_READ_INTERVAL_MS    1000UL
#define BATTERY_STATUS_SAMPLE_MS    50UL
#define BATTERY_FULL_CONFIRM_MS     2500UL
#define NETWORK_STATUS_INTERVAL_MS  250UL
#define NETWORK_ACTIVITY_HOLD_MS    1500UL
// PA8 is the Vsource net in the schematic, but PA8 has no ADC channel on
// STM32F103C8T6. Keep voltage conversion disabled until Vsource is rerouted
// to an actual ADC pin on the PCB.
#define BATTERY_ADC_AVAILABLE       0

struct W25WebAsset {
  uint32_t address;
  uint32_t length;
};

struct W25WebHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t count;
  W25WebAsset assets[W25_WEB_ASSET_COUNT];
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SPIClass *w25SPI = nullptr;
byte mac[] = {0xDE,0xAD,0xBE,0xEF,0xFE,0xED};
EthernetServer server(80);
IPAddress ip, subnet, gateway;

uint32_t w25JedecId = 0;
bool w25Ready = false;
bool w25WebValid = false;
W25WebHeader w25WebHeader;
uint32_t w25LogNextAddress = W25_LOG_BANK0_ADDR + W25_LOG_DATA_OFFSET;
bool w25LogReady = false;
uint8_t w25LogActiveBank = 0;
uint32_t w25LogGeneration = 0;
uint16_t w25LogCount = 0;
uint32_t deviceStartTime = 0;
bool startTimeNeedsSync = true;
unsigned long remoteIgnoreUntil = 0;
uint8_t pendingRelay[PENDING_LOG_MAX];
uint8_t pendingState[PENDING_LOG_MAX];
uint8_t pendingSource[PENDING_LOG_MAX];
uint32_t pendingSourceIp[PENDING_LOG_MAX];
uint8_t pendingRelayCount = 0;

// The relay drivers in the schematic are active LOW; both relays start OFF.
bool relay1State = false;
bool relay2State = false;
bool secureMode = false;
bool remoteEnabled = true;
enum NetworkIconState {
  NETWORK_UNPLUGGED,
  NETWORK_PLUGGED,
  NETWORK_ACTIVE
};
NetworkIconState networkIconState = NETWORK_UNPLUGGED;
unsigned long networkLastStatusCheck = 0;
unsigned long networkActivityUntil = 0;
uint32_t webSessionOwnerIp = 0;
unsigned long webSessionCreatedAt = 0;

uint16_t batteryRaw = 0;
uint32_t batteryMilliVolts = 0;
uint8_t batteryPercent = 0;
bool batteryCharging = false;
bool batteryFull = false;
bool batteryPresent = false;
unsigned long batteryLastRead = 0;
bool batteryInitialized = false;
bool oledReady = false;
bool batteryStatusInitialized = false;
bool batteryLastChrgLow = false;
bool batteryNoCellPulseSeen = false;
unsigned long batteryStatusLastSample = 0;
unsigned long batteryFullCandidateSince = 0;

String eUser;
String ePass;

// ===== LCD menu =====
#define MENU_ENTER_HOLD_MS  3000UL
#define MENU_BACK_HOLD_MS   3000UL
#define MENU_IDLE_TIMEOUT_MS 10000UL
#define MENU_DEBOUNCE_MS    80UL
#define MENU_TOP_Y          8
#define MENU_ITEM_Y         24
#define MENU_ITEM_STEP      10
#define MENU_VISIBLE_ITEMS  3
#define MENU_ITEM_X         8
#define MENU_MESSAGE_MS     2000UL
#define REMOTE_LEARN_HOLD_MS   2200UL
#define REMOTE_LEARN_WINDOW_MS 10000UL
#define REMOTE_CLEAR_HOLD_MS    20000UL
#define REMOTE_ACTION_RESULT_MS 1800UL

enum MenuScreen : uint8_t {
  MENU_CLOSED,
  MENU_MAIN,
  MENU_RELAY,
  MENU_NETWORK,
  MENU_NETWORK_EDIT,
  MENU_RESET,
  MENU_REMOTE,
  MENU_REMOTE_LEARN,
  MENU_BATTERY,
  MENU_LOGS
};

enum MenuNetworkEditMode : uint8_t {
  MENU_NET_SELECT_DIGIT,
  MENU_NET_EDIT_DIGIT
};

enum RemoteLearnState : uint8_t {
  REMOTE_LEARN_IDLE,
  REMOTE_LEARN_HOLDING,
  REMOTE_LEARN_WAITING,
  REMOTE_LEARN_CLEARING,
  REMOTE_LEARN_RESULT
};

bool menuActive = false;
MenuScreen menuScreen = MENU_CLOSED;
uint8_t menuIndex = 0;
uint8_t menuNetField = 0;
uint8_t menuNetDigit = 0;
uint8_t menuNetEditMode = MENU_NET_SELECT_DIGIT;
uint8_t menuNetBytes[4] = {0, 0, 0, 0};
uint16_t menuLogCursor = 0;
unsigned long menuLastActivity = 0;
unsigned long menuLastDraw = 0;
bool menuNeedsRedraw = false;
bool menuSelectHeld = false;
unsigned long menuSelectPressedAt = 0;
unsigned long menuLastUpEvent = 0;
unsigned long menuLastDownEvent = 0;
unsigned long menuLastSelectEvent = 0;
bool menuLastUp = false;
bool menuLastDown = false;
bool menuLastSelect = false;
RemoteLearnState remoteLearnState = REMOTE_LEARN_IDLE;
unsigned long remoteLearnStartedAt = 0;
bool remoteLearnKeySeen = false;
bool remoteLearnLastCh1 = false;
bool remoteLearnLastCh2 = false;
bool remoteClearMode = false;
MenuScreen remoteActionReturnScreen = MENU_REMOTE;

bool parseManualRTC(String req, uint32_t &value);
void showRTCPage(EthernetClient &client);
void printPackedIP(Print &out, uint32_t value);
void serviceLCDMenu();
void drawLCDMenu();
void updateOLED();
static void refreshOLEDForStateChange();
void updateBattery();
void updateBatteryChargerStatus();
void updateNetworkStatus();
void markNetworkActivity();
bool resetW5500();
static void drawLCDBattery();
static void drawLCDRemoteLearn();
static void startRemoteLearn();
static void startRemoteClear();
static void stopRemoteLearn();
static void serviceRemoteLearn();
static void pressRemoteSwitch();
static void releaseRemoteSwitch();

uint32_t w25ReadJedecId() {
  uint32_t id = 0;
  w25SPI->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(W25_CS, LOW);
  w25SPI->transfer(0x9F);
  id |= (uint32_t)w25SPI->transfer(0x00) << 16;
  id |= (uint32_t)w25SPI->transfer(0x00) << 8;
  id |= (uint32_t)w25SPI->transfer(0x00);
  digitalWrite(W25_CS, HIGH);
  w25SPI->endTransaction();
  return id;
}

void initW25() {
  pinMode(W25_CS, OUTPUT);
  digitalWrite(W25_CS, HIGH);
  w25SPI = new SPIClass(W25_MOSI, W25_MISO, W25_SCK);
  if (!w25SPI) {
    Serial1.println("W25 SPI ALLOC FAILED");
    return;
  }
  w25SPI->begin();
  delay(2);

  w25JedecId = w25ReadJedecId();
  w25Ready = (w25JedecId != 0x000000UL && w25JedecId != 0xFFFFFFUL);

  Serial1.print("W25 JEDEC ID: 0x");
  Serial1.println(w25JedecId, HEX);
  Serial1.println(w25Ready ? "W25 READY" : "W25 NOT FOUND");
}


uint8_t w25ReadStatus() {
  uint8_t status;
  w25SPI->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(W25_CS, LOW);
  w25SPI->transfer(0x05);
  status = w25SPI->transfer(0x00);
  digitalWrite(W25_CS, HIGH);
  w25SPI->endTransaction();
  return status;
}

bool w25WaitReady(unsigned long timeoutMs) {
  unsign