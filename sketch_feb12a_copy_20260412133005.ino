  #include <SPI.h>
  #include <Ethernet.h>
  #include <EEPROM.h>
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>

  // مشخصات OLED
  #define SCREEN_WIDTH 128 // پیکسل عرض
  #define SCREEN_HEIGHT 64 // پیکسل ارتفاع
  #define OLED_RESET    -1 // اکثر ماژول‌ها RESET ندارن
  #define BTN1 PB0
  #define BTN2 PB1

  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

  #define RELAY1 PA0
  #define RELAY2 PA1
  #define W5500_CS PA4
  #define BTN_RESET PA15

  unsigned long btnResetPressTime = 0;
  bool btnResetActive = false;


  byte mac[] = {0xDE,0xAD,0xBE,0xEF,0xFE,0xED};

  EthernetServer server(80);

  bool relay1State = true;
  bool relay2State = true;

  // ===== EEPROM آدرس‌ها =====
  #define EEPROM_IP 0      // 4 بایت
  #define EEPROM_SUBNET 4  // 4 بایت
  #define EEPROM_GATEWAY 8 // 4 بایت

  IPAddress ip, subnet, gateway;

  bool isLoggedIn = false;
String USERNAME = "admin";
String PASSWORD = "maxa123";
bool secureMode = true;
  

void showLoginPage(EthernetClient &client) {

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  client.println("<title>Login</title>");

  client.println("</head>");

  client.println("<body style='background:black;font-family:Arial;text-align:center;color:white;'>");

  // عنوان
  client.println("<h1 style='color:orange;margin-top:40px;'>LOGIN</h1>");

  client.println("<form action='/login'>");

  // یوزرنیم
  client.println("<input "
                 "style='width:200px;padding:10px;margin:10px;"
                 "border:2px solid orange;background:black;color:white;"
                 "border-radius:8px;text-align:center;' "
                 "type='text' name='user' placeholder='USERNAME'>");

  client.println("<br>");

  // پسورد
  client.println("<input "
                 "style='width:200px;padding:10px;margin:10px;"
                 "border:2px solid orange;background:black;color:white;"
                 "border-radius:8px;text-align:center;' "
                 "type='password' name='pass' placeholder='PASSWORD'>");

  client.println("<br><br>");

  // دکمه ENTER
  client.println("<button type='submit' "
                 "style='width:120px;padding:8px;"
                 "background:orange;color:black;"
                 "border:none;border-radius:6px;"
                 "font-weight:bold;cursor:pointer;'>");

  client.println("ENTER");

  client.println("</button>");

  client.println("</form>");

  client.println("</body></html>");
}
  // ===== تابع ریست شبکه با تنظیمات جدید =====
  void applyNetwork() {
  Ethernet.begin(mac, ip, gateway, subnet);
  }
  void initEEPROMDefaultIP() {
    IPAddress defaultIP(192,168,1,3);
    IPAddress defaultSubnet(255,255,255,0);
    IPAddress defaultGateway(192,168,1,1);

    byte test = EEPROM.read(0);

    if (test == 0xFF || test == 0x00) {

      for (int i = 0; i < 4; i++) {
        EEPROM.write(EEPROM_IP + i, defaultIP[i]);
        EEPROM.write(EEPROM_SUBNET + i, defaultSubnet[i]);
        EEPROM.write(EEPROM_GATEWAY + i, defaultGateway[i]);
      }

      // ❌ NO commit here
    }
  }
  // ===== ذخیره IP به EEPROM =====
  void saveIPAddressToEEPROM(int startAddr, IPAddress ipAddr) {
  for (int i = 0; i < 4; i++) {
  EEPROM.write(startAddr + i, ipAddr[i]);
  }
  }

  // ===== خواندن IP از EEPROM =====
  IPAddress readIPAddressFromEEPROM(int startAddr) {
  byte b[4];
  for (int i = 0; i < 4; i++) {
  b[i] = EEPROM.read(startAddr + i);
  }
  return IPAddress(b[0], b[1], b[2], b[3]);
  }

  void toggleRelay(int relay) {

    if (relay == 1) {
      relay1State = !relay1State;
      digitalWrite(RELAY1, relay1State ? LOW : HIGH);
    }

    if (relay == 2) {
      relay2State = !relay2State;
      digitalWrite(RELAY2, relay2State ? LOW : HIGH);
    }

    updateOLED();
  }

  void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1); // سایز متن کوچک برای 3 خط
  display.setTextColor(SSD1306_WHITE);

  // خط اول: عنوان
  display.setCursor(0,0);
  display.print("      Maxa Relay");


// ===== LOCK ICON =====
display.fillRect(120, 0, 8, 8, SSD1306_BLACK); // پاک کردن محل آیکون

if (secureMode) {
  const unsigned char lock_bmp [] PROGMEM = {
    0b00111100,
    0b01111110,
    0b01100110,
    0b01111110,
    0b01111110,
    0b01111110,
    0b01100110,
    0b00111100
  };

  display.drawBitmap(120, 0, lock_bmp, 8, 8, SSD1306_WHITE);
}


  // خط دوم: IP
  display.setCursor(0,30); // 10 پیکسل پایین‌تر برای خط دوم
  display.print("IP:  ");
  display.print(ip);

  // خط سوم: وضعیت رله‌ها
  display.setCursor(0,50); // 20 پیکسل پایین‌تر برای خط سوم
  display.print("R1:  ");
  display.print(relay1State ? "ON" : "OFF");
  display.print("   R2:  ");
  display.print(relay2State ? "ON" : "OFF");

  

  display.display();
  }

  void redirectToHome(EthernetClient &client) {
  client.println("HTTP/1.1 302 Found");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();
  client.stop();
  }

  void setup() {

  randomSeed(analogRead(0));

  initEEPROMDefaultIP();

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  digitalWrite(RELAY1, HIGH); // OFF
  digitalWrite(RELAY2, HIGH);

  pinMode(BTN_RESET, INPUT_PULLDOWN);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // آدرس 0x3C استاندارده
  // اگه OLED وصل نبود، برنامه ادامه پیدا کنه ولی چیزی نشون داده نمیشه
  Serial.println(F("SSD1306 allocation failed"));

  } else {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("IP: ");
  display.print(ip);
  display.display();
  }



  pinMode(W5500_CS, OUTPUT);
  digitalWrite(W5500_CS, HIGH);

  SPI.begin();
  Ethernet.init(W5500_CS);

  // ===== خواندن شبکه از EEPROM یا مقدار پیشفرض =====
  ip = readIPAddressFromEEPROM(EEPROM_IP);
  subnet = readIPAddressFromEEPROM(EEPROM_SUBNET);
  gateway = readIPAddressFromEEPROM(EEPROM_GATEWAY);

  // اگر EEPROM خالی بود، مقدار پیشفرض
  if (ip[0]==0 && ip[1]==0 && ip[2]==0 && ip[3]==0) ip = IPAddress(192,168,1,3);
  if (subnet[0]==0) subnet = IPAddress(255,255,255,0);
  if (gateway[0]==0) gateway = IPAddress(192,168,1,1);

  applyNetwork();
  server.begin();

  display.clearDisplay();
  display.setCursor(0,20);
  display.setTextSize(3);      // اندازه بزرگ
  display.print("   R");
  display.display();
  delay(3000);                 // ۱ ثانیه صبر کن
  updateOLED();
  }

  void loop() {

    
    // ===== دکمه ریست شبکه (بیار اول loop) =====
    if (digitalRead(BTN_RESET) == HIGH) {
      if (!btnResetActive) {
        btnResetActive = true;
        btnResetPressTime = millis();
      } else {
        if (millis() - btnResetPressTime >= 5000) {

          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0,20);
          display.print("Reset Network");
          display.display();

          ip = IPAddress(192,168,1,3);
          subnet = IPAddress(255,255,255,0);
          gateway = IPAddress(192,168,1,1);

          saveIPAddressToEEPROM(EEPROM_IP, ip);
          saveIPAddressToEEPROM(EEPROM_SUBNET, subnet);
          saveIPAddressToEEPROM(EEPROM_GATEWAY, gateway);

          applyNetwork();

          delay(2000);
          updateOLED();

          btnResetActive = false;
        }
      }
    } else {
      btnResetActive = false;
    }



        static bool lastBtn1 = HIGH;
  static bool lastBtn2 = HIGH;

  bool currentBtn1 = digitalRead(BTN1);
  bool currentBtn2 = digitalRead(BTN2);

  // تشخیص لبه فشرده شدن
  if (lastBtn1 == HIGH && currentBtn1 == LOW) {
    toggleRelay(1);
    delay(200); // debounce ساده
  }

  if (lastBtn2 == HIGH && currentBtn2 == LOW) {
    toggleRelay(2);
    delay(200);
  }

  lastBtn1 = currentBtn1;
  lastBtn2 = currentBtn2;

  EthernetClient client = server.available();
  if (!client) {
    // فقط وب رد بشه، نه کل loop
  } else {
    // کد وب
  }

  String req = "";
  while (client.connected()) {
  if (client.available()) {
  char c = client.read();
  if (c == '\n') break;
  req += c;
  }
  }
  
  

 bool clientAllowed = (!secureMode) || isLoggedIn;

  if (req.indexOf("GET /logout") >= 0) {
    isLoggedIn = false;   // اگر سیستم ساده لاگین داری
    redirectToHome(client);
    return;
  } 

 if (req.indexOf("GET /securetoggle") >= 0) {
  secureMode = !secureMode;
  redirectToHome(client);
  return;
}
  // ===== LOGIN =====

if (req.indexOf("GET /login?") >= 0) {

  int u1 = req.indexOf("user=") + 5;
  int u2 = req.indexOf('&', u1);

  int p1 = req.indexOf("pass=") + 5;
  int p2 = req.indexOf(' ', p1);
  if (p2 == -1) p2 = req.length();

  String user = req.substring(u1, u2);
  String pass = req.substring(p1, p2);

  if (user == USERNAME && pass == PASSWORD) {
    isLoggedIn = true;

    client.println("HTTP/1.1 302 Found");
    client.println("Location: /");
    client.println("Connection: close");
    client.println();
    client.stop();
    return;
  }

  showLoginPage(client);
  client.stop();
  return;
}
if (!isLoggedIn && req.indexOf("GET /login") == -1) {
  showLoginPage(client);
  client.stop();
  return;
}
if (req.indexOf("GET /securetoggle") >= 0) {
  secureMode = !secureMode;
  redirectToHome(client);
  return;
}
if (secureMode && !clientAllowed) {

  if (req.indexOf("GET /login") == -1) {
    showLoginPage(client);
    client.stop();
    return;
  }
}
  // ===== کنترل Relay =====
  if (req.indexOf("GET /on1 ") >= 0) { relay1State = true; digitalWrite(RELAY1, LOW); redirectToHome(client);updateOLED(); return; }
  if (req.indexOf("GET /off1 ") >= 0) { relay1State = false; digitalWrite(RELAY1, HIGH); redirectToHome(client);updateOLED(); return; }
  if (req.indexOf("GET /on2 ") >= 0) { relay2State = true; digitalWrite(RELAY2, LOW); redirectToHome(client);updateOLED(); return; }
  if (req.indexOf("GET /off2 ") >= 0) { relay2State = false; digitalWrite(RELAY2, HIGH); redirectToHome(client);updateOLED(); return; }
  if (req.startsWith("GET /toggle1")) { relay1State = !relay1State; digitalWrite(RELAY1, relay1State ? LOW : HIGH); redirectToHome(client);updateOLED(); return;}
  if (req.startsWith("GET /toggle2")) { relay2State = !relay2State; digitalWrite(RELAY2, relay2State ? LOW : HIGH); redirectToHome(client);updateOLED(); return;}
  if (req.indexOf("GET /out?1:1 ") >= 0) { relay1State = true; digitalWrite(RELAY1, LOW); redirectToHome(client);updateOLED(); return; }
  if (req.indexOf("GET /out?1:0 ") >= 0) { relay1State = false; digitalWrite(RELAY1, HIGH); redirectToHome(client);updateOLED(); return; }
  if (req.indexOf("GET /out?2:1 ") >= 0) { relay2State = true; digitalWrite(RELAY2, LOW); redirectToHome(client);updateOLED(); return; }
  if (req.indexOf("GET /out?2:0 ") >= 0) { relay2State = false; digitalWrite(RELAY2, HIGH); redirectToHome(client);updateOLED(); return; }

  // ===== ذخیره تنظیمات شبکه =====
  if (req.indexOf("GET /setnetwork") >= 0) {
  int ip1, ip2, ip3, ip4;
  int mask1, mask2, mask3, mask4;
  int gw1, gw2, gw3, gw4;


  sscanf(req.c_str(), "GET /setnetwork?ip=%d.%d.%d.%d&subnet=%d.%d.%d.%d&gateway=%d.%d.%d.%d",
        &ip1,&ip2,&ip3,&ip4,
        &mask1,&mask2,&mask3,&mask4,
        &gw1,&gw2,&gw3,&gw4);

  ip = IPAddress(ip1,ip2,ip3,ip4);
  subnet = IPAddress(mask1,mask2,mask3,mask4);
  gateway = IPAddress(gw1,gw2,gw3,gw4);

  saveIPAddressToEEPROM(EEPROM_IP, ip);
  saveIPAddressToEEPROM(EEPROM_SUBNET, subnet);
  saveIPAddressToEEPROM(EEPROM_GATEWAY, gateway);

  applyNetwork();
  redirectToHome(client);
  updateOLED();
  return;


  }



// ===== وضعیت JSON =====
if (req.indexOf("GET /status") >= 0) {

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  client.print("{\"relay1\":");
  client.print(relay1State ? 1 : 0);
  client.print(",\"relay2\":");
  client.print(relay2State ? 1 : 0);
  client.println("}");

  client.stop();
  return;
}
  // ===== صفحه اصلی =====
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html><head><meta charset='utf-8'>");
  client.println("<title>Maxa Relay</title>");
  //client.println("<meta http-equiv='refresh' content='2'>");
  // CSS دکمه شبکه
  client.println("<style>");
  client.println(".secureBtn { position:absolute; top:10px; left:120px; padding:8px 12px; background:orange; color:black; border:none; border-radius:5px; cursor:pointer; z-index:1000; }");
  client.println(".logoutBtn { position:absolute; top:10px; right:10px; padding:8px 12px; background:orange; color:black; border:none; border-radius:5px; cursor:pointer; z-index:1000; }");
  client.println(".networkBtn { position:absolute; top:10px; left:10px; padding:8px 12px; background:orange; color:black; border:none; border-radius:5px; cursor:pointer; z-index:1000; }");
  client.println("</style>");

  // JS باز و بسته کردن فرم
  client.println("<script>");
  client.println("function toggleNetwork() {");
  client.println("  var x = document.getElementById('networkForm');");
  client.println("  if (x.style.display === 'none') x.style.display='block'; else x.style.display='none';");
  client.println("}");
  client.println("</script>");

  client.println("</head><body style='background:#111;color:white;font-family:Arial;text-align:center;'>");

  // دکمه شبکه
  client.println("<button onclick='toggleNetwork()' class='networkBtn'>Network</button>");
  client.println("<button onclick=\"window.location='/logout'\" class='logoutBtn'>Logout</button>");
  // دکمه Network
client.println("<button onclick='toggleNetwork()' class='networkBtn'>Network</button>");

// دکمه Logout (سمت راست بالا)
client.println("<button onclick=\"window.location='/logout'\" "
               "style='position:absolute;top:10px;right:10px;"
               "padding:8px 12px;border-radius:5px;"
               "background:orange;color:black;border:none;"
               "cursor:pointer;z-index:1000;'>Logout</button>");

// دکمه Secure
client.print("<button onclick=\"window.location='/securetoggle'\" "
             "style='position:absolute;top:10px;left:120px;"
             "padding:8px 12px;border-radius:5px;"
             "cursor:pointer;z-index:1000;");

if (secureMode) {
  client.print("background:black;color:orange;border:2px solid orange;'>");
  client.print("SECURE ON");
} else {
  client.print("background:orange;color:black;border:none;'>");
  client.print("SECURE OFF");
}

client.println("</button>");

  // عنوان
  client.println("<h1 style='margin-top:30px;'>"
  "<span style='color:orange;background:black;padding:18px 36px;'>Maxa</span>"
  "<span style='color:black;background:orange;padding:18px 36px;'>Relay</span>");

  // Relay 1
  client.println("<div style='margin-top:120px;'>");
  client.print("<p>Relay 1: <b style='color:"); client.print(relay1State?"lime":"red"); client.print(";'>");
  client.print(relay1State?"ON":"OFF"); client.println("</b></p>");
  client.println("<form action='/toggle1'><button style='font-size:18px;padding:10px 30px;background:orange;color:black;border:none;border-radius:8px;'>TOGGLE</button></form>");
  client.println("</div>");

  // Relay 2
  client.println("<div style='margin-top:30px;'>");
  client.print("<p>Relay 2: <b style='color:"); client.print(relay2State?"lime":"red"); client.print(";'>");
  client.print(relay2State?"ON":"OFF"); client.println("</b></p>");
  client.println("<form action='/toggle2'><button style='font-size:18px;padding:10px 30px;background:orange;color:black;border:none;border-radius:8px;'>TOGGLE</button></form>");
  client.println("</div>");

  // فرم شبکه (مخفی)
  client.println("<div id='networkForm' style='display:none; position:absolute; top:50px; left:10px; background:#222; padding:15px; border-radius:8px; color:white;'>");
  client.println("<form action='/setnetwork'>");
  client.print("IP Address: <input type='text' name='ip' value='"); client.print(ip); client.println("'><br>");
  client.print("Subnet Mask: <input type='text' name='subnet' value='"); client.print(subnet); client.println("'><br>");
  client.print("Gateway: <input type='text' name='gateway' value='"); client.print(gateway); client.println("'><br>");
  client.println("<button type='submit' style='font-size:16px;padding:8px 20px;background:green;color:white;border:none;border-radius:6px;'>Save</button>");
  client.println("</form></div>");
  client.println("</body></html>");
  client.stop();
  }


