#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// ════════════════════════════════════════════
//   FRANKEN-BOT HARDWARE TEST v1.1
//   Phase 1: Display + Touch
//
//   Pin Map:
//     Display SPI: GPIO 10(CS), 11(MOSI), 12(SCK), 13(MISO)
//     DC: GPIO 14  |  RST: GPIO 9  |  BL: GPIO 3
//     Touch: GPIO 47(CS), 21(IRQ)
//     Touch SPI: T_CLK→12, T_DIN→11, T_DO→13 (direct wired)
//     Power: 3.3V + GND
// ════════════════════════════════════════════

#define BL_PIN 3

TFT_eSPI tft = TFT_eSPI();

bool touchWorking = false;
uint16_t calData[5];
unsigned long lastTouchPrint = 0;

// ── Forward Declarations ──
void testColors();
void testShapes();
void testText();
void testFrankFace();
bool testTouch();
void drawingMode();
void drawTarget(int x, int y, uint16_t color);

// ════════════════════════════════════════════
//   SETUP
// ════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("================================================");
    Serial.println("  FRANKEN-BOT HARDWARE TEST v1.1");
    Serial.println("  Phase 1: Display + Touch");
    Serial.println("================================================");
    Serial.println();

    // ── System Info ──
    Serial.printf("[SYS] Chip: %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("[SYS] CPU:  %d MHz\n", ESP.getCpuFreqMHz());
    if (psramFound()) {
        Serial.printf("[SYS] PSRAM: %.1f MB  ✓\n", ESP.getPsramSize() / 1048576.0);
    } else {
        Serial.println("[SYS] PSRAM: NOT FOUND  ✗");
    }
    Serial.printf("[SYS] Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println();

    // ── Pin Report ──
    Serial.println("  PIN CONFIGURATION");
    Serial.println("  ─────────────────────────────────");
    Serial.println("  Display SPI (via SPI3/HSPI):");
    Serial.printf("    CS:   GPIO %d\n", TFT_CS);
    Serial.printf("    MOSI: GPIO %d\n", TFT_MOSI);
    Serial.printf("    SCK:  GPIO %d\n", TFT_SCLK);
    Serial.printf("    MISO: GPIO %d\n", TFT_MISO);
    Serial.println("  Display control:");
    Serial.printf("    DC:   GPIO %d\n", TFT_DC);
    Serial.printf("    RST:  GPIO %d\n", TFT_RST);
    Serial.printf("    BL:   GPIO %d\n", BL_PIN);
    Serial.println("  Touch:");
    Serial.printf("    T_CS: GPIO %d\n", TOUCH_CS);
    Serial.printf("    T_IRQ:GPIO %d\n", TOUCH_IRQ);
    Serial.println("    T_CLK/T_DIN/T_DO: direct wired to SCK/MOSI/MISO");
    Serial.printf("  SPI Clock: %d MHz (display), %.1f MHz (touch)\n",
                  SPI_FREQUENCY / 1000000, SPI_TOUCH_FREQUENCY / 1000000.0);
    Serial.println("  ─────────────────────────────────");
    Serial.println();

    // ── Backlight OFF ──
    pinMode(BL_PIN, OUTPUT);
    digitalWrite(BL_PIN, LOW);
    Serial.println("[BL]  Backlight OFF");

    // ── Initialize Display ──
    Serial.println("[TFT] Initializing ILI9341...");
    tft.init();
    tft.setRotation(3);  // Landscape: 320x240, pins on LEFT
    tft.fillScreen(TFT_BLACK);
    Serial.println("[TFT] Display initialized (320x240 landscape, pins left)");

    // ── T_IRQ pull-up ──
    pinMode(TOUCH_IRQ, INPUT_PULLUP);

    // ── Wake up XPT2046 ──
    // The XPT2046 needs SPI commands to enable PENIRQ.
    // getTouchRaw() bypasses the IRQ check and sends commands directly.
    Serial.println("[XPT] Sending wake-up commands to XPT2046...");
    uint16_t dummyX, dummyY;
    for (int i = 0; i < 10; i++) {
        tft.getTouchRaw(&dummyX, &dummyY);
        delay(10);
    }
    Serial.printf("[XPT] Wake-up complete (last raw: x=%d y=%d)\n", dummyX, dummyY);

    // ── Backlight ON ──
    delay(1000);
    Serial.println("[BL]  Backlight ON");
    digitalWrite(BL_PIN, HIGH);

    tft.fillScreen(TFT_WHITE);
    delay(150);
    tft.fillScreen(TFT_BLACK);
    delay(150);

    // Splash
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("FRANK HW TEST v1.1", 160, 50);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Phase 1: Display + Touch", 160, 80);
    tft.drawString("Watch serial monitor for details", 160, 100);
    delay(2500);

    // ══════════════════════════════════
    //   DISPLAY TESTS
    // ══════════════════════════════════
    Serial.println();
    Serial.println("══════ DISPLAY TESTS ══════");
    Serial.println();

    testColors();
    testShapes();
    testText();
    testFrankFace();

    Serial.println();
    Serial.println("  ┌─────────────────────────┐");
    Serial.println("  │   DISPLAY TEST: PASS ✓   │");
    Serial.println("  └─────────────────────────┘");
    Serial.println();

    // ══════════════════════════════════
    //   TOUCH TESTS
    // ══════════════════════════════════
    Serial.println("══════ TOUCH TESTS ══════");
    Serial.println();

    touchWorking = testTouch();

    if (touchWorking) {
        Serial.println();
        Serial.println("  ┌─────────────────────────┐");
        Serial.println("  │   TOUCH TEST: PASS ✓     │");
        Serial.println("  └─────────────────────────┘");
        Serial.println();
        Serial.println("[DRAW] Entering drawing mode");
        Serial.println("[DRAW] Touch to draw, red button to clear");

        tft.fillScreen(TFT_BLACK);
        tft.fillRoundRect(0, 0, 56, 22, 4, TFT_RED);
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(1);
        tft.drawString("CLEAR", 28, 11);
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(BR_DATUM);
        tft.drawString("Touch to draw | Coords on serial", 316, 238);
    } else {
        Serial.println();
        Serial.println("  ┌──────────────────────────────────────────┐");
        Serial.println("  │   TOUCH: NO RESPONSE                     │");
        Serial.println("  │                                           │");
        Serial.println("  │   Raw values did not change with touch.   │");
        Serial.println("  │   SPI is not reaching the XPT2046.       │");
        Serial.println("  │                                           │");
        Serial.println("  │   Verify wiring:                         │");
        Serial.println("  │     T_CLK → GPIO 12 row (SCK)           │");
        Serial.println("  │     T_DIN → GPIO 11 row (MOSI)          │");
        Serial.println("  │     T_DO  → GPIO 13 row (MISO)          │");
        Serial.println("  │     T_CS  → GPIO 47                     │");
        Serial.println("  │     T_IRQ → GPIO 21                     │");
        Serial.println("  └──────────────────────────────────────────┘");
    }
}

// ════════════════════════════════════════════
//   LOOP
// ════════════════════════════════════════════
void loop() {
    if (touchWorking) {
        drawingMode();
    }
    delay(5);
}

// ════════════════════════════════════════════
//   TEST 1: COLOR FILLS
// ════════════════════════════════════════════
void testColors() {
    Serial.println("[TEST 1] Color Fill");

    struct { uint16_t color; const char* name; } tests[] = {
        {TFT_RED,     "RED"},     {TFT_GREEN,   "GREEN"},
        {TFT_BLUE,    "BLUE"},    {TFT_WHITE,   "WHITE"},
        {TFT_BLACK,   "BLACK"},   {TFT_YELLOW,  "YELLOW"},
        {TFT_CYAN,    "CYAN"},    {TFT_MAGENTA, "MAGENTA"}
    };

    for (auto& t : tests) {
        unsigned long start = millis();
        tft.fillScreen(t.color);
        unsigned long ms = millis() - start;

        bool lightBg = (t.color == TFT_WHITE || t.color == TFT_YELLOW ||
                        t.color == TFT_CYAN  || t.color == TFT_GREEN);
        tft.setTextColor(lightBg ? TFT_BLACK : TFT_WHITE);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(3);
        tft.drawString(t.name, 160, 120);

        Serial.printf("  %-8s  %3lu ms  ✓\n", t.name, ms);
        delay(400);
    }
    Serial.println("[TEST 1] PASS ✓\n");
}

// ════════════════════════════════════════════
//   TEST 2: SHAPES
// ════════════════════════════════════════════
void testShapes() {
    Serial.println("[TEST 2] Shapes");
    tft.fillScreen(TFT_BLACK);

    tft.fillRect(10, 10, 80, 50, TFT_RED);
    tft.drawRect(10, 10, 80, 50, TFT_WHITE);
    Serial.println("  Rectangles  ✓");

    tft.fillCircle(160, 35, 25, TFT_GREEN);
    tft.drawCircle(160, 35, 25, TFT_WHITE);
    Serial.println("  Circles     ✓");

    tft.fillTriangle(240, 10, 300, 60, 210, 60, TFT_BLUE);
    tft.drawTriangle(240, 10, 300, 60, 210, 60, TFT_WHITE);
    Serial.println("  Triangles   ✓");

    for (int i = 0; i < 320; i += 4) {
        uint8_t r = (i * 255) / 320;
        tft.drawLine(i, 75, 320 - i, 175, tft.color565(r, 0, 255 - r));
    }
    Serial.println("  Gradients   ✓");

    tft.fillRoundRect(40, 185, 240, 45, 10, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("ALL SHAPES OK", 160, 207);

    Serial.println("[TEST 2] PASS ✓\n");
    delay(2000);
}

// ════════════════════════════════════════════
//   TEST 3: TEXT
// ════════════════════════════════════════════
void testText() {
    Serial.println("[TEST 3] Text Rendering");
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);

    int y = 5;
    uint16_t colors[] = {TFT_CYAN, TFT_GREEN, TFT_YELLOW, TFT_MAGENTA};
    for (int sz = 1; sz <= 4; sz++) {
        tft.setTextSize(sz);
        tft.setTextColor(colors[sz - 1], TFT_BLACK);
        char buf[32];
        snprintf(buf, sizeof(buf), "Size %d: Frank!", sz);
        tft.drawString(buf, 10, y);
        Serial.printf("  Size %d  ✓\n", sz);
        y += (sz * 10) + 8;
    }

    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("SPI3/HSPI (GPIO 10-13) @ 40MHz", 10, 195);
    tft.drawString("Touch: CS=47  IRQ=21 @ 2.5MHz", 10, 210);
    tft.drawString("Backlight: GPIO 3 | RST: GPIO 9", 10, 225);

    Serial.println("[TEST 3] PASS ✓\n");
    delay(2500);
}

// ════════════════════════════════════════════
//   TEST 4: FRANK'S FACE
// ════════════════════════════════════════════
void testFrankFace() {
    Serial.println("[TEST 4] Frank's Face");
    tft.fillScreen(TFT_BLACK);

    int w = 320, h = 240;
    int eyeR  = w / 10;
    int eyeY  = h / 3;
    int lEyeX = w / 4;
    int rEyeX = (w * 3) / 4;
    int mouthY = (h * 2) / 3;

    tft.fillCircle(lEyeX, eyeY, eyeR, TFT_GREEN);
    tft.fillCircle(rEyeX, eyeY, eyeR, TFT_GREEN);
    Serial.println("  Eyes      ✓");

    tft.fillCircle(w / 2, mouthY, 30, TFT_GREEN);
    tft.fillCircle(w / 2, mouthY - 5, 30, TFT_BLACK);
    Serial.println("  Mouth     ✓");

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.drawString("DISPLAY PASS", w / 2, h - 25);

    Serial.println("[TEST 4] PASS ✓");
    delay(3000);
}

// ════════════════════════════════════════════
//   TEST 5+6: TOUCH DIAGNOSTIC & CALIBRATION
//
//   Uses getTouchRaw() throughout — this
//   bypasses the IRQ check and talks directly
//   to the XPT2046 over SPI.
// ════════════════════════════════════════════
bool testTouch() {
    Serial.println("[TEST 5] Touch Diagnostic");
    Serial.println();

    // ── STEP 1: Baseline (don't touch) ──
    Serial.println("  Step 1: Reading baseline (DON'T touch screen)...");

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("TOUCH DIAGNOSTIC", 160, 20);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Step 1: DON'T touch the screen", 160, 50);
    tft.drawString("Reading baseline...", 160, 70);
    delay(2000);

    uint16_t baseX[5], baseY[5];
    for (int i = 0; i < 5; i++) {
        tft.getTouchRaw(&baseX[i], &baseY[i]);
        Serial.printf("    Baseline %d: rawX=%-5d rawY=%-5d\n",
            i + 1, baseX[i], baseY[i]);
        delay(100);
    }

    int irqNow = digitalRead(TOUCH_IRQ);
    Serial.printf("  T_IRQ: %s\n", irqNow ? "HIGH (correct)" : "LOW");
    Serial.println();

    // ── STEP 2: Touch detection ──
    Serial.println("  Step 2: PRESS AND HOLD the screen now...");
    Serial.println("  Watching for T_IRQ to go LOW (15 seconds)");
    Serial.println();

    tft.fillRect(0, 45, 320, 40, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Step 2: PRESS AND HOLD the screen!", 160, 50);
    tft.drawString("Waiting for IRQ signal (15 sec)...", 160, 70);

    // Labels
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("IRQ:", 10, 100);
    tft.drawString("Raw X:", 10, 118);
    tft.drawString("Raw Y:", 10, 136);
    tft.drawString("Status:", 10, 164);

    bool touchDetected = false;
    int touchCount = 0;
    unsigned long startTime = millis();
    unsigned long lastUpdate = 0;

    while (millis() - startTime < 15000) {
        uint16_t rawX = 0, rawY = 0;
        tft.getTouchRaw(&rawX, &rawY);
        int irq = digitalRead(TOUCH_IRQ);

        // FIX: Use IRQ as the real touch indicator.
        // Raw values have noise from breadboard coupling.
        // IRQ is hardware-driven by XPT2046's pen-detect — no false positives.
        bool realTouch = (irq == LOW);

        if (millis() - lastUpdate > 150) {
            lastUpdate = millis();
            char buf[48];

            tft.fillRect(60, 100, 250, 12, TFT_BLACK);
            tft.setTextDatum(TL_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(realTouch ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
            tft.drawString(realTouch ? "LOW  >>> TOUCH! <<<" : "HIGH (waiting...)", 60, 100);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.fillRect(60, 118, 250, 12, TFT_BLACK);
            snprintf(buf, sizeof(buf), "%d", rawX);
            tft.drawString(buf, 60, 118);

            tft.fillRect(60, 136, 250, 12, TFT_BLACK);
            snprintf(buf, sizeof(buf), "%d", rawY);
            tft.drawString(buf, 60, 136);

            tft.fillRect(60, 164, 250, 12, TFT_BLACK);
            if (realTouch) {
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.drawString(">>> XPT2046 RESPONDING! <<<", 60, 164);
            } else {
                tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
                tft.drawString("Waiting for finger...", 60, 164);
            }

            Serial.printf("  IRQ=%s  rawX=%-5d  rawY=%-5d  %s\n",
                irq ? "HIGH" : "LOW ",
                rawX, rawY,
                realTouch ? "<<< REAL TOUCH" : "");
        }

        if (realTouch) {
            touchDetected = true;
            touchCount++;
        }

        if (touchCount >= 15) break;
        delay(10);
    }

    Serial.println();

    if (!touchDetected) {
        Serial.println("  ✗ T_IRQ never went LOW.");
        Serial.println("    XPT2046 pen-detect not triggering.");
        return false;
    }

    Serial.printf("  ✓ Touch detected via IRQ! (%d events)\n", touchCount);
    Serial.println();

    // ── STEP 3: Calibration ──
    Serial.println("[TEST 6] Touch Calibration");
    Serial.println("  Touch each arrow marker PRECISELY when it appears...");
    Serial.println("  (Take your time — accuracy matters here)");

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("CALIBRATION", 160, 90);
    tft.drawString("Touch each corner arrow PRECISELY", 160, 110);
    tft.drawString("when it appears (4 corners)", 160, 125);
    delay(3000);

    tft.calibrateTouch(calData, TFT_GREEN, TFT_BLACK, 15);
    tft.setTouch(calData);

    Serial.println("  Calibration complete!");
    Serial.printf("  Data: {%d, %d, %d, %d, %d}\n",
        calData[0], calData[1], calData[2], calData[3], calData[4]);
    Serial.println("  ^^^ Save these for FrankOS (avoids recalibrating every boot)");

    // ── STEP 4: Accuracy verify ──
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("CALIBRATED!", 160, 20);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    char calStr[64];
    snprintf(calStr, sizeof(calStr), "{%d, %d, %d, %d, %d}",
        calData[0], calData[1], calData[2], calData[3], calData[4]);
    tft.drawString(calStr, 160, 48);

    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Touch each + to check accuracy", 160, 75);
    tft.drawString("Dots should land ON the crosshairs", 160, 90);
    tft.drawString("Touch center to continue", 160, 105);

    // Targets at known positions
    drawTarget(40, 40, TFT_YELLOW);
    drawTarget(280, 40, TFT_YELLOW);
    drawTarget(40, 200, TFT_YELLOW);
    drawTarget(280, 200, TFT_YELLOW);
    drawTarget(160, 120, TFT_CYAN);  // Center — touch to continue

    unsigned long verifyStart = millis();
    while (millis() - verifyStart < 20000) {
        uint16_t tx, ty;
        if (tft.getTouch(&tx, &ty, 300)) {
            tft.fillCircle(tx, ty, 4, TFT_WHITE);
            Serial.printf("  Verify: x=%d y=%d\n", tx, ty);

            // Center touch = continue
            if (tx > 120 && tx < 200 && ty > 80 && ty < 160) {
                delay(300);
                break;
            }
        }
        delay(10);
    }

    return true;
}

// ════════════════════════════════════════════
//   DRAWING MODE
// ════════════════════════════════════════════
void drawingMode() {
    uint16_t x, y;
    if (tft.getTouch(&x, &y, 300)) {
        if (x < 60 && y < 25) {
            tft.fillScreen(TFT_BLACK);
            tft.fillRoundRect(0, 0, 56, 22, 4, TFT_RED);
            tft.setTextColor(TFT_WHITE, TFT_RED);
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.drawString("CLEAR", 28, 11);
            tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
            tft.setTextDatum(BR_DATUM);
            tft.drawString("Touch to draw | Coords on serial", 316, 238);
            delay(300);
            return;
        }

        tft.fillCircle(x, y, 3, TFT_WHITE);

        if (millis() - lastTouchPrint > 50) {
            Serial.printf("[DRAW] x=%-3d y=%-3d\n", x, y);
            lastTouchPrint = millis();
        }
    }
}

// ════════════════════════════════════════════
//   HELPER: Crosshair target
// ════════════════════════════════════════════
void drawTarget(int x, int y, uint16_t color) {
    tft.drawLine(x - 10, y, x + 10, y, color);
    tft.drawLine(x, y - 10, x, y + 10, color);
    tft.drawCircle(x, y, 8, color);
}
