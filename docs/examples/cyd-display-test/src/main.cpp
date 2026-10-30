#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Touch controller: its own SPI bus (VSPI), separate from the display (HSPI)
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

void setup() {
  Serial.begin(115200);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);  // the CYD examples use this together with the calibration below

  tft.init();
  tft.setRotation(0);  // portrait, 240x320
  tft.fillScreen(TFT_BLACK);

  // colour bars: if red/green/blue are not what the label says, the driver or colour order is wrong
  const uint16_t cols[6] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA};
  const char *names[6] = {"RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA"};
  for (int i = 0; i < 6; i++) {
    tft.fillRect(0, i * 40, 240, 40, cols[i]);
    tft.setTextColor(TFT_BLACK, cols[i]);
    tft.drawString(names[i], 8, i * 40 + 12, 4);
  }
  tft.fillRect(0, 240, 240, 80, TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("Touch the screen", 8, 256, 4);
}

void loop() {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    // raw -> landscape pixels (CYD calibration), then rotate into portrait
    int xl = map(p.x, 200, 3700, 0, 320);
    int yl = map(p.y, 240, 3800, 0, 240);
    int x = constrain(239 - yl, 0, 239);
    int y = constrain(xl, 0, 319);
    tft.fillCircle(x, y, 3, TFT_BLACK);
    Serial.printf("raw=(%d,%d) -> screen=(%d,%d)\n", p.x, p.y, x, y);
  }
  delay(10);
}
