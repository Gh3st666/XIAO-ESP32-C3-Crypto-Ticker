/*
  Crypto Ticker
  Hardware : Seeed Studio XIAO ESP32C3 + XIAO Expansion Board (SSD1306 128x64 OLED, user button on D1)
  Libraries: U8g2 (olikraus), ArduinoJson (v7, Benoit Blanchon)
  Data     : CoinGecko free API (no key needed)

  Controls : short press = next coin, long press (>0.8 s) = previous coin
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>

// ---------------- USER SETTINGS ----------------
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const char* CURRENCY = "usd";                 // usd, eur, gbp ...
const char* CURRENCY_SYMBOL = "$";            // shown before the price
const unsigned long FETCH_INTERVAL = 30000;   // ms between API calls (don't go below ~20 s)

// Add / remove / reorder coins here. "id" = CoinGecko ID (see coingecko.com/en/coins/<id>)
struct Coin {
  const char* id;
  const char* symbol;
  float price;
  float change;   // 24h change in %
  bool valid;
};

Coin coins[] = {
  {"bitcoin",      "BTC",  0, 0, false},
  {"ethereum",     "ETH",  0, 0, false},
  {"solana",       "SOL",  0, 0, false},
  {"ripple",       "XRP",  0, 0, false},
  {"binancecoin",  "BNB",  0, 0, false},
  {"dogecoin",     "DOGE", 0, 0, false},
  {"cardano",      "ADA",  0, 0, false},
  {"chainlink",    "LINK", 0, 0, false},
};
// ------------------------------------------------

const int NUM_COINS = sizeof(coins) / sizeof(coins[0]);
const int BUTTON_PIN = D1;                    // user button on the expansion board
const unsigned long LONG_PRESS_MS = 800;
const unsigned long DEBOUNCE_MS = 30;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

int currentCoin = 0;
unsigned long lastFetch = 0;
unsigned long lastDraw = 0;
unsigned long lastSuccess = 0;
bool lastFetchOk = true;
bool needRedraw = true;
int lastCode = 0;   // 200 = OK, -2 = no WiFi, -3 = JSON error, -1/other = connection problem

// ---------- helpers ----------
void showMessage(const char* line1, const char* line2 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 24, line1);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(0, 44, line2);
  u8g2.sendBuffer();
}

void connectWiFi() {
  showMessage("WiFi...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    showMessage("Connected!", WiFi.localIP().toString().c_str());
    delay(800);
  } else {
    showMessage("WiFi failed", "Retrying...");
  }
}

bool fetchPrices() {
  if (WiFi.status() != WL_CONNECTED) { lastCode = -2; return false; }

  String ids;
  for (int i = 0; i < NUM_COINS; i++) {
    if (i) ids += ",";
    ids += coins[i].id;
  }
  String url = "https://api.coingecko.com/api/v3/simple/price?ids=" + ids +
               "&vs_currencies=" + CURRENCY + "&include_24hr_change=true";

  WiFiClientSecure client;
  client.setInsecure();                       // skip certificate check (simple & fine for a price display)
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) return false;
  http.addHeader("accept", "application/json");
  http.addHeader("User-Agent", "XIAO-ESP32C3-CryptoTicker");

  int code = http.GET();
  lastCode = code;
  Serial.printf("HTTP code: %d\n", code);
  if (code != 200) {
    if (code < 0) Serial.printf("Connection error: %s\n", http.errorToString(code).c_str());
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();
  Serial.println(payload);

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    lastCode = -3;
    return false;
  }

  String changeKey = String(CURRENCY) + "_24h_change";
  bool any = false;
  for (int i = 0; i < NUM_COINS; i++) {
    JsonObject o = doc[coins[i].id];
    if (!o.isNull() && !o[CURRENCY].isNull()) {
      coins[i].price = o[CURRENCY].as<float>();
      coins[i].change = o[changeKey].as<float>();
      coins[i].valid = true;
      any = true;
    }
  }
  return any;
}

// Format price: 67432 -> "67,432"   2.35 -> "2.35"   0.00012 -> "0.000120"
void formatPrice(float p, char* out, size_t n) {
  if (p >= 1000) {
    long long v = (long long)(p + 0.5f);
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%lld", v);
    int len = strlen(tmp);
    int o = 0;
    for (int i = 0; i < len && o < (int)n - 2; i++) {
      if (i > 0 && (len - i) % 3 == 0) out[o++] = ',';
      out[o++] = tmp[i];
    }
    out[o] = '\0';
  } else if (p >= 1) {
    snprintf(out, n, "%.2f", p);
  } else if (p >= 0.01) {
    snprintf(out, n, "%.4f", p);
  } else {
    snprintf(out, n, "%.6f", p);
  }
}

void drawScreen() {
  Coin& c = coins[currentCoin];
  u8g2.clearBuffer();

  // Symbol (top-left)
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(0, 14, c.symbol);

  if (!c.valid) {
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.drawStr(0, 40, lastFetchOk ? "Loading..." : "No data");
    if (!lastFetchOk) {
      char e[24];
      snprintf(e, sizeof(e), "Error code: %d", lastCode);
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(0, 54, e);
    }
  } else {
    // 24h change (top-right)
    char chg[16];
    snprintf(chg, sizeof(chg), "%c%.2f%%", c.change >= 0 ? '+' : '-', fabsf(c.change));
    u8g2.setFont(u8g2_font_7x13B_tr);
    int w = u8g2.getStrWidth(chg);
    u8g2.drawStr(128 - w, 13, chg);

    // Price (auto-pick biggest font that fits)
    char price[32];
    char full[40];
    formatPrice(c.price, price, sizeof(price));
    snprintf(full, sizeof(full), "%s%s", CURRENCY_SYMBOL, price);

    const uint8_t* fonts[] = {u8g2_font_logisoso22_tr, u8g2_font_logisoso16_tr, u8g2_font_helvB12_tr};
    const int baselines[] = {48, 46, 44};
    for (int i = 0; i < 3; i++) {
      u8g2.setFont(fonts[i]);
      if (u8g2.getStrWidth(full) <= 126 || i == 2) {
        u8g2.drawStr(0, baselines[i], full);
        break;
      }
    }
  }

  // Page dots (bottom-left)
  for (int i = 0; i < NUM_COINS; i++) {
    int x = i * 8;
    if (i == currentCoin) u8g2.drawBox(x, 58, 5, 5);
    else u8g2.drawFrame(x, 58, 5, 5);
  }

  // Data age / error indicator (bottom-right)
  u8g2.setFont(u8g2_font_5x7_tr);
  char age[16];
  if (lastSuccess == 0) {
    snprintf(age, sizeof(age), "--");
  } else {
    snprintf(age, sizeof(age), "%s%lus", lastFetchOk ? "" : "! ", (millis() - lastSuccess) / 1000);
  }
  u8g2.drawStr(128 - u8g2.getStrWidth(age), 63, age);

  u8g2.sendBuffer();
}

// ---------- button handling ----------
void handleButton() {
  static bool lastReading = HIGH;
  static bool stableState = HIGH;
  static unsigned long lastChange = 0;
  static unsigned long pressStart = 0;

  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) {
    lastChange = millis();
    lastReading = reading;
  }

  if (millis() - lastChange > DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {                 // pressed
      pressStart = millis();
    } else {                                  // released
      unsigned long held = millis() - pressStart;
      if (held >= LONG_PRESS_MS) {
        currentCoin = (currentCoin - 1 + NUM_COINS) % NUM_COINS;   // previous
      } else {
        currentCoin = (currentCoin + 1) % NUM_COINS;               // next
      }
      needRedraw = true;
    }
  }
}

// ---------- main ----------
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  u8g2.begin();
  u8g2.setContrast(200);
  showMessage("Crypto", "Ticker starting...");
  delay(800);

  connectWiFi();
}

void loop() {
  handleButton();

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 10000) {
      lastRetry = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  }

  unsigned long interval = lastFetchOk ? FETCH_INTERVAL : 15000;   // retry faster after a failure
  if (lastFetch == 0 || millis() - lastFetch >= interval) {
    lastFetch = millis();
    lastFetchOk = fetchPrices();
    if (lastFetchOk) lastSuccess = millis();
    needRedraw = true;
  }

  if (needRedraw || millis() - lastDraw >= 1000) {
    lastDraw = millis();
    needRedraw = false;
    drawScreen();
  }
}
