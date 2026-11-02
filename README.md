# XIAO ESP32-C3 Crypto Ticker

A tiny live cryptocurrency price ticker for the **Seeed Studio XIAO ESP32-C3** and the **XIAO Expansion Board**. Prices show up on the built-in OLED screen, and you switch between coins with the on-board user button. No API key needed.

<p align="center">
  <img src="images/demo.jpg" alt="XIAO ESP32-C3 crypto ticker showing the BTC price in a 3D printed case" width="400">
</p>

## Features

- Live prices for BTC, ETH, SOL, XRP, BNB, DOGE, ADA and LINK (easy to change)
- 24h price change in %
- Switch coins with the user button: short press = next, long press = previous
- Price font auto-resizes so big and tiny prices both fit on screen
- Page dots show which coin you're viewing, plus a "last updated" timer
- Auto WiFi reconnect and fast retry after a failed update
- Error code shown on screen for easy troubleshooting
- All coins fetched in a single API call (friendly to free rate limits)

## Hardware

- [Seeed Studio XIAO ESP32-C3](https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html)
- [Seeed Studio XIAO Expansion Board](https://www.seeedstudio.com/Seeeduino-XIAO-Expansion-board-p-4746.html) (0.96" SSD1306 128x64 OLED + user button on D1)
- USB-C cable
- A 2.4 GHz WiFi network (the ESP32-C3 does not support 5 GHz)
- Optional: 3D printed case (see below)

No wiring needed: just plug the XIAO into the expansion board.

## 3D printed case (optional)

A two-part case (top frame with cutouts for the screen, button and XIAO, plus a bottom cover) fits the expansion board. The model is included in this repo as a 3MF file:

[`case/XIAO_Expansion_Board_Case.3mf`](case/XIAO_Expansion_Board_Case.3mf)

<p align="center">
  <img src="images/case-render.png" alt="Render of the 3D printed case: top frame, bottom cover and button cap" width="500">
</p>

Open it in your slicer (PrusaSlicer, Bambu Studio, Cura, etc.) and print. I printed mine in black.

> **Credit:** the case model is not my own design. Original author and license: _add link and name here_.

## Software requirements

- Arduino IDE (or compatible)
- **esp32 by Espressif** board package, with board set to `XIAO_ESP32C3`
- Libraries (install via Library Manager):
  - [U8g2](https://github.com/olikraus/u8g2)
  - [ArduinoJson](https://arduinojson.org/) v7

## Setup

1. Clone or download this repository.
2. Open `crypto_ticker/crypto_ticker.ino` in Arduino IDE.
3. Enter your WiFi details at the top of the sketch:
   ```cpp
   const char* WIFI_SSID = "YOUR_WIFI_NAME";
   const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
   ```
4. Select **XIAO_ESP32C3** as the board and the correct port.
5. Upload.

## Usage

| Action | Result |
|---|---|
| Short press the user button | Next coin |
| Long press (about 0.8 s) | Previous coin |

## Customization

**Change coins.** Edit the `coins[]` list. The first value is the [CoinGecko](https://www.coingecko.com/) coin ID (the last part of the coin's page URL), the second is the symbol shown on screen:

```cpp
{"avalanche-2", "AVAX", 0, 0, false},
```

**Change currency.**

```cpp
const char* CURRENCY = "eur";
const char* CURRENCY_SYMBOL = "€";
```

**Change update interval.**

```cpp
const unsigned long FETCH_INTERVAL = 30000; // ms
```

Keep this at 20 seconds or more to stay inside CoinGecko's free-tier limits. The page-dot indicator fits about 15 coins.

## Troubleshooting

If the screen says **No data**, check the error code shown under it (also printed to the Serial Monitor at 115200 baud):

| Code | Meaning | Fix |
|---|---|---|
| -2 | Not connected to WiFi | Check SSID/password, make sure it's a 2.4 GHz network |
| -1, -11, other negatives | HTTPS connection failed | Weak signal, blocked network or DNS issue; try a phone hotspot |
| 429 | Rate limited | Increase `FETCH_INTERVAL` |
| 403 / 1020 | Request blocked | Try a different network (some VPNs/IPs are blocked) |
| -3 | JSON parse error | Check the raw response in the Serial Monitor |

If the button behaves oddly, the sketch expects it to read LOW when pressed (internal pull-up on D1). If the screen stays blank, check that the OLED responds at I2C address `0x3C`.

## How it works

The ESP32-C3 connects to WiFi and calls the CoinGecko `simple/price` endpoint every 30 seconds with all configured coin IDs. The JSON response is parsed with ArduinoJson and cached in memory, so switching coins with the button is instant. The OLED is driven with U8g2 over I2C.

## Notes

- HTTPS certificate validation is disabled (`setInsecure()`) to keep the code simple. This is fine for a public price display, but don't reuse this approach for anything handling private data.
- Price data comes from [CoinGecko](https://www.coingecko.com/). This project is not affiliated with CoinGecko or Seeed Studio, and displayed prices are for information only, not financial advice.

## Ideas for improvement

- Auto-rotate through coins when idle
- Buzzer alert when a coin moves more than X%
- Mini price chart using stored history
- WiFi setup portal instead of hardcoded credentials

## License

MIT. Do whatever you like, attribution appreciated.
