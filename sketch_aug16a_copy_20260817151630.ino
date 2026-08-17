#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============================================================
// NADI ESP8266 - LIGHTING / MONITOR SYSTEM
// Google Sheets + WebUI + LCD + Relay
// OOM SAFE VERSION
// ESP8266 CORE 3.1.2 COMPATIBLE
// ============================================================


// ============================================================
// WIFI
// ============================================================

const char WIFI_SSID[] = "Cactusrift";
const char WIFI_PASSWORD[] = "Siniguabisikin";


// ============================================================
// GOOGLE APPS SCRIPT
// ============================================================

const char GOOGLE_SCRIPT_URL[] =
  "https://script.google.com/macros/s/AKfycbzWjUf1vesVyh3Ig4kP_9EKRxjtJ93vD96XzzE46meItaI3MCXixouHxQpvXyFhACuT/exec";


// ============================================================
// PIN CONFIGURATION
// ============================================================

#define SENSOR_PIN A0

#define LCD_SDA D2
#define LCD_SCL D1

#define RELAY_PIN D5

#define RELAY_ON LOW
#define RELAY_OFF HIGH


// ============================================================
// LCD
// ============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);


// ============================================================
// WEB SERVER
// ============================================================

ESP8266WebServer server(80);


// ============================================================
// VARIABLES
// ============================================================

int rawValue = 0;
int levelValue = 0;

bool relayState = false;
bool autoMode = true;


// ============================================================
// TIMER
// ============================================================

unsigned long lastSensor = 0;
unsigned long lastGoogle = 0;
unsigned long lastLCD = 0;
unsigned long lastWiFiCheck = 0;

const unsigned long SENSOR_INTERVAL = 1000UL;
const unsigned long GOOGLE_INTERVAL = 60000UL;
const unsigned long LCD_INTERVAL = 1000UL;
const unsigned long WIFI_CHECK_INTERVAL = 30000UL;


// ============================================================
// STATUS
// ============================================================

const char* statusText()
{
  if (levelValue >= 50)
  {
    return "BRIGHT";
  }

  return "DARK";
}


// ============================================================
// READ SENSOR
// ============================================================

void readSensor()
{
  rawValue = analogRead(SENSOR_PIN);

  /*
     Calibration:

     raw 650 = level 0%
     raw 300 = level 100%

     Semakin kecil raw:
     semakin terang
  */

  levelValue = map(rawValue, 650, 300, 0, 100);

  levelValue = constrain(levelValue, 0, 100);
}


// ============================================================
// APPLY RELAY
// ============================================================

void applyRelay()
{
  if (relayState)
  {
    digitalWrite(RELAY_PIN, RELAY_ON);
  }
  else
  {
    digitalWrite(RELAY_PIN, RELAY_OFF);
  }
}


// ============================================================
// AUTO MODE
// ============================================================

void updateAuto()
{
  if (!autoMode)
  {
    return;
  }

  /*
     Jika level < 50%
     Relay ON

     Jika level >= 50%
     Relay OFF
  */

  if (levelValue < 50)
  {
    relayState = true;
  }
  else
  {
    relayState = false;
  }

  applyRelay();
}


// ============================================================
// LCD CLEAR LINE
// ============================================================

void lcdClearLine(byte row)
{
  lcd.setCursor(0, row);
  lcd.print("                ");
}


// ============================================================
// UPDATE LCD
// ============================================================

void updateLCD()
{
  // ----------------------------------------------------------
  // BARIS 1
  // ----------------------------------------------------------

  lcdClearLine(0);

  lcd.setCursor(0, 0);

  lcd.print("L:");
  lcd.print(levelValue);
  lcd.print("% ");

  lcd.print(statusText());


  // ----------------------------------------------------------
  // BARIS 2
  // ----------------------------------------------------------

  lcdClearLine(1);

  lcd.setCursor(0, 1);

  lcd.print("R:");

  if (relayState)
  {
    lcd.print("ON ");
  }
  else
  {
    lcd.print("OFF");
  }

  lcd.print(" ");

  if (autoMode)
  {
    lcd.print("AUTO");
  }
  else
  {
    lcd.print("MANUAL");
  }
}


// ============================================================
// SEND GOOGLE SHEETS
// ============================================================

bool sendGoogleSheets()
{
  // ----------------------------------------------------------
  // WIFI CHECK
  // ----------------------------------------------------------

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Google Sheets skipped: WiFi disconnected");
    return false;
  }


  // ----------------------------------------------------------
  // HEADER
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("==============================");
  Serial.println("GOOGLE SHEETS");
  Serial.println("==============================");

  Serial.println("WiFi: CONNECTED");

  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  Serial.print("Free Heap Before HTTPS: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");


  // ----------------------------------------------------------
  // BUILD URL
  // ----------------------------------------------------------

  String url;

  url.reserve(320);

  url = GOOGLE_SCRIPT_URL;

  url += "?device=NADI-ESP8266";

  url += "&raw=";
  url += String(rawValue);

  url += "&level=";
  url += String(levelValue);

  url += "&status=";
  url += statusText();

  url += "&relay=";

  if (relayState)
  {
    url += "ON";
  }
  else
  {
    url += "OFF";
  }

  url += "&mode=";

  if (autoMode)
  {
    url += "AUTO";
  }
  else
  {
    url += "MANUAL";
  }


  Serial.println("Sending URL:");
  Serial.println(url);

  Serial.println("Opening HTTPS connection...");
  Serial.println("Sending GET...");


  // ----------------------------------------------------------
  // HTTPS CLIENT
  // ----------------------------------------------------------

  BearSSL::WiFiClientSecure client;

  /*
     Google HTTPS certificate tidak diverifikasi.
     Lebih ringan dan cocok untuk testing ESP8266.
  */

  client.setInsecure();

  /*
     Timeout pendek agar ESP8266 tidak terlalu lama
     tertahan pada koneksi HTTPS.
  */

  client.setTimeout(5000);

  /*
     Buffer kecil untuk mengurangi penggunaan RAM.
  */

  client.setBufferSizes(512, 512);


  // ----------------------------------------------------------
  // HTTP CLIENT
  // ----------------------------------------------------------

  HTTPClient https;

  https.setTimeout(7000);

  /*
     IMPORTANT:

     ESP8266 Core 3.1.2 tidak menerima:

       https.setFollowRedirects(false);

     karena fungsi tersebut membutuhkan enum.

     Kita gunakan:

       HTTPC_DISABLE_FOLLOW_REDIRECTS

     Google Apps Script mengembalikan HTTP 302.

     Dari pengujian Anda:
       HTTP Code: 302

     dan data SUDAH masuk Google Sheets.

     Jadi redirect TIDAK perlu diikuti.
  */

  https.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);


  // ----------------------------------------------------------
  // BEGIN HTTPS
  // ----------------------------------------------------------

  if (!https.begin(client, url))
  {
    Serial.println("HTTPS begin FAILED");

    Serial.print("Free Heap After HTTPS: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");

    https.end();
    client.stop();

    return false;
  }


  // ----------------------------------------------------------
  // GET
  // ----------------------------------------------------------

  int code = https.GET();


  Serial.print("HTTP Code: ");
  Serial.println(code);


  bool success = false;


  // ----------------------------------------------------------
  // RESPONSE
  // ----------------------------------------------------------

  if (code == HTTP_CODE_OK)
  {
    success = true;

    Serial.println("GOOGLE SHEETS: SUCCESS (200)");
  }
  else if (code == HTTP_CODE_MOVED_PERMANENTLY)
  {
    /*
       HTTP 301.

       Request sudah diterima tetapi server meminta
       redirect permanen.
    */

    success = true;

    Serial.println("GOOGLE SHEETS: REDIRECT (301)");
  }
  else if (code == HTTP_CODE_FOUND)
  {
    /*
       HTTP 302.

       Ini adalah response normal dari Google Apps Script.

       Berdasarkan pengujian Anda, data sudah masuk
       ke Google Sheets walaupun response 302.

       Jadi dianggap SUCCESS.
    */

    success = true;

    Serial.println("GOOGLE SHEETS: SUCCESS (302)");
    Serial.println("Redirect received - data accepted by Google.");
  }
  else
  {
    success = false;

    Serial.print("HTTP Error: ");
    Serial.println(code);
  }


  // ----------------------------------------------------------
  // CLEANUP
  // ----------------------------------------------------------

  https.end();

  client.stop();

  yield();


  // ----------------------------------------------------------
  // HEAP
  // ----------------------------------------------------------

  Serial.print("Free Heap After HTTPS: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  Serial.println("==============================");


  return success;
}


// ============================================================
// WEB PAGE HEADER
// Stored in FLASH
// ============================================================

const char PAGE_HEAD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>

<head>

<meta name="viewport"
content="width=device-width,initial-scale=1">

<title>NADI Lighting Monitor</title>

<style>

body{
font-family:Arial,sans-serif;
margin:0;
background:#f2f2f2;
color:#222;
}

nav{
background:#222;
color:white;
padding:14px;
font-size:18px;
font-weight:bold;
}

.navbar{
display:flex;
gap:10px;
margin-top:8px;
font-size:14px;
}

.navbar a{
color:white;
text-decoration:none;
}

main{
max-width:700px;
margin:auto;
padding:15px;
}

.card{
background:white;
padding:15px;
margin:10px 0;
border-radius:10px;
box-shadow:0 2px 5px rgba(0,0,0,.08);
}

h2{
margin-top:0;
}

.value{
font-size:28px;
font-weight:bold;
}

button{
padding:10px 16px;
margin:4px;
border:none;
border-radius:6px;
cursor:pointer;
font-size:14px;
}

</style>

</head>

<body>

<nav>

NADI LIGHTING MONITOR

<div class="navbar">

<a href="/">HOME</a>

<a href="/auto">AUTO</a>

<a href="/manual">MANUAL</a>

</div>

</nav>

<main>
)rawliteral";


// ============================================================
// WEB PAGE FOOTER
// ============================================================

const char PAGE_TAIL[] PROGMEM = R"rawliteral(

<div class="card">

<a href="/">Refresh</a>

</div>

</main>

</body>

</html>

)rawliteral";


// ============================================================
// WEB ROOT
// ============================================================

void handleRoot()
{
  server.sendContent_P(PAGE_HEAD);


  String page;

  page.reserve(1200);


  // ==========================================================
  // MONITORING CARD
  // ==========================================================

  page += "<div class='card'>";

  page += "<h2>Monitoring</h2>";

  page += "<p>Raw Value: <b>";
  page += String(rawValue);
  page += "</b></p>";

  page += "<p>Level: <span class='value'>";
  page += String(levelValue);
  page += "%</span></p>";

  page += "<p>Status: <b>";
  page += statusText();
  page += "</b></p>";

  page += "<p>Relay: <b>";

  if (relayState)
  {
    page += "ON";
  }
  else
  {
    page += "OFF";
  }

  page += "</b></p>";

  page += "<p>Mode: <b>";

  if (autoMode)
  {
    page += "AUTO";
  }
  else
  {
    page += "MANUAL";
  }

  page += "</b></p>";

  page += "<p>WiFi: <b>CONNECTED</b></p>";

  page += "<p>RSSI: <b>";
  page += String(WiFi.RSSI());
  page += " dBm</b></p>";

  page += "<p>Free Heap: <b>";
  page += String(ESP.getFreeHeap());
  page += " bytes</b></p>";

  page += "</div>";


  // ==========================================================
  // CONTROL CARD
  // ==========================================================

  page += "<div class='card'>";

  page += "<h2>Control</h2>";

  page += "<a href='/relay/on'>";
  page += "<button>RELAY ON</button>";
  page += "</a>";

  page += "<a href='/relay/off'>";
  page += "<button>RELAY OFF</button>";
  page += "</a>";

  page += "<br>";

  page += "<a href='/auto'>";
  page += "<button>AUTO MODE</button>";
  page += "</a>";

  page += "<a href='/manual'>";
  page += "<button>MANUAL MODE</button>";
  page += "</a>";

  page += "</div>";


  // ==========================================================
  // SEND PAGE
  // ==========================================================

  server.sendContent(page);

  page = String();

  server.sendContent_P(PAGE_TAIL);
}


// ============================================================
// REDIRECT HOME
// ============================================================

void redirectHome()
{
  server.sendHeader("Location", "/");

  server.send(302, "text/plain", "");
}


// ============================================================
// RELAY ON
// ============================================================

void handleRelayOn()
{
  autoMode = false;

  relayState = true;

  applyRelay();

  updateLCD();

  redirectHome();
}


// ============================================================
// RELAY OFF
// ============================================================

void handleRelayOff()
{
  autoMode = false;

  relayState = false;

  applyRelay();

  updateLCD();

  redirectHome();
}


// ============================================================
// AUTO MODE
// ============================================================

void handleAuto()
{
  autoMode = true;

  updateAuto();

  updateLCD();

  redirectHome();
}


// ============================================================
// MANUAL MODE
// ============================================================

void handleManual()
{
  autoMode = false;

  updateLCD();

  redirectHome();
}


// ============================================================
// WIFI CONNECT
// ============================================================

bool connectWiFi()
{
  Serial.println();
  Serial.println("==============================");
  Serial.println("WIFI");
  Serial.println("==============================");

  Serial.print("Connecting to: ");
  Serial.println(WIFI_SSID);


  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);


  unsigned long startTime = millis();


  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);

    Serial.print(".");

    yield();


    /*
       Timeout 20 detik.
    */

    if (millis() - startTime >= 20000UL)
    {
      Serial.println();

      Serial.println("WiFi connection TIMEOUT");

      return false;
    }
  }


  Serial.println();

  Serial.println("WiFi CONNECTED");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());

  Serial.println("==============================");


  return true;
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(300);


  // ==========================================================
  // RELAY
  // ==========================================================

  pinMode(RELAY_PIN, OUTPUT);

  relayState = false;

  applyRelay();


  // ==========================================================
  // I2C
  // ==========================================================

  Wire.begin(LCD_SDA, LCD_SCL);


  // ==========================================================
  // LCD
  // ==========================================================

  lcd.init();

  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("NADI SYSTEM");

  lcd.setCursor(0, 1);

  lcd.print("Starting...");


  // ==========================================================
  // SERIAL
  // ==========================================================

  Serial.println();

  Serial.println("==============================");

  Serial.println("NADI ESP8266 SYSTEM");

  Serial.println("==============================");


  // ==========================================================
  // WIFI
  // ==========================================================

  if (!connectWiFi())
  {
    Serial.println("WiFi FAILED");

    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("WiFi FAILED");

    lcd.setCursor(0, 1);

    lcd.print("Check WiFi");
  }
  else
  {
    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("WiFi CONNECTED");

    delay(1000);
  }


  // ==========================================================
  // WEB SERVER ROUTES
  // ==========================================================

  server.on("/", handleRoot);

  server.on("/relay/on", handleRelayOn);

  server.on("/relay/off", handleRelayOff);

  server.on("/auto", handleAuto);

  server.on("/manual", handleManual);


  // ==========================================================
  // START SERVER
  // ==========================================================

  server.begin();

  Serial.println("Web Server Started");


  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("Open: http://");

    Serial.println(WiFi.localIP());
  }


  // ==========================================================
  // INITIAL SENSOR
  // ==========================================================

  readSensor();

  updateAuto();

  updateLCD();


  // ==========================================================
  // GOOGLE TIMER
  // ==========================================================

  /*
     Kirim Google Sheets segera setelah startup.

     Tidak menggunakan retry loop.
  */

  lastGoogle = millis() - GOOGLE_INTERVAL;


  Serial.println();

  Serial.println("System READY");

  Serial.println("==============================");
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  // ==========================================================
  // WEB SERVER
  // ==========================================================

  server.handleClient();

  yield();


  unsigned long now = millis();


  // ==========================================================
  // SENSOR
  // ==========================================================

  if (now - lastSensor >= SENSOR_INTERVAL)
  {
    lastSensor = now;

    readSensor();

    updateAuto();
  }


  // ==========================================================
  // LCD
  // ==========================================================

  if (now - lastLCD >= LCD_INTERVAL)
  {
    lastLCD = now;

    updateLCD();
  }


  // ==========================================================
  // GOOGLE SHEETS
  // ==========================================================

  if (now - lastGoogle >= GOOGLE_INTERVAL)
  {
    lastGoogle = now;

    /*
       Hanya satu HTTPS request.

       Tidak ada:
       - retry loop
       - reconnect HTTPS berkali-kali
       - delay panjang
       - response body besar
    */

    sendGoogleSheets();
  }


  // ==========================================================
  // WIFI RECOVERY
  // ==========================================================

  if (now - lastWiFiCheck >= WIFI_CHECK_INTERVAL)
  {
    lastWiFiCheck = now;


    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println();

      Serial.println("WiFi disconnected.");

      Serial.println("Attempting reconnect...");


      WiFi.disconnect();

      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }


  yield();
}