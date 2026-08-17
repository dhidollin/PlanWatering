/*
================================================================
 PLANT WATERING - SMART PLANT MONITORING
 Version 2.0

 ESP8266 NodeMCU 1.0 ESP-12E
 ADS1115 16-bit 4 Channel ADC
 LCD 16x2 I2C
 Google Sheets
 Professional Web GUI
 Inline SVG Icons
 Dark / Light Mode

================================================================
 SENSOR MAPPING
================================================================

 ADS1115 A0 = Soil Moisture
 ADS1115 A1 = Tank Water Level
 ADS1115 A2 = pH Sensor Po
 ADS1115 A3 = Analog Light Sensor

 ESP8266:
 D1 = I2C SCL
 D2 = I2C SDA
 D5 = Relay Pump

================================================================
 FEATURES
================================================================

 MONITORING
 - Light / Lux
 - Soil Moisture %
 - Tank Level %
 - pH

 CONTROL
 - AUTO
 - MANUAL
 - Pump ON / OFF
 - Tank Low Safety Interlock

 LCD
 - Startup
 - WiFi
 - IP
 - Lux
 - Soil
 - Tank
 - pH
 - Pump
 - Mode

 WEB GUI
 - Dashboard
 - Hardware
 - Network
 - WiFi Scan
 - WiFi Connect
 - Dark / Light mode
 - Inline SVG
 - Responsive desktop/mobile

 GOOGLE SHEETS
 - HTTPS
 - BearSSL
 - Redirect handling
 - Retry
 - Timeout
 - Heap monitoring

================================================================
 REQUIRED LIBRARIES
================================================================

 ESP8266 board package
 LiquidCrystal_I2C
 Adafruit ADS1X15

================================================================
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_ADS1X15.h>


// ================================================================
// DEVICE
// ================================================================

const char DEVICE_NAME[] = "NADI-PLANT-WATERING";

const char FIRMWARE_VERSION[] = "2.0.0";


// ================================================================
// DEFAULT WIFI
// ================================================================

const char DEFAULT_WIFI_SSID[] = "Cactusrift";
const char DEFAULT_WIFI_PASSWORD[] = "Siniguabisikin";


// ================================================================
// GOOGLE APPS SCRIPT
// ================================================================

const char GOOGLE_SCRIPT_URL[] =
"https://script.google.com/macros/s/AKfycbzWjUf1vesVyh3Ig4kP_9EKRxjtJ93vD96XzzE46meItaI3MCXixouHxQpvXyFhACuT/exec";


// ================================================================
// PIN
// ================================================================

#define I2C_SDA D2
#define I2C_SCL D1

#define RELAY_PIN D5

#define RELAY_ON  LOW
#define RELAY_OFF HIGH


// ================================================================
// I2C
// ================================================================

#define LCD_ADDRESS 0x27
#define ADS_ADDRESS 0x48


// ================================================================
// OBJECT
// ================================================================

LiquidCrystal_I2C lcd(
  LCD_ADDRESS,
  16,
  2
);

Adafruit_ADS1115 ads;

ESP8266WebServer server(80);


// ================================================================
// ADS1115 CHANNEL
// ================================================================

#define ADS_SOIL   0
#define ADS_TANK   1
#define ADS_PH     2
#define ADS_LIGHT  3


// ================================================================
// SENSOR CALIBRATION
// ================================================================

// ---------------- SOIL ----------------

int SOIL_DRY_RAW = 665;
int SOIL_WET_RAW = 365;


// ---------------- TANK ----------------

int TANK_EMPTY_RAW = 300;
int TANK_FULL_RAW  = 700;


// ---------------- PH ----------------
//
// MUST be calibrated later using buffer solution.
//
// Default:
// pH 7 = 2.50V
// pH 4 = 3.00V

float PH7_VOLTAGE = 2.50;
float PH4_VOLTAGE = 3.00;


// ---------------- LIGHT ----------------
//
// Temporary generic conversion.
//
// Actual Lux must be calibrated according
// to the exact light sensor module.

float LIGHT_LUX_MAX = 100000.0;


// ================================================================
// CONTROL SETTINGS
// ================================================================

int SOIL_PUMP_ON_THRESHOLD  = 35;
int SOIL_PUMP_OFF_THRESHOLD = 50;

int TANK_LOW_THRESHOLD = 15;


// ================================================================
// SENSOR VARIABLES
// ================================================================

int16_t soilRaw  = 0;
int16_t tankRaw  = 0;
int16_t phRaw    = 0;
int16_t lightRaw = 0;

int soilPercent = 0;
int tankPercent = 0;

float phVoltage = 0.0;
float phValue = 7.0;

float lightLux = 0.0;


// ================================================================
// CONTROL VARIABLES
// ================================================================

bool relayState = false;
bool autoMode = true;


// ================================================================
// HARDWARE STATUS
// ================================================================

bool adsOK = false;
bool lcdOK = false;


// ================================================================
// WIFI
// ================================================================

String currentSSID = DEFAULT_WIFI_SSID;
String currentPassword = DEFAULT_WIFI_PASSWORD;

bool wifiConnecting = false;

unsigned long wifiConnectStart = 0;

const unsigned long WIFI_CONNECT_TIMEOUT = 20000UL;


// ================================================================
// GOOGLE STATUS
// ================================================================

bool googleLastSuccess = false;

int googleLastCode = 0;

unsigned long googleLastAttempt = 0;

unsigned long googleLastSuccessMillis = 0;

uint8_t googleRetryCount = 0;

const uint8_t GOOGLE_MAX_RETRY = 2;


// ================================================================
// TIMERS
// ================================================================

unsigned long lastSensorMillis = 0;
unsigned long lastLCDMillis = 0;
unsigned long lastGoogleMillis = 0;
unsigned long lastWiFiMillis = 0;

const unsigned long SENSOR_INTERVAL = 1000UL;

const unsigned long LCD_INTERVAL = 2500UL;

const unsigned long GOOGLE_INTERVAL = 60000UL;

const unsigned long WIFI_CHECK_INTERVAL = 30000UL;


// ================================================================
// LCD
// ================================================================

uint8_t lcdPage = 0;


// ================================================================
// SYSTEM
// ================================================================

unsigned long systemStartMillis = 0;


// ================================================================
// SVG ICONS
// ================================================================

String svgIcon(
  const String &type,
  const String &size = "28"
)
{
  String s;

  s.reserve(500);

  s += "<svg width='";
  s += size;
  s += "' height='";
  s += size;
  s += "' viewBox='0 0 24 24' fill='none' "
       "stroke='currentColor' stroke-width='1.8' "
       "stroke-linecap='round' stroke-linejoin='round'>";

  if (type == "sun")
  {
    s +=
    "<circle cx='12' cy='12' r='4'/>"
    "<path d='M12 2v2M12 20v2M4.93 4.93l1.42 1.42"
    "M17.65 17.65l1.42 1.42M2 12h2M20 12h2"
    "M4.93 19.07l1.42-1.42M17.65 6.35l1.42-1.42'/>";
  }

  else if (type == "plant")
  {
    s +=
    "<path d='M12 21V10'/>"
    "<path d='M12 13C7 13 4 10 4 5c5 0 8 3 8 8z'/>"
    "<path d='M12 16c5 0 8-3 8-8-5 0-8 3-8 8z'/>";
  }

  else if (type == "tank")
  {
    s +=
    "<path d='M6 3h12v18H6z'/>"
    "<path d='M8 8h8v11H8z'/>"
    "<path d='M8 15c2 1 4 1 8 0'/>";
  }

  else if (type == "ph")
  {
    s +=
    "<path d='M9 3h6'/>"
    "<path d='M10 3v5l-4 8a5 5 0 004 5h4a5 5 0 004-5l-4-8V3'/>"
    "<path d='M8 15h8'/>";
  }

  else if (type == "pump")
  {
    s +=
    "<circle cx='12' cy='12' r='8'/>"
    "<path d='M12 7v10M7 12h10'/>"
    "<path d='M4 5l3 2M20 5l-3 2'/>";
  }

  else if (type == "wifi")
  {
    s +=
    "<path d='M5 12.55a11 11 0 0114.08 0'/>"
    "<path d='M8.5 16.05a6 6 0 017 0'/>"
    "<path d='M12 19h.01'/>";
  }

  else if (type == "gear")
  {
    s +=
    "<circle cx='12' cy='12' r='3'/>"
    "<path d='M19.4 15a1.7 1.7 0 000-6l-1.2-.7"
    "a1.7 1.7 0 00-2.4-2.4L15 4.6a1.7 1.7 0 00-6 0"
    "l-.7 1.2a1.7 1.7 0 00-2.4 2.4L4.6 9"
    "a1.7 1.7 0 000 6l1.3.7a1.7 1.7 0 002.4 2.4"
    "l.7 1.3a1.7 1.7 0 006 0l.7-1.3a1.7 1.7 0 002.4-2.4z'/>";
  }

  else
  {
    s +=
    "<circle cx='12' cy='12' r='8'/>";
  }

  s += "</svg>";

  return s;
}


// ================================================================
// URL ENCODE
// ================================================================

String urlEncode(
  const String &input
)
{
  String encoded;

  encoded.reserve(
    input.length() * 2
  );

  const char hex[] = "0123456789ABCDEF";

  for (
    size_t i = 0;
    i < input.length();
    i++
  )
  {
    char c = input[i];

    if (
      (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') ||
      c == '-' ||
      c == '_' ||
      c == '.' ||
      c == '~'
    )
    {
      encoded += c;
    }
    else
    {
      encoded += '%';

      encoded +=
        hex[
          (c >> 4) & 0x0F
        ];

      encoded +=
        hex[
          c & 0x0F
        ];
    }
  }

  return encoded;
}


// ================================================================
// PH CALCULATION
// ================================================================

float calculatePH(
  float voltage
)
{
  if (
    PH4_VOLTAGE ==
    PH7_VOLTAGE
  )
  {
    return 7.0;
  }

  float slope =
    (4.0 - 7.0) /
    (PH4_VOLTAGE - PH7_VOLTAGE);

  float ph =
    7.0 +
    (
      (voltage - PH7_VOLTAGE)
      * slope
    );

  return constrain(
    ph,
    0.0,
    14.0
  );
}


// ================================================================
// LIGHT CALCULATION
// ================================================================

float calculateLux(
  int raw
)
{
  if (raw <= 0)
    return 0;

  if (raw >= 32767)
    return LIGHT_LUX_MAX;

  return
    (
      (float)raw /
      32767.0
    )
    *
    LIGHT_LUX_MAX;
}


// ================================================================
// READ SENSOR
// ================================================================

void readSensors()
{
  if (!adsOK)
    return;

  soilRaw =
    ads.readADC_SingleEnded(
      ADS_SOIL
    );

  tankRaw =
    ads.readADC_SingleEnded(
      ADS_TANK
    );

  phRaw =
    ads.readADC_SingleEnded(
      ADS_PH
    );

  lightRaw =
    ads.readADC_SingleEnded(
      ADS_LIGHT
    );


  // ---------------- SOIL ----------------

  soilPercent =
    map(
      soilRaw,
      SOIL_DRY_RAW,
      SOIL_WET_RAW,
      0,
      100
    );

  soilPercent =
    constrain(
      soilPercent,
      0,
      100
    );


  // ---------------- TANK ----------------

  tankPercent =
    map(
      tankRaw,
      TANK_EMPTY_RAW,
      TANK_FULL_RAW,
      0,
      100
    );

  tankPercent =
    constrain(
      tankPercent,
      0,
      100
    );


  // ---------------- PH ----------------

  phVoltage =
    ads.computeVolts(
      phRaw
    );

  phValue =
    calculatePH(
      phVoltage
    );


  // ---------------- LIGHT ----------------

  lightLux =
    calculateLux(
      lightRaw
    );
}


// ================================================================
// TANK SAFETY
// ================================================================

bool tankSafetyOK()
{
  return
    tankPercent >
    TANK_LOW_THRESHOLD;
}


// ================================================================
// APPLY RELAY
// ================================================================

void applyRelay()
{
  digitalWrite(
    RELAY_PIN,
    relayState
    ? RELAY_ON
    : RELAY_OFF
  );
}


// ================================================================
// AUTO CONTROL
// ================================================================

void updateAutoControl()
{
  if (!autoMode)
    return;


  // Tank safety has highest priority

  if (!tankSafetyOK())
  {
    relayState = false;

    applyRelay();

    return;
  }


  // Soil hysteresis

  if (
    soilPercent <=
    SOIL_PUMP_ON_THRESHOLD
  )
  {
    relayState = true;
  }

  else if (
    soilPercent >=
    SOIL_PUMP_OFF_THRESHOLD
  )
  {
    relayState = false;
  }


  applyRelay();
}


// ================================================================
// SAFETY
// ================================================================

void enforceSafety()
{
  if (
    relayState &&
    !tankSafetyOK()
  )
  {
    relayState = false;

    applyRelay();
  }
}


// ================================================================
// TEXT
// ================================================================

const char* modeText()
{
  return
    autoMode
    ? "AUTO"
    : "MANUAL";
}


const char* relayText()
{
  return
    relayState
    ? "ON"
    : "OFF";
}


const char* tankStatus()
{
  if (
    tankPercent <=
    TANK_LOW_THRESHOLD
  )
    return "LOW";

  if (tankPercent < 40)
    return "MEDIUM";

  return "GOOD";
}


const char* soilStatus()
{
  if (
    soilPercent <
    SOIL_PUMP_ON_THRESHOLD
  )
    return "DRY";

  if (
    soilPercent <
    SOIL_PUMP_OFF_THRESHOLD
  )
    return "MOIST";

  return "WET";
}


// ================================================================
// LCD
// ================================================================

void updateLCD()
{
  if (!lcdOK)
    return;

  lcd.clear();


  if (lcdPage == 0)
  {
    lcd.setCursor(0, 0);

    lcd.print("Lux:");
    lcd.print(
      (long)lightLux
    );

    lcd.setCursor(0, 1);

    lcd.print("Soil:");
    lcd.print(
      soilPercent
    );
    lcd.print("%");
  }


  else if (lcdPage == 1)
  {
    lcd.setCursor(0, 0);

    lcd.print("Tank:");
    lcd.print(
      tankPercent
    );
    lcd.print("%");

    lcd.setCursor(0, 1);

    lcd.print("pH:");
    lcd.print(
      phValue,
      2
    );
  }


  else if (lcdPage == 2)
  {
    lcd.setCursor(0, 0);

    lcd.print("Pump:");
    lcd.print(
      relayText()
    );

    lcd.setCursor(0, 1);

    lcd.print("Mode:");
    lcd.print(
      modeText()
    );
  }


  else
  {
    lcd.setCursor(0, 0);

    if (
      WiFi.status() ==
      WL_CONNECTED
    )
    {
      lcd.print(
        "WiFi Connected"
      );

      lcd.setCursor(0, 1);

      lcd.print(
        WiFi.localIP()
      );
    }
    else
    {
      lcd.print(
        "WiFi Offline"
      );

      lcd.setCursor(0, 1);

      lcd.print(
        "Reconnect..."
      );
    }
  }


  lcdPage++;

  if (lcdPage > 3)
    lcdPage = 0;
}


// ================================================================
// GOOGLE SHEETS
// ================================================================

bool sendGoogleSheets()
{
  if (
    WiFi.status() !=
    WL_CONNECTED
  )
  {
    Serial.println(
      "[GS] WiFi not connected"
    );

    googleLastSuccess = false;

    googleLastCode = -100;

    return false;
  }


  Serial.println();
  Serial.println(
    "========== GOOGLE SHEETS =========="
  );

  Serial.print(
    "Free Heap Before: "
  );

  Serial.println(
    ESP.getFreeHeap()
  );


  // --------------------------------------------------------------
  // BUILD URL
  // --------------------------------------------------------------

  String url;

  url.reserve(1100);

  url += GOOGLE_SCRIPT_URL;

  url += "?device=";
  url += urlEncode(
    DEVICE_NAME
  );

  url += "&version=";
  url += urlEncode(
    FIRMWARE_VERSION
  );

  url += "&soilRaw=";
  url += String(
    soilRaw
  );

  url += "&soil=";
  url += String(
    soilPercent
  );

  url += "&tankRaw=";
  url += String(
    tankRaw
  );

  url += "&tank=";
  url += String(
    tankPercent
  );

  url += "&phRaw=";
  url += String(
    phRaw
  );

  url += "&phVoltage=";
  url += String(
    phVoltage,
    3
  );

  url += "&ph=";
  url += String(
    phValue,
    2
  );

  url += "&lightRaw=";
  url += String(
    lightRaw
  );

  url += "&lux=";
  url += String(
    lightLux,
    1
  );

  url += "&relay=";
  url += urlEncode(
    relayText()
  );

  url += "&mode=";
  url += urlEncode(
    modeText()
  );

  url += "&tankStatus=";
  url += urlEncode(
    tankStatus()
  );

  url += "&soilStatus=";
  url += urlEncode(
    soilStatus()
  );

  url += "&rssi=";
  url += String(
    WiFi.RSSI()
  );


  // --------------------------------------------------------------
  // RETRY
  // --------------------------------------------------------------

  for (
    uint8_t attempt = 0;
    attempt <= GOOGLE_MAX_RETRY;
    attempt++
  )
  {
    Serial.print(
      "[GS] Attempt "
    );

    Serial.print(
      attempt + 1
    );

    Serial.println(
      "/3"
    );


    BearSSL::WiFiClientSecure client;

    client.setInsecure();

    client.setTimeout(
      12000
    );

    client.setBufferSizes(
      512,
      512
    );


    HTTPClient https;

    https.setTimeout(
      12000
    );


    // IMPORTANT:
    // Follow Google Apps Script redirects.

    https.setFollowRedirects(
      HTTPC_STRICT_FOLLOW_REDIRECTS
    );

    https.setRedirectLimit(
      5
    );


    // HTTP/1.0 can be more stable for
    // small ESP8266 HTTPS requests.

    https.useHTTP10(
      true
    );


    if (
      !https.begin(
        client,
        url
      )
    )
    {
      Serial.println(
        "[GS] HTTPS begin FAILED"
      );

      googleLastCode = -101;

      https.end();

      client.stop();

      yield();

      continue;
    }


    https.addHeader(
      "User-Agent",
      "ESP8266-Plant-Watering"
    );

    https.addHeader(
      "Connection",
      "close"
    );


    int code =
      https.GET();


    googleLastCode =
      code;


    Serial.print(
      "[GS] HTTP Code: "
    );

    Serial.println(
      code
    );


    // ------------------------------------------------------------
    // VALID HTTP RESPONSE
    // ------------------------------------------------------------

    if (
      code >= 200 &&
      code < 400
    )
    {
      googleLastSuccess =
        true;

      googleLastSuccessMillis =
        millis();

      googleRetryCount =
        attempt;


      Serial.println(
        "[GS] SUCCESS"
      );

      Serial.print(
        "[GS] Free Heap After: "
      );

      Serial.println(
        ESP.getFreeHeap()
      );

      https.end();

      client.stop();

      yield();

      Serial.println(
        "==================================="
      );

      return true;
    }


    // ------------------------------------------------------------
    // ERROR
    // ------------------------------------------------------------

    Serial.print(
      "[GS] FAILED: "
    );

    Serial.println(
      https.errorToString(
        code
      )
    );


    https.end();

    client.stop();

    yield();


    // Small non-blocking-friendly pause
    // between retry attempts.

    if (
      attempt <
      GOOGLE_MAX_RETRY
    )
    {
      unsigned long retryStart =
        millis();

      while (
        millis() -
        retryStart <
        500
      )
      {
        server.handleClient();

        yield();
      }
    }
  }


  googleLastSuccess =
    false;


  Serial.print(
    "[GS] FINAL FAILED. Code: "
  );

  Serial.println(
    googleLastCode
  );

  Serial.print(
    "[GS] Free Heap After: "
  );

  Serial.println(
    ESP.getFreeHeap()
  );

  Serial.println(
    "==================================="
  );


  return false;
}


// ================================================================
// WIFI CONNECT
// ================================================================

bool connectWiFi(
  const char* ssid,
  const char* password
)
{
  Serial.println();
  Serial.println(
    "========== WIFI CONNECT =========="
  );

  WiFi.mode(
    WIFI_STA
  );

  WiFi.persistent(
    false
  );

  WiFi.setAutoReconnect(
    true
  );

  WiFi.disconnect();

  delay(100);

  WiFi.begin(
    ssid,
    password
  );


  unsigned long start =
    millis();


  while (
    WiFi.status() !=
    WL_CONNECTED &&
    millis() -
    start <
    WIFI_CONNECT_TIMEOUT
  )
  {
    delay(200);

    Serial.print(
      "."
    );

    yield();
  }


  Serial.println();


  if (
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    currentSSID =
      ssid;

    currentPassword =
      password;


    Serial.println(
      "WiFi CONNECTED"
    );

    Serial.print(
      "SSID: "
    );

    Serial.println(
      WiFi.SSID()
    );

    Serial.print(
      "IP: "
    );

    Serial.println(
      WiFi.localIP()
    );

    Serial.print(
      "RSSI: "
    );

    Serial.println(
      WiFi.RSSI()
    );


    return true;
  }


  Serial.println(
    "WiFi CONNECTION FAILED"
  );


  return false;
}


// ================================================================
// WIFI SCAN
// ================================================================

String wifiScanHTML()
{
  String html;

  html.reserve(
    4500
  );


  int count =
    WiFi.scanNetworks(
      false,
      true
    );


  if (count <= 0)
  {
    html +=
      "<div class='empty'>"
      "No WiFi networks found."
      "</div>";

    return html;
  }


  for (
    int i = 0;
    i < count;
    i++
  )
  {
    html +=
      "<div class='wifi-row'>";


    html +=
      "<div class='wifi-name'>";

    html +=
      svgIcon(
        "wifi",
        "20"
      );

    html +=
      "<span>";

    html +=
      WiFi.SSID(i);

    html +=
      "</span>";

    html +=
      "</div>";


    html +=
      "<div class='wifi-signal'>";

    html +=
      String(
        WiFi.RSSI(i)
      );

    html +=
      " dBm";


    if (
      WiFi.encryptionType(i)
      !=
      ENC_TYPE_NONE
    )
    {
      html +=
        " 🔒";
    }


    html +=
      "</div>";

    html +=
      "</div>";
  }


  WiFi.scanDelete();


  return html;
}


// ================================================================
// WEB HEADER
// ================================================================

const char WEB_HEAD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta
name="viewport"
content="width=device-width,initial-scale=1">

<title>Plant Watering</title>


<style>

/* ============================================================
   THEME
   ============================================================ */

:root{

--primary:#00979D;
--primary-dark:#00777B;

--blue:#1688E8;
--green:#16A56A;
--orange:#F59E0B;
--red:#DC3545;

--bg:#F4F8FA;
--card:#FFFFFF;

--text:#17324D;
--muted:#718096;

--line:#DDE7EC;

--shadow:
0 8px 25px rgba(24,72,95,.08);

}

body.dark{

--bg:#0E1720;
--card:#17232E;

--text:#E8F1F5;
--muted:#91A4B1;

--line:#2B3B47;

--shadow:
0 8px 25px rgba(0,0,0,.25);

}


/* ============================================================
   RESET
   ============================================================ */

*{
box-sizing:border-box;
}

html{
scroll-behavior:smooth;
}

body{

margin:0;

font-family:
Arial,
Helvetica,
sans-serif;

background:
var(--bg);

color:
var(--text);

transition:
background .2s,
color .2s;

}


/* ============================================================
   HEADER
   ============================================================ */

.top{

position:
sticky;

top:0;

z-index:100;

background:
var(--card);

border-bottom:
1px solid var(--line);

box-shadow:
0 3px 15px rgba(0,0,0,.06);

}


.header{

max-width:
1200px;

margin:auto;

padding:
16px 20px 10px;

display:flex;

justify-content:space-between;

align-items:center;

gap:20px;

}


.brand{

display:flex;

align-items:center;

gap:12px;

}


.brand-mark{

width:44px;

height:44px;

border-radius:12px;

background:
linear-gradient(
135deg,
#00979D,
#16A56A
);

color:white;

display:flex;

align-items:center;

justify-content:center;

font-size:23px;

font-weight:bold;

}


.brand-title{

font-size:21px;

font-weight:800;

letter-spacing:.3px;

}


.brand-sub{

font-size:12px;

color:
var(--muted);

margin-top:3px;

}


.header-actions{

display:flex;

align-items:center;

gap:8px;

}


.icon-btn{

width:40px;

height:40px;

border:1px solid var(--line);

background:var(--card);

color:var(--text);

border-radius:11px;

cursor:pointer;

display:flex;

align-items:center;

justify-content:center;

}


.icon-btn:hover{

border-color:
var(--primary);

color:
var(--primary);

}


/* ============================================================
   NAV
   ============================================================ */

.nav{

max-width:
1200px;

margin:auto;

padding:
0 20px 10px;

display:flex;

gap:7px;

overflow-x:auto;

}


.nav a{

text-decoration:none;

color:
var(--muted);

font-size:13px;

font-weight:700;

padding:
9px 14px;

border-radius:9px;

white-space:nowrap;

}


.nav a:hover{

background:
rgba(0,151,157,.08);

color:
var(--primary);

}


/* ============================================================
   CONTAINER
   ============================================================ */

.container{

max-width:
1200px;

margin:auto;

padding:
20px;

}


/* ============================================================
   GRID
   ============================================================ */

.sensor-grid{

display:grid;

grid-template-columns:
repeat(
4,
minmax(0,1fr)
);

gap:14px;

margin-bottom:16px;

}


.main-grid{

display:grid;

grid-template-columns:
260px
minmax(0,1fr)
320px;

gap:14px;

}


.two-grid{

display:grid;

grid-template-columns:
1fr
1fr;

gap:14px;

margin-top:14px;

}


/* ============================================================
   CARD
   ============================================================ */

.card{

background:
var(--card);

border:
1px solid var(--line);

border-radius:16px;

box-shadow:
var(--shadow);

padding:17px;

}


.card-title{

display:flex;

align-items:center;

gap:9px;

font-size:15px;

font-weight:800;

margin-bottom:15px;

}


.card-title svg{

color:
var(--primary);

}


/* ============================================================
   SENSOR
   ============================================================ */

.sensor-card{

min-height:
150px;

}


.sensor-top{

display:flex;

justify-content:space-between;

align-items:center;

}


.sensor-icon{

width:42px;

height:42px;

border-radius:12px;

display:flex;

align-items:center;

justify-content:center;

background:
rgba(0,151,157,.09);

color:
var(--primary);

}


.sensor-name{

font-size:12px;

font-weight:700;

color:
var(--muted);

}


.sensor-value{

font-size:28px;

font-weight:800;

margin-top:12px;

}


.sensor-unit{

font-size:13px;

font-weight:600;

color:
var(--muted);

}


.progress{

height:7px;

background:
var(--line);

border-radius:20px;

overflow:hidden;

margin-top:13px;

}


.progress span{

display:block;

height:100%;

border-radius:20px;

}


.progress.green span{
background:var(--green);
}

.progress.blue span{
background:var(--blue);
}

.progress.orange span{
background:var(--orange);
}

.progress.teal span{
background:var(--primary);
}


/* ============================================================
   STATUS
   ============================================================ */

.status{

font-size:11px;

font-weight:800;

margin-top:9px;

}


.good{
color:var(--green);
}

.warn{
color:var(--orange);
}

.bad{
color:var(--red);
}


/* ============================================================
   CONTROL
   ============================================================ */

.control-button{

width:100%;

padding:12px;

border:1px solid var(--line);

background:
var(--card);

color:
var(--text);

border-radius:10px;

font-weight:800;

cursor:pointer;

margin-bottom:8px;

}


.control-button:hover{

border-color:
var(--primary);

}


.control-button.active{

background:
var(--primary);

color:white;

border-color:
var(--primary);

}


.pump-row{

display:flex;

justify-content:space-between;

align-items:center;

padding:13px;

border:
1px solid var(--line);

border-radius:11px;

margin-top:12px;

}


.switch{

width:46px;

height:25px;

background:#AAB7BF;

border-radius:30px;

position:relative;

}


.switch.on{

background:
var(--green);

}


.switch-dot{

width:19px;

height:19px;

background:white;

border-radius:50%;

position:absolute;

top:3px;

left:3px;

transition:.2s;

}


.switch.on .switch-dot{

left:24px;

}


/* ============================================================
   INFO ROW
   ============================================================ */

.info-row{

display:flex;

justify-content:space-between;

align-items:center;

padding:
10px 0;

border-bottom:
1px solid var(--line);

font-size:13px;

}


.info-row:last-child{
border-bottom:0;
}


.info-label{

color:
var(--muted);

}


.info-value{

font-weight:800;

}


/* ============================================================
   BUTTON
   ============================================================ */

.btn{

border:0;

border-radius:9px;

padding:
10px 14px;

background:
var(--primary);

color:white;

font-weight:800;

cursor:pointer;

}


.btn:hover{

background:
var(--primary-dark);

}


.btn-danger{
background:var(--red);
}


.btn-secondary{

background:
#687784;

}


/* ============================================================
   WIFI
   ============================================================ */

.wifi-row{

display:flex;

justify-content:space-between;

align-items:center;

padding:
12px 0;

border-bottom:
1px solid var(--line);

}


.wifi-name{

display:flex;

align-items:center;

gap:9px;

font-size:13px;

font-weight:700;

}


.wifi-name svg{

color:
var(--primary);

}


.wifi-signal{

font-size:12px;

color:
var(--muted);

}


/* ============================================================
   FORM
   ============================================================ */

input{

width:100%;

padding:11px;

border:
1px solid var(--line);

border-radius:9px;

background:
var(--card);

color:
var(--text);

margin:
6px 0 12px;

outline:none;

}


input:focus{

border-color:
var(--primary);

}


/* ============================================================
   HARDWARE
   ============================================================ */

.hardware-grid{

display:grid;

grid-template-columns:
repeat(
2,
minmax(0,1fr)
);

gap:10px;

}


.hardware-item{

border:
1px solid var(--line);

border-radius:11px;

padding:12px;

display:flex;

align-items:center;

gap:10px;

}


.hardware-icon{

color:
var(--primary);

}


.hardware-name{

font-size:13px;

font-weight:800;

}


.hardware-detail{

font-size:10px;

color:
var(--muted);

margin-top:3px;

}


/* ============================================================
   FOOTER
   ============================================================ */

.footer{

margin-top:20px;

padding:
14px 0;

border-top:
1px solid var(--line);

font-size:11px;

color:
var(--muted);

display:flex;

justify-content:space-between;

}


/* ============================================================
   MOBILE
   ============================================================ */

@media(max-width:950px){

.sensor-grid{

grid-template-columns:
repeat(2,1fr);

}

.main-grid{

grid-template-columns:
1fr
1fr;

}

.main-grid > :first-child{

grid-column:
1 / -1;

}

.two-grid{

grid-template-columns:
1fr;

}

}


@media(max-width:600px){

.header{

padding:
12px 14px 8px;

}

.brand-title{

font-size:17px;

}

.brand-mark{

width:38px;

height:38px;

font-size:19px;

}

.container{

padding:
14px;

}

.sensor-grid{

grid-template-columns:
1fr 1fr;

gap:9px;

}

.sensor-card{

padding:13px;

min-height:135px;

}

.sensor-value{

font-size:22px;

}

.main-grid{

grid-template-columns:
1fr;

}

.main-grid > :first-child{

grid-column:
auto;

}

.hardware-grid{

grid-template-columns:
1fr;

}

.footer{

flex-direction:column;

gap:5px;

}

}


/* ============================================================
   EMPTY
   ============================================================ */

.empty{

padding:
20px;

text-align:center;

color:
var(--muted);

font-size:13px;

}

</style>


<script>

function theme(){

document.body.classList.toggle("dark");

localStorage.setItem(
"plant-theme",
document.body.classList.contains("dark")
? "dark"
: "light"
);

}


function loadTheme(){

if(
localStorage.getItem(
"plant-theme"
)
===
"dark"
){

document.body.classList.add(
"dark"
);

}

}


window.addEventListener(
"load",
loadTheme
);

</script>

</head>


<body>


<div class="top">

<div class="header">

<div class="brand">

<div class="brand-mark">
🌱
</div>

<div>

<div class="brand-title">
PLANT WATERING
</div>

<div class="brand-sub">
Smart Plant Monitoring
</div>

</div>

</div>


<div class="header-actions">

<div class="small-status">

)rawliteral";


// ================================================================
// WEB TAIL
// ================================================================

const char WEB_TAIL[] PROGMEM = R"rawliteral(

</div>

<button
class="icon-btn"
onclick="theme()"
title="Dark / Light">

☀︎

</button>

</div>

</div>


<div class="nav">

<a href="/">
Dashboard
</a>

<a href="/hardware">
Hardware
</a>

<a href="/network">
Network
</a>

</div>

</div>


<div class="container">

)rawliteral";


// ================================================================
// SEND WEB HEADER
// ================================================================

void sendWebHeader()
{
  server.sendContent_P(
    WEB_HEAD
  );
}


// ================================================================
// SEND WEB FOOTER
// ================================================================

void sendWebFooter()
{
  server.sendContent_P(
    WEB_TAIL
  );
}


// ================================================================
// DASHBOARD
// ================================================================

void handleDashboard()
{
  sendWebHeader();


  String status;

  status.reserve(
    800
  );


  if (
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    status +=
      "<span class='good'>● Online</span>";
  }
  else
  {
    status +=
      "<span class='bad'>● Offline</span>";
  }


  server.sendContent(
    status
  );


  String html;

  html.reserve(
    7000
  );


  // ============================================================
  // SENSOR GRID
  // ============================================================

  html +=
    "<div class='sensor-grid'>";


  // LIGHT

  html +=
    "<div class='card sensor-card'>";

  html +=
    "<div class='sensor-top'>";

  html +=
    "<div class='sensor-name'>"
    "LIGHT / LUX"
    "</div>";

  html +=
    "<div class='sensor-icon'>";

  html +=
    svgIcon(
      "sun",
      "25"
    );

  html +=
    "</div>";

  html +=
    "</div>";

  html +=
    "<div class='sensor-value'>";

  html +=
    String(
      lightLux,
      0
    );

  html +=
    " <span class='sensor-unit'>lux</span>";

  html +=
    "</div>";

  html +=
    "<div class='progress orange'>"
    "<span style='width:";

  int lightProgress =
    constrain(
      (int)(
        lightLux /
        100000.0 *
        100
      ),
      0,
      100
    );

  html +=
    String(
      lightProgress
    );

  html +=
    "%'></span></div>";

  html +=
    "</div>";


  // SOIL

  html +=
    "<div class='card sensor-card'>";

  html +=
    "<div class='sensor-top'>";

  html +=
    "<div class='sensor-name'>"
    "SOIL MOISTURE"
    "</div>";

  html +=
    "<div class='sensor-icon'>";

  html +=
    svgIcon(
      "plant",
      "25"
    );

  html +=
    "</div>";

  html +=
    "</div>";

  html +=
    "<div class='sensor-value'>";

  html +=
    String(
      soilPercent
    );

  html +=
    "%</div>";

  html +=
    "<div class='progress green'>"
    "<span style='width:";

  html +=
    String(
      soilPercent
    );

  html +=
    "%'></span></div>";

  html +=
    "<div class='status ";

  html +=
    soilPercent <
    SOIL_PUMP_ON_THRESHOLD
    ? "bad"
    : "good";

  html +=
    "'>";

  html +=
    soilStatus();

  html +=
    "</div>";

  html +=
    "</div>";


  // TANK

  html +=
    "<div class='card sensor-card'>";

  html +=
    "<div class='sensor-top'>";

  html +=
    "<div class='sensor-name'>"
    "TANK LEVEL"
    "</div>";

  html +=
    "<div class='sensor-icon'>";

  html +=
    svgIcon(
      "tank",
      "25"
    );

  html +=
    "</div>";

  html +=
    "</div>";

  html +=
    "<div class='sensor-value'>";

  html +=
    String(
      tankPercent
    );

  html +=
    "%</div>";

  html +=
    "<div class='progress blue'>"
    "<span style='width:";

  html +=
    String(
      tankPercent
    );

  html +=
    "%'></span></div>";

  html +=
    "<div class='status ";

  html +=
    tankSafetyOK()
    ? "good"
    : "bad";

  html +=
    "'>";

  html +=
    tankStatus();

  html +=
    "</div>";

  html +=
    "</div>";


  // PH

  html +=
    "<div class='card sensor-card'>";

  html +=
    "<div class='sensor-top'>";

  html +=
    "<div class='sensor-name'>"
    "pH WATER"
    "</div>";

  html +=
    "<div class='sensor-icon'>";

  html +=
    svgIcon(
      "ph",
      "25"
    );

  html +=
    "</div>";

  html +=
    "</div>";

  html +=
    "<div class='sensor-value'>";

  html +=
    String(
      phValue,
      2
    );

  html +=
    " <span class='sensor-unit'>pH</span>";

  html +=
    "</div>";

  html +=
    "<div class='progress teal'>"
    "<span style='width:";

  int phProgress =
    constrain(
      (int)(
        phValue /
        14.0 *
        100
      ),
      0,
      100
    );

  html +=
    String(
      phProgress
    );

  html +=
    "%'></span></div>";

  html +=
    "<div class='status good'>"
    "Monitoring"
    "</div>";

  html +=
    "</div>";


  html +=
    "</div>";


  // ============================================================
  // MAIN GRID
  // ============================================================

  html +=
    "<div class='main-grid'>";


  // CONTROL

  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>";

  html +=
    svgIcon(
      "gear",
      "20"
    );

  html +=
    "System Control</div>";


  html +=
    "<a href='/mode/auto'>"
    "<button class='control-button ";

  if (autoMode)
    html += "active";

  html +=
    "'>AUTO</button></a>";


  html +=
    "<a href='/mode/manual'>"
    "<button class='control-button ";

  if (!autoMode)
    html += "active";

  html +=
    "'>MANUAL</button></a>";


  html +=
    "<div class='pump-row'>";

  html +=
    "<div>";

  html +=
    "<b>Relay Pump</b>";

  html +=
    "<div class='sensor-name'>";

  html +=
    modeText();

  html +=
    "</div>";

  html +=
    "</div>";


  html +=
    "<div class='switch ";

  if (relayState)
    html += "on";

  html +=
    "'><div class='switch-dot'></div></div>";

  html +=
    "</div>";


  html +=
    "<br>";


  html +=
    "<a href='/pump/on'>"
    "<button class='btn'>PUMP ON</button>"
    "</a>";


  html +=
    "<a href='/pump/off'>"
    "<button class='btn btn-danger'>PUMP OFF</button>"
    "</a>";


  html +=
    "</div>";


  // TANK STATUS

  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>";

  html +=
    svgIcon(
      "tank",
      "20"
    );

  html +=
    "Tank Status</div>";


  html +=
    "<div style='text-align:center;"
    "padding:8px'>";

  html +=
    "<div style='font-size:46px;"
    "font-weight:800;"
    "color:var(--blue)'>";

  html +=
    String(
      tankPercent
    );

  html +=
    "%</div>";

  html +=
    "<div class='sensor-name'>"
    "Water Level"
    "</div>";

  html +=
    "</div>";


  html +=
    "<div class='info-row'>";

  html +=
    "<span class='info-label'>Raw</span>";

  html +=
    "<span class='info-value'>";

  html +=
    String(
      tankRaw
    );

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>";

  html +=
    "<span class='info-label'>Safety</span>";

  html +=
    "<span class='info-value ";

  html +=
    tankSafetyOK()
    ? "good"
    : "bad";

  html +=
    "'>";

  html +=
    tankSafetyOK()
    ? "OK"
    : "PUMP OFF";

  html +=
    "</span></div>";


  html +=
    "</div>";


  // SYSTEM STATUS

  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>";

  html +=
    svgIcon(
      "wifi",
      "20"
    );

  html +=
    "System Status</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>WiFi</span>"
    "<span class='info-value'>";

  html +=
    WiFi.status() ==
    WL_CONNECTED
    ? "Connected"
    : "Offline";

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>IP</span>"
    "<span class='info-value'>";

  html +=
    WiFi.localIP().toString();

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>RSSI</span>"
    "<span class='info-value'>";

  html +=
    String(
      WiFi.RSSI()
    );

  html +=
    " dBm</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Free Heap</span>"
    "<span class='info-value'>";

  html +=
    String(
      ESP.getFreeHeap()
    );

  html +=
    " B</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Google Sheets</span>"
    "<span class='info-value ";

  html +=
    googleLastSuccess
    ? "good"
    : "bad";

  html +=
    "'>";

  html +=
    googleLastSuccess
    ? "OK"
    : "FAILED";

  html +=
    "</span></div>";


  html +=
    "</div>";


  html +=
    "</div>";


  // ============================================================
  // HARDWARE SUMMARY
  // ============================================================

  html +=
    "<div class='two-grid'>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>";

  html +=
    svgIcon(
      "gear",
      "20"
    );

  html +=
    "Hardware</div>";


  html +=
    "<div class='hardware-grid'>";


  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "gear",
      "25"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>ADS1115</div>"
    "<div class='hardware-detail'>I2C ADC 16-bit</div>"
    "</div></div>";


  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "sun",
      "25"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>Light</div>"
    "<div class='hardware-detail'>ADS A3</div>"
    "</div></div>";


  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "plant",
      "25"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>Soil</div>"
    "<div class='hardware-detail'>ADS A0</div>"
    "</div></div>";


  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "ph",
      "25"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>pH</div>"
    "<div class='hardware-detail'>ADS A2 / Po</div>"
    "</div></div>";


  html +=
    "</div>";

  html +=
    "</div>";


  // RAW DATA

  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>"
    "Sensor Raw Data"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Soil</span>"
    "<span class='info-value'>";

  html +=
    String(
      soilRaw
    );

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Tank</span>"
    "<span class='info-value'>";

  html +=
    String(
      tankRaw
    );

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>pH</span>"
    "<span class='info-value'>";

  html +=
    String(
      phRaw
    );

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Light</span>"
    "<span class='info-value'>";

  html +=
    String(
      lightRaw
    );

  html +=
    "</span></div>";


  html +=
    "</div>";


  html +=
    "</div>";


  // ============================================================
  // FOOTER
  // ============================================================

  html +=
    "<div class='footer'>";

  html +=
    "<span>Plant Watering v";

  html +=
    FIRMWARE_VERSION;

  html +=
    " • ESP8266</span>";

  html +=
    "<span>Smart Plant Monitoring</span>";

  html +=
    "</div>";


  server.sendContent(
    html
  );

  sendWebFooter();
}


// ================================================================
// HARDWARE PAGE
// ================================================================

void handleHardware()
{
  sendWebHeader();


  String html;

  html.reserve(
    6000
  );


  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>";

  html +=
    svgIcon(
      "gear",
      "22"
    );

  html +=
    "Hardware Configuration"
    "</div>";


  html +=
    "<div class='hardware-grid'>";


  // ADS

  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "gear",
      "30"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>ADS1115</div>"
    "<div class='hardware-detail'>"
    "Address 0x48 • 16-bit ADC"
    "</div>"
    "</div></div>";


  // LCD

  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "gear",
      "30"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>LCD 16x2</div>"
    "<div class='hardware-detail'>"
    "I2C Address 0x27"
    "</div>"
    "</div></div>";


  // SOIL

  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "plant",
      "30"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>Soil Moisture</div>"
    "<div class='hardware-detail'>"
    "ADS1115 A0 • Analog"
    "</div>"
    "</div></div>";


  // TANK

  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "tank",
      "30"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>Tank Level</div>"
    "<div class='hardware-detail'>"
    "ADS1115 A1 • Analog"
    "</div>"
    "</div></div>";


  // PH

  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "ph",
      "30"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>pH Sensor</div>"
    "<div class='hardware-detail'>"
    "ADS1115 A2 • Po"
    "</div>"
    "</div></div>";


  // LIGHT

  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "sun",
      "30"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>Light Sensor</div>"
    "<div class='hardware-detail'>"
    "ADS1115 A3 • Analog"
    "</div>"
    "</div></div>";


  // RELAY

  html +=
    "<div class='hardware-item'>";

  html +=
    "<div class='hardware-icon'>";

  html +=
    svgIcon(
      "pump",
      "30"
    );

  html +=
    "</div>";

  html +=
    "<div>"
    "<div class='hardware-name'>Relay Pump</div>"
    "<div class='hardware-detail'>"
    "ESP8266 D5"
    "</div>"
    "</div></div>";


  html +=
    "</div>";

  html +=
    "</div>";


  // PIN MAP

  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>"
    "I2C / GPIO Mapping"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>D1</span>"
    "<span class='info-value'>I2C SCL</span>"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>D2</span>"
    "<span class='info-value'>I2C SDA</span>"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>D5</span>"
    "<span class='info-value'>Relay Pump</span>"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>ADS A0</span>"
    "<span class='info-value'>Soil Moisture</span>"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>ADS A1</span>"
    "<span class='info-value'>Tank Level</span>"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>ADS A2</span>"
    "<span class='info-value'>pH Po</span>"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>ADS A3</span>"
    "<span class='info-value'>Light Sensor</span>"
    "</div>";


  html +=
    "</div>";


  // CALIBRATION

  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>"
    "Calibration Values"
    "</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Soil Dry</span>"
    "<span class='info-value'>";

  html +=
    String(
      SOIL_DRY_RAW
    );

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Soil Wet</span>"
    "<span class='info-value'>";

  html +=
    String(
      SOIL_WET_RAW
    );

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Tank Empty</span>"
    "<span class='info-value'>";

  html +=
    String(
      TANK_EMPTY_RAW
    );

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Tank Full</span>"
    "<span class='info-value'>";

  html +=
    String(
      TANK_FULL_RAW
    );

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>pH 7 Voltage</span>"
    "<span class='info-value'>";

  html +=
    String(
      PH7_VOLTAGE,
      3
    );

  html +=
    " V</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>pH 4 Voltage</span>"
    "<span class='info-value'>";

  html +=
    String(
      PH4_VOLTAGE,
      3
    );

  html +=
    " V</span></div>";


  html +=
    "</div>";


  server.sendContent(
    html
  );

  sendWebFooter();
}


// ================================================================
// NETWORK PAGE
// ================================================================

void handleNetwork()
{
  sendWebHeader();


  String html;

  html.reserve(
    6500
  );


  // STATUS

  html +=
    "<div class='two-grid'>";


  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>";

  html +=
    svgIcon(
      "wifi",
      "22"
    );

  html +=
    "Network Status</div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Status</span>"
    "<span class='info-value'>";

  html +=
    WiFi.status() ==
    WL_CONNECTED
    ? "CONNECTED"
    : "OFFLINE";

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>SSID</span>"
    "<span class='info-value'>";

  html +=
    WiFi.SSID();

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>IP Address</span>"
    "<span class='info-value'>";

  html +=
    WiFi.localIP().toString();

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>Gateway</span>"
    "<span class='info-value'>";

  html +=
    WiFi.gatewayIP().toString();

  html +=
    "</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>RSSI</span>"
    "<span class='info-value'>";

  html +=
    String(
      WiFi.RSSI()
    );

  html +=
    " dBm</span></div>";


  html +=
    "<div class='info-row'>"
    "<span class='info-label'>MAC</span>"
    "<span class='info-value'>";

  html +=
    WiFi.macAddress();

  html +=
    "</span></div>";


  html +=
    "</div>";


  // CONNECT

  html +=
    "<div class='card'>";

  html +=
    "<div class='card-title'>"
    "Connect WiFi"
    "</div>";


  html +=
    "<form action='/wifi/connect' method='GET'>";


  html +=
    "<label>SSID</label>";

  html +=
    "<input name='ssid' required>";


  html +=
    "<label>Password</label>";

  html +=
    "<input name='pass' type='password'>";


  html +=
    "<button class='btn' type='submit'>"
    "CONNECT"
    "</button>";


  html +=
    "</form>";


  html +=
    "</div>";


  html +=
    "</div>";


  // SCAN

  html +=
    "<div class='card' style='margin-top:14px'>";


  html +=
    "<div class='card-title'>";

  html +=
    svgIcon(
      "wifi",
      "22"
    );

  html +=
    "WiFi Scan</div>";


  html +=
    "<a href='/network/scan'>"
    "<button class='btn'>"
    "SCAN NETWORKS"
    "</button>"
    "</a>";


  html +=
    "</div>";


  server.sendContent(
    html
  );

  sendWebFooter();
}


// ================================================================
// WIFI SCAN
// ================================================================

void handleWiFiScan()
{
  sendWebHeader();


  String html;

  html.reserve(
    5000
  );


  html +=
    "<div class='card'>";


  html +=
    "<div class='card-title'>";

  html +=
    svgIcon(
      "wifi",
      "22"
    );

  html +=
    "Available WiFi</div>";


  html +=
    wifiScanHTML();


  html +=
    "<br>";

  html +=
    "<a href='/network'>"
    "<button class='btn'>BACK</button>"
    "</a>";


  html +=
    "</div>";


  server.sendContent(
    html
  );

  sendWebFooter();
}


// ================================================================
// REDIRECT
// ================================================================

void redirectHome()
{
  server.sendHeader(
    "Location",
    "/"
  );

  server.send(
    303,
    "text/plain",
    ""
  );
}


// ================================================================
// PUMP ON
// ================================================================

void handlePumpOn()
{
  autoMode = false;


  if (
    tankSafetyOK()
  )
  {
    relayState = true;
  }
  else
  {
    relayState = false;
  }


  applyRelay();

  redirectHome();
}


// ================================================================
// PUMP OFF
// ================================================================

void handlePumpOff()
{
  autoMode = false;

  relayState = false;

  applyRelay();

  redirectHome();
}


// ================================================================
// AUTO
// ================================================================

void handleAuto()
{
  autoMode = true;

  updateAutoControl();

  redirectHome();
}


// ================================================================
// MANUAL
// ================================================================

void handleManual()
{
  autoMode = false;

  enforceSafety();

  redirectHome();
}


// ================================================================
// WIFI CONNECT WEB
// ================================================================

void handleWiFiConnect()
{
  if (
    !server.hasArg(
      "ssid"
    )
  )
  {
    server.send(
      400,
      "text/plain",
      "SSID missing"
    );

    return;
  }


  String ssid =
    server.arg(
      "ssid"
    );

  String pass;


  if (
    server.hasArg(
      "pass"
    )
  )
  {
    pass =
      server.arg(
        "pass"
      );
  }


  server.send(
    200,
    "text/html",
    "<html>"
    "<head>"
    "<meta name='viewport'"
    "content='width=device-width,initial-scale=1'>"
    "</head>"
    "<body style='font-family:Arial;"
    "padding:30px'>"
    "<h2>Connecting WiFi...</h2>"
    "<p>Please wait...</p>"
    "</body>"
    "</html>"
  );


  delay(100);


  connectWiFi(
    ssid.c_str(),
    pass.c_str()
  );
}


// ================================================================
// NOT FOUND
// ================================================================

void handleNotFound()
{
  server.send(
    404,
    "text/plain",
    "404 - Page Not Found"
  );
}


// ================================================================
// LCD STARTING
// ================================================================

void showStarting()
{
  if (!lcdOK)
    return;


  lcd.clear();


  lcd.setCursor(
    0,
    0
  );

  lcd.print(
    "PLANT WATERING"
  );


  lcd.setCursor(
    0,
    1
  );

  lcd.print(
    "Starting..."
  );


  delay(800);
}


// ================================================================
// LCD WIFI
// ================================================================

void showWiFiStatus()
{
  if (!lcdOK)
    return;


  lcd.clear();


  if (
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    lcd.setCursor(
      0,
      0
    );

    lcd.print(
      "WiFi Connected"
    );


    lcd.setCursor(
      0,
      1
    );

    lcd.print(
      WiFi.localIP()
    );


    delay(1500);
  }

  else
  {
    lcd.setCursor(
      0,
      0
    );

    lcd.print(
      "WiFi Failed"
    );


    lcd.setCursor(
      0,
      1
    );

    lcd.print(
      "Check Network"
    );


    delay(1000);
  }
}


// ================================================================
// SETUP
// ================================================================

void setup()
{
  Serial.begin(
    115200
  );


  delay(300);


  systemStartMillis =
    millis();


  // ============================================================
  // RELAY
  // ============================================================

  pinMode(
    RELAY_PIN,
    OUTPUT
  );


  relayState =
    false;


  applyRelay();


  // ============================================================
  // I2C
  // ============================================================

  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );


  // ============================================================
  // LCD
  // ============================================================

  lcd.init();

  lcd.backlight();

  lcdOK = true;


  showStarting();


  // ============================================================
  // ADS1115
  // ============================================================

  Serial.println();

  Serial.println(
    "Initializing ADS1115..."
  );


  ads.setGain(
    GAIN_ONE
  );


  adsOK =
    ads.begin(
      ADS_ADDRESS
    );


  if (adsOK)
  {
    Serial.println(
      "ADS1115: OK"
    );
  }
  else
  {
    Serial.println(
      "ADS1115: ERROR"
    );
  }


  // ============================================================
  // WIFI
  // ============================================================

  connectWiFi(
    DEFAULT_WIFI_SSID,
    DEFAULT_WIFI_PASSWORD
  );


  showWiFiStatus();


  // ============================================================
  // WEB SERVER
  // ============================================================

  server.on(
    "/",
    handleDashboard
  );


  server.on(
    "/hardware",
    handleHardware
  );


  server.on(
    "/network",
    handleNetwork
  );


  server.on(
    "/network/scan",
    handleWiFiScan
  );


  server.on(
    "/wifi/connect",
    handleWiFiConnect
  );


  server.on(
    "/pump/on",
    handlePumpOn
  );


  server.on(
    "/pump/off",
    handlePumpOff
  );


  server.on(
    "/mode/auto",
    handleAuto
  );


  server.on(
    "/mode/manual",
    handleManual
  );


  server.onNotFound(
    handleNotFound
  );


  server.begin();


  Serial.println(
    "Web Server Started"
  );


  // ============================================================
  // INITIAL SENSOR
  // ============================================================

  readSensors();


  updateAutoControl();


  updateLCD();


  // ============================================================
  // GOOGLE FIRST SEND
  // ============================================================

  lastGoogleMillis =
    millis() -
    GOOGLE_INTERVAL;


  // ============================================================
  // READY
  // ============================================================

  Serial.println();

  Serial.println(
    "================================"
  );

  Serial.println(
    "PLANT WATERING READY"
  );

  Serial.println(
    "================================"
  );


  if (
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    Serial.print(
      "Web GUI: http://"
    );

    Serial.println(
      WiFi.localIP()
    );
  }


  Serial.print(
    "Free Heap: "
  );

  Serial.println(
    ESP.getFreeHeap()
  );
}


// ================================================================
// LOOP
// ================================================================

void loop()
{
  // --------------------------------------------------------------
  // WEB SERVER
  // --------------------------------------------------------------

  server.handleClient();

  yield();


  unsigned long now =
    millis();


  // --------------------------------------------------------------
  // SENSOR
  // --------------------------------------------------------------

  if (
    now -
    lastSensorMillis >=
    SENSOR_INTERVAL
  )
  {
    lastSensorMillis =
      now;


    readSensors();


    if (autoMode)
    {
      updateAutoControl();
    }
    else
    {
      enforceSafety();
    }
  }


  // --------------------------------------------------------------
  // LCD
  // --------------------------------------------------------------

  if (
    now -
    lastLCDMillis >=
    LCD_INTERVAL
  )
  {
    lastLCDMillis =
      now;


    updateLCD();
  }


  // --------------------------------------------------------------
  // GOOGLE SHEETS
  // --------------------------------------------------------------

  if (
    now -
    lastGoogleMillis >=
    GOOGLE_INTERVAL
  )
  {
    lastGoogleMillis =
      now;


    sendGoogleSheets();
  }


  // --------------------------------------------------------------
  // WIFI CHECK
  // --------------------------------------------------------------

  if (
    now -
    lastWiFiMillis >=
    WIFI_CHECK_INTERVAL
  )
  {
    lastWiFiMillis =
      now;


    if (
      WiFi.status() !=
      WL_CONNECTED
    )
    {
      Serial.println(
        "[WiFi] Disconnected"
      );


      WiFi.reconnect();
    }
  }


  yield();
}
