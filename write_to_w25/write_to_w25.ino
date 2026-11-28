#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>

#define W25_CS   PB12
#define W25_SCK  PB13
#define W25_MISO PB14
#define W25_MOSI PB15
#define W25_WEB_HEADER_ADDR 0x000000UL
#define W25_WEB_DATA_ADDR   0x001000UL
#define W25_WEB_MAGIC       0x4D415850UL
#define W25_WEB_VERSION     1UL
#define W25_WEB_ASSET_COUNT 4
#define W25_PAGE_HOME       0
#define W25_PAGE_LOGIN      1
#define W25_PAGE_RTC        2
#define W25_PAGE_LOGS       3
#define W25_WEB_MAX_LENGTH  0x0EF000UL

SPIClass *w25SPI = nullptr;
bool w25Ready = false;
uint32_t w25JedecId = 0;

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

// همان صفحه‌ی اصلی فعلی؛ این نسخه فقط یک‌بار توسط این اسکچ روی W25 نوشته می‌شود.
IPAddress ip, subnet, gateway;
W25WebHeader webHeader;
uint32_t webBytes = 0;

void renderHomePageBody(Print &client) {
  client.println("<!DOCTYPE html><html>");
  client.println("<head>");
  client.println("<meta charset='utf-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  client.println("<title>Maxa Relay</title>");

  client.println("<style>");

  client.println("body{margin:0;padding:0;background:#efeff2;font-family:'Ravi',Tahoma,sans-serif;direction:rtl;}");
  client.println("button,input{font-family:inherit;}");

  client.println(".card{");
  client.println("width:430px;");
  client.println("margin:20px auto;");
  client.println("background:white;");
  client.println("border-radius:22px;");
  client.println("padding:22px;");
  client.println("box-shadow:0 0 15px rgba(0,0,0,0.08);");
  client.println("}");

  client.println(".topbar{");
  client.println("display:flex;");
  client.println("justify-content:space-between;");
  client.println("align-items:center;");
  client.println("margin-bottom:25px;");
  client.println("direction:ltr;");
  client.println("}");

  client.println(".logoutBtn{");
  client.println("border:3px solid red;");
  client.println("background:white;");
  client.println("color:red;");
  client.println("font-family:'Ravi',Tahoma,sans-serif;");
  client.println("padding:6px 14px;");
  client.println("width:68px;");
  client.println("height:36px;");
  client.println("box-sizing:border-box;");
  client.println("border-radius:8px;");
  client.println("font-size:14px;");
  client.println("font-weight:bold;"); 
  client.println("cursor:pointer;");
  client.println("}");

  client.println(".logo{");
  client.println("font-size:30px;");
  client.println("font-weight:bold;");
  client.println("color:#0d223d;");
  client.println("}");

  client.println(".switchCard{");
  client.println("border:1px solid #ddd;");
  client.println("border-radius:14px;");
  client.println("padding:14px;");
  client.println("background:white;");
  client.println("text-align:center;");
  client.println("}");

  client.println(".switchCardTitle{");
  client.println("font-size:14px;");
  client.println("font-weight:bold;");
  client.println("color:#444;");
  client.println("margin-bottom:12px;");
  client.println("}");

  client.println(".switchStatus{");
  client.println("margin-top:10px;");
  client.println("font-size:13px;");
  client.println("font-weight:bold;");
  client.println("}");

  client.println(".onText{color:#2cab55;}");
  client.println(".offText{color:#d43b3b;}");

  client.println(".sectionTitle{");
  client.println("font-size:17px;");
  client.println("font-weight:bold;");
  client.println("color:#222;");
  client.println("margin-top:35px;");
  client.println("margin-bottom:8px;");
  client.println("}");

  client.println(".subTitle{");
  client.println("font-size:15px;");
  client.println("color:#777;");
  client.println("margin-bottom:20px;");
  client.println("}");

  client.println(".row{");
  client.println("display:flex;");
  client.println("justify-content:space-between;");
  client.println("align-items:center;");
  client.println("margin-bottom:25px;");
  client.println("}");

  client.println(".switchBox{text-align:center;}");

  client.println(".switchText{");
  client.println("font-size:14px;");
  client.println("color:#555;");
  client.println("margin-bottom:10px;");
  client.println("}");

  client.println(".switch{");
  client.println("position:relative;");
  client.println("display:inline-block;");
  client.println("width:36px;");
  client.println("height:18px;");
  client.println("}");

  client.println(".switch input{display:none;}");

  client.println(".slider{");
  client.println("position:absolute;");
  client.println("cursor:pointer;");
  client.println("top:0;");
  client.println("left:0;");
  client.println("right:0;");
  client.println("bottom:0;");
  client.println("background:#ccc;");
  client.println("transition:.3s;");
  client.println("border-radius:34px;");
  client.println("}");

  /* ✅ FIX فقط این بخش */
  client.println(".toggleInput{");
  client.println("position:relative;");
  client.println("border:1px solid #ddd;");
  client.println("border-radius:14px;");
  client.println("padding:14px 14px 14px 55px;");
  client.println("background:white;");
  client.println("min-height:70px;");
  client.println("box-sizing:border-box;");
  client.println("}");

  client.println(".toggleSwitchWrap{");
  client.println("position:absolute;");
  client.println("left:12px;");
  client.println("top:50%;");
  client.println("transform:translateY(-50%);");
  client.println("}");

  client.println(".toggleLabel{");
  client.println("position:absolute;");
  client.println("top:-9px;");
  client.println("right:12px;");
  client.println("background:white;");
  client.println("padding:0 6px;");
  client.println("font-size:10px;");
  client.println("font-weight:bold;");
  client.println("color:#666;");
  client.println("}");

  client.println(".toggleStatus{");
  client.println("font-size:10px;");
  client.println("font-weight:bold;");
  client.println("margin-top:8px;");
  client.println("color:#777;");
  client.println("}");

  client.println(".inputGroup{");
  client.println("position:relative;");
  client.println("margin-top:12px;");
  client.println("}");

  client.println(".inputGroup input{");
  client.println("width:100%;");
  client.println("padding:14px 10px 10px 10px;");
  client.println("border:1px solid #ddd;");
  client.println("border-radius:12px;");
  client.println("font-size:15px;");
  client.println("background:white;");
  client.println("box-sizing:border-box;");
  client.println("outline:none;");
  client.println("}");

  client.println(".inputGroup label{");
  client.println("position:absolute;");
  client.println("top:-9px;");
  client.println("right:12px;");
  client.println("background:white;");
  client.println("padding:0 6px;");
  client.println("font-size:10px;");
  client.println("color:#777;");
  client.println("font-weight:bold;");
  client.println("}");

  client.println(".slider:before{");
  client.println("position:absolute;");
  client.println("content:'';");
  client.println("height:12px;");
  client.println("width:12px;");
  client.println("left:3px;");
  client.println("bottom:3px;");
  client.println("background:white;");
  client.println("transition:.3s;");
  client.println("border-radius:50%;");
  client.println("}");

  client.println("input:checked + .slider{background:#41b6e6;}");
  client.println("input:checked + .slider:before{transform:translateX(20px);}");


  client.println(".grid{");
  client.println("display:grid;");
  client.println("grid-template-columns:1fr 1fr 1fr;");
  client.println("gap:14px;");
  client.println("margin-top:15px;");
  client.println("}");

  client.println(".label{");
  client.println("font-size:13px;");
  client.println("color:#666;");
  client.println("margin-bottom:7px;");
  client.println("}");

  client.println(".saveBtn{");
  client.println("margin-top:25px;");
  client.println("background:#08233d;");
  client.println("color:white;");
  client.println("border:none;");
  client.println("font-family:'Ravi',Tahoma,sans-serif;");
  client.println("padding:6px 14px;");
  client.println("width:68px;