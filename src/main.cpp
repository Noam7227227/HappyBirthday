/*
 * ESP32 Hebrew Birthday Message Display - Premium Gift Edition 🎁
 *
 * Hardware Wiring:
 * -------------------------------------------------------------
 * Component          ESP32 Pin       Description
 * -------------------------------------------------------------
 * Push Button        GPIO 25         Toggle page / Hold 3s for Melody (Terminal
 * 2 to GND) Passive Buzzer     GPIO 26         Plays Happy Birthday Melody
 * (Positive to GPIO 26, Negative to GND) SSD1306 OLED SDA   GPIO 27         I2C
 * Data Line SSD1306 OLED SCL   GPIO 33         I2C Clock Line VCC 3.3V / 5V
 * Power Supply GND                GND             Common Ground
 * -------------------------------------------------------------
 *
 * Features:
 * - Hold Push Button for 3 seconds to play the Happy Birthday Song Melody!
 * - Short press toggles to the next Hebrew blessing
 * - Festive Opening Splash Animation with Birthday Cake & Sparkles
 * - Smooth Vertical Scrolling Engine for long Hebrew blessings
 * - Auto-Sleep Mode (Desk Companion): Dim & Sleep after 3 min of inactivity
 */

#include <Arduino.h>
#include <LittleFS.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <algorithm>
#include <vector>

// --- Pin Definitions ---
#define BUTTON_PIN 25
#define BUZZER_PIN 26 // Passive Buzzer connected to GPIO 26
#define OLED_SDA_PIN 27
#define OLED_SCL_PIN 33

// --- Scroll Speed & Timing Configuration ---
#define SCROLL_SPEED 0.9f    // Scrolling speed in pixels/frame
#define PAUSE_TOP_MS 2000    // Pause at top of blessing (ms)
#define PAUSE_BOTTOM_MS 2800 // Pause at bottom of blessing (ms)
#define DEBOUNCE_DELAY_MS 50 // Button debounce (ms)
#define LONG_PRESS_MS 3000   // Hold button for 3 seconds to trigger melody
#define AUTO_SLEEP_TIMEOUT_MS                                                  \
  (3 * 60 * 1000) // 3 minutes of inactivity -> Sleep Mode

// --- Musical Notes for Happy Birthday Song ---
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784
#define NOTE_C6 1047

// --- U8g2 OLED Initialization (Hardware I2C) ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// --- Dynamic Blessings List ---
std::vector<String> birthdayMessages;

// --- Navigation & State Variables ---
int currentPage = 0;
unsigned long lastActivityTime = 0;
unsigned long buttonPressStartTime = 0;
bool lastButtonState = HIGH;
bool isButtonPressed = false;
bool longPressTriggered = false;
bool isSleeping = false;
bool isStartupSplash = true;

// --- Smooth Vertical Scrolling State ---
float scrollY = 0.0f;
enum ScrollState { PAUSE_TOP, SCROLLING_DOWN, PAUSE_BOTTOM, RESETTING };
ScrollState scrollState = PAUSE_TOP;
unsigned long scrollTimer = 0;

/**
 * @brief Resets the vertical scroll animation to top
 */
void resetScrollPosition() {
  scrollY = 0.0f;
  scrollState = PAUSE_TOP;
  scrollTimer = millis();
}

/**
 * @brief Reverses UTF-8 Hebrew text so it renders correctly Right-to-Left (RTL)
 */
String fixHebrewRTL(const String &input) {
  std::vector<String> glyphs;
  int len = input.length();
  int i = 0;

  while (i < len) {
    unsigned char c = (unsigned char)input[i];
    String glyph = "";

    if ((c & 0x80) == 0) {
      glyph += input[i];
      i += 1;
    } else if ((c & 0xE0) == 0xC0) {
      if (i + 1 < len) {
        glyph += input[i];
        glyph += input[i + 1];
      }
      i += 2;
    } else if ((c & 0xF0) == 0xE0) {
      if (i + 2 < len) {
        glyph += input[i];
        glyph += input[i + 1];
        glyph += input[i + 2];
      }
      i += 3;
    } else if ((c & 0xF8) == 0xF0) {
      if (i + 3 < len) {
        glyph += input[i];
        glyph += input[i + 1];
        glyph += input[i + 2];
        glyph += input[i + 3];
      }
      i += 4;
    } else {
      i++;
    }

    if (glyph.length() > 0) {
      glyphs.push_back(glyph);
    }
  }

  String reversedResult = "";
  for (int j = (int)glyphs.size() - 1; j >= 0; j--) {
    reversedResult += glyphs[j];
  }

  return reversedResult;
}

/**
 * @brief Renders Musical Screen while melody is playing
 */
void renderMusicScreen() {
  u8g2.clearBuffer();
  u8g2.drawRFrame(0, 0, 128, 64, 4);

  u8g2.setFont(u8g2_font_6x10_tf);
  const char *musicTitle = "* MUSIC MODE *";
  int musicTitleWidth = u8g2.getStrWidth(musicTitle);
  u8g2.drawStr((128 - musicTitleWidth) / 2, 16, musicTitle);

  u8g2.setFont(u8g2_font_cu12_t_hebrew);
  String musicText = fixHebrewRTL("יום הולדת שמח! 🎂");
  int w = u8g2.getUTF8Width(musicText.c_str());
  u8g2.drawUTF8((128 - w) / 2, 42, musicText.c_str());

  u8g2.sendBuffer();
}

/**
 * @brief Plays the Happy Birthday Melody on Buzzer (GPIO 26)
 */
void playHappyBirthdayMelody() {
#ifdef BUZZER_PIN
  renderMusicScreen();

  int melody[] = {NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_C5, NOTE_B4, NOTE_G4,
                  NOTE_G4, NOTE_A4, NOTE_G4, NOTE_D5, NOTE_C5, NOTE_G4, NOTE_G4,
                  NOTE_G5, NOTE_E5, NOTE_C5, NOTE_B4, NOTE_A4, NOTE_F5, NOTE_F5,
                  NOTE_E5, NOTE_C5, NOTE_D5, NOTE_C5};

  int durations[] = {4, 4, 2, 2, 2, 1, 4, 4, 2, 2, 2, 1, 4,
                     4, 2, 2, 2, 2, 1, 4, 4, 2, 2, 2, 1};

  int totalNotes = sizeof(melody) / sizeof(melody[0]);
  for (int i = 0; i < totalNotes; i++) {
    int noteDuration = 1000 / durations[i];
    tone(BUZZER_PIN, melody[i], noteDuration);
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(BUZZER_PIN);
  }
#endif
}

/**
 * @brief Plays a short cheerful click sound on button press
 */
void playClickSound() {
#ifdef BUZZER_PIN
  tone(BUZZER_PIN, NOTE_C5, 30);
  delay(35);
  tone(BUZZER_PIN, NOTE_E5, 45);
  delay(50);
  noTone(BUZZER_PIN);
#endif
}

/**
 * @brief Plays a cheerful wake up chime
 */
void playWakeupChime() {
#ifdef BUZZER_PIN
  tone(BUZZER_PIN, NOTE_E5, 50);
  delay(60);
  tone(BUZZER_PIN, NOTE_G5, 50);
  delay(60);
  tone(BUZZER_PIN, NOTE_C6, 80);
  delay(90);
  noTone(BUZZER_PIN);
#endif
}

/**
 * @brief Plays a short fanfare before the long-press melody
 */
void playLongPressStartChime() {
#ifdef BUZZER_PIN
  tone(BUZZER_PIN, NOTE_C5, 60);
  delay(70);
  tone(BUZZER_PIN, NOTE_E5, 60);
  delay(70);
  tone(BUZZER_PIN, NOTE_G5, 100);
  delay(110);
  noTone(BUZZER_PIN);
#endif
}

/**
 * @brief Renders the festive Birthday Cake Splash Screen
 */
void renderStartupSplash() {
  u8g2.clearBuffer();

  u8g2.drawRFrame(0, 0, 128, 64, 4);

  // Centered Header Title
  u8g2.setFont(u8g2_font_6x10_tf);
  const char *titleText = "* HAPPY BIRTHDAY *";
  int titleWidth = u8g2.getStrWidth(titleText);
  u8g2.drawStr((128 - titleWidth) / 2, 14, titleText);

  u8g2.drawBox(48, 38, 32, 12);
  u8g2.drawRFrame(52, 30, 24, 9, 1);

  u8g2.drawVLine(56, 24, 6);
  u8g2.drawVLine(64, 23, 7);
  u8g2.drawVLine(72, 24, 6);

  if ((millis() / 250) % 2 == 0) {
    u8g2.drawDisc(56, 22, 1);
    u8g2.drawDisc(64, 21, 1);
    u8g2.drawDisc(72, 22, 1);
  } else {
    u8g2.drawPixel(56, 22);
    u8g2.drawDisc(64, 20, 1);
    u8g2.drawPixel(72, 22);
  }

  u8g2.setFont(u8g2_font_cu12_t_hebrew);
  String welcomeText = fixHebrewRTL("לחצי על הכפתור 💖");
  int w = u8g2.getUTF8Width(welcomeText.c_str());
  u8g2.drawUTF8((128 - w) / 2, 60, welcomeText.c_str());

  u8g2.sendBuffer();
}

/**
 * @brief Renders bedtime farewell animation before sleep
 */
void renderBedtimeSleep() {
  u8g2.clearBuffer();
  u8g2.drawRFrame(0, 0, 128, 64, 4);

  u8g2.setFont(u8g2_font_cu12_t_hebrew);
  String sleepText = fixHebrewRTL("לילה טוב");
  int w = u8g2.getUTF8Width(sleepText.c_str());
  u8g2.drawUTF8((128 - w) / 2, 36, sleepText.c_str());

  u8g2.sendBuffer();
  delay(2000);

  u8g2.clearBuffer();
  u8g2.sendBuffer();
  u8g2.setPowerSave(1);
}

/**
 * @brief Loads blessing text files from LittleFS filesystem (/Blessings folder)
 */
void loadBlessingsFromLittleFS() {
  birthdayMessages.clear();

  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Error mounting LittleFS filesystem!");
  } else {
    Serial.println(
        "[LittleFS] LittleFS mounted successfully. Reading /Blessings...");

    struct BlessingItem {
      String filename;
      String content;
    };
    std::vector<BlessingItem> items;

    const char *targetDirs[] = {"/Blessings", "/data/Blessings", "/"};
    for (const char *dirPath : targetDirs) {
      File dir = LittleFS.open(dirPath);
      if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        while (file) {
          String fname = String(file.name());
          if (!file.isDirectory() && fname.endsWith(".txt")) {
            String content = file.readString();
            content.replace("\r", "");
            content.trim();
            if (content.length() > 0) {
              items.push_back({fname, content});
              Serial.printf("[LittleFS] Loaded (%s): %s\n", fname.c_str(),
                            content.c_str());
            }
          }
          file = dir.openNextFile();
        }
        dir.close();
      }
      if (!items.empty())
        break;
    }

    std::sort(items.begin(), items.end(),
              [](const BlessingItem &a, const BlessingItem &b) {
                return a.filename < b.filename;
              });

    for (const auto &item : items) {
      birthdayMessages.push_back(item.content);
    }
  }

  if (birthdayMessages.empty()) {
    birthdayMessages.push_back("יום הולדת שמח");
    birthdayMessages.push_back("מזל טוב עד 120!");
    birthdayMessages.push_back("בריאות, אושר ושמחה!");
    birthdayMessages.push_back("הגשמת כל החלומות!");
  }
}

/**
 * @brief Splits full blessing text into display-wrapped lines
 */
std::vector<String> prepareBlessingLines(const String &fullText) {
  std::vector<String> lines;

  int paragraphStart = 0;
  int newlineIdx = fullText.indexOf('\n');

  std::vector<String> paragraphs;
  while (newlineIdx != -1) {
    String p = fullText.substring(paragraphStart, newlineIdx);
    p.trim();
    paragraphs.push_back(p);
    paragraphStart = newlineIdx + 1;
    newlineIdx = fullText.indexOf('\n', paragraphStart);
  }
  String lastP = fullText.substring(paragraphStart);
  lastP.trim();
  paragraphs.push_back(lastP);

  int maxLineWidth = 118;

  for (const String &para : paragraphs) {
    if (para.length() == 0) {
      lines.push_back("");
      continue;
    }

    std::vector<String> words;
    int wordStart = 0;
    int spaceIdx = para.indexOf(' ');
    while (spaceIdx != -1) {
      String w = para.substring(wordStart, spaceIdx);
      w.trim();
      if (w.length() > 0)
        words.push_back(w);
      wordStart = spaceIdx + 1;
      spaceIdx = para.indexOf(' ', wordStart);
    }
    String lastW = para.substring(wordStart);
    lastW.trim();
    if (lastW.length() > 0)
      words.push_back(lastW);

    String currentLine = "";
    for (const String &word : words) {
      String testLine =
          (currentLine.length() == 0) ? word : currentLine + " " + word;
      String rtlTest = fixHebrewRTL(testLine);
      int width = u8g2.getUTF8Width(rtlTest.c_str());

      if (width <= maxLineWidth) {
        currentLine = testLine;
      } else {
        if (currentLine.length() > 0)
          lines.push_back(currentLine);
        currentLine = word;
      }
    }
    if (currentLine.length() > 0)
      lines.push_back(currentLine);
  }

  return lines;
}

/**
 * @brief Updates vertical scroll animation state
 */
void updateScrollAnimation(int totalContentHeight, int viewportHeight) {
  unsigned long now = millis();
  int maxScroll = totalContentHeight - viewportHeight + 6;
  if (maxScroll < 0)
    maxScroll = 0;

  if (maxScroll == 0) {
    scrollY = 0.0f;
    return;
  }

  switch (scrollState) {
  case PAUSE_TOP:
    scrollY = 0.0f;
    if (now - scrollTimer >= PAUSE_TOP_MS) {
      scrollState = SCROLLING_DOWN;
    }
    break;

  case SCROLLING_DOWN:
    scrollY += SCROLL_SPEED;
    if (scrollY >= maxScroll) {
      scrollY = (float)maxScroll;
      scrollState = PAUSE_BOTTOM;
      scrollTimer = now;
    }
    break;

  case PAUSE_BOTTOM:
    scrollY = (float)maxScroll;
    if (now - scrollTimer >= PAUSE_BOTTOM_MS) {
      scrollState = RESETTING;
      scrollTimer = now;
    }
    break;

  case RESETTING:
    scrollY -= 1.5f;
    if (scrollY <= 0.0f) {
      scrollY = 0.0f;
      scrollState = PAUSE_TOP;
      scrollTimer = now;
    }
    break;
  }
}

void drawFrame(int page, int totalPages) {
  u8g2.drawRFrame(0, 0, 128, 64, 4);
  u8g2.drawRFrame(2, 2, 124, 60, 3);

  u8g2.drawPixel(5, 5);
  u8g2.drawPixel(122, 5);
  u8g2.drawPixel(5, 58);
  u8g2.drawPixel(122, 58);

  if (totalPages > 0) {
    int totalWidth = (totalPages * 6);
    int startX = (128 - totalWidth) / 2;
    for (int i = 0; i < totalPages; i++) {
      if (i == page) {
        u8g2.drawDisc(startX + (i * 6) + 2, 57, 2);
      } else {
        u8g2.drawCircle(startX + (i * 6) + 2, 57, 1);
      }
    }
  }
}

void renderPage(int page) {
  if (birthdayMessages.empty())
    return;
  int totalPages = (int)birthdayMessages.size();
  page = page % totalPages;

  u8g2.clearBuffer();

  drawFrame(page, totalPages);

  u8g2.setFont(u8g2_font_cu12_t_hebrew);
  u8g2.setFontDirection(0);

  std::vector<String> lines = prepareBlessingLines(birthdayMessages[page]);

  int lineHeight = 14;
  int totalContentHeight = (int)lines.size() * lineHeight;
  int viewportHeight = 44;

  updateScrollAnimation(totalContentHeight, viewportHeight);

  int startY = 22 - (int)scrollY;

  if (totalContentHeight <= viewportHeight) {
    startY = 32 - (totalContentHeight / 2) + 10;
  }

  for (size_t i = 0; i < lines.size(); i++) {
    int currentY = startY + ((int)i * lineHeight);

    if (currentY >= 12 && currentY <= 55) {
      if (lines[i].length() > 0) {
        String rtlLine = fixHebrewRTL(lines[i]);
        int lineWidth = u8g2.getUTF8Width(rtlLine.c_str());
        int lineX = (128 - lineWidth) / 2;
        if (lineX < 4)
          lineX = 4;
        u8g2.drawUTF8(lineX, currentY, rtlLine.c_str());
      }
    }
  }

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Hebrew Birthday Display Initializing...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
#ifdef BUZZER_PIN
  pinMode(BUZZER_PIN, OUTPUT);
#endif

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  u8g2.begin();
  u8g2.enableUTF8Print();

  loadBlessingsFromLittleFS();

  resetScrollPosition();
  lastActivityTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- Auto-Sleep Check (3 minutes of inactivity) ---
  if (!isSleeping &&
      (currentMillis - lastActivityTime >= AUTO_SLEEP_TIMEOUT_MS)) {
    Serial.println("Entering Auto-Sleep Mode...");
    renderBedtimeSleep();
    isSleeping = true;
  }

  // --- Push Button Handling (Short Click & 3-Second Hold) ---
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Button Press Down (Active LOW)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    buttonPressStartTime = currentMillis;
    isButtonPressed = true;
    longPressTriggered = false;
  }

  // Button Being Held Down
  if (isButtonPressed && currentButtonState == LOW) {
    if (!longPressTriggered &&
        (currentMillis - buttonPressStartTime >= LONG_PRESS_MS)) {
      longPressTriggered = true;
      lastActivityTime = currentMillis;
      Serial.println(
          "Long Press (3s) Detected! Playing Happy Birthday Melody...");

      if (isSleeping) {
        isSleeping = false;
        u8g2.setPowerSave(0);
      }

      playLongPressStartChime();
      playHappyBirthdayMelody();

      lastActivityTime = millis();
    }
  }

  // Button Release (LOW to HIGH)
  if (lastButtonState == LOW && currentButtonState == HIGH) {
    isButtonPressed = false;

    if (!longPressTriggered &&
        (currentMillis - buttonPressStartTime > DEBOUNCE_DELAY_MS)) {
      lastActivityTime = currentMillis;

      if (isSleeping) {
        isSleeping = false;
        u8g2.setPowerSave(0);
        isStartupSplash = true;
        playWakeupChime();
        Serial.println("Woke up from Sleep!");
      } else if (isStartupSplash) {
        isStartupSplash = false;
        currentPage = 0;
        resetScrollPosition();
        playClickSound();
      } else {
        int totalPages = (int)birthdayMessages.size();
        if (totalPages > 0) {
          currentPage = (currentPage + 1) % totalPages;
          resetScrollPosition();
          playClickSound();
        }
      }
    }
  }

  lastButtonState = currentButtonState;

  // --- Render Current Screen State ---
  if (isSleeping) {
    delay(100);
    return;
  }

  if (isStartupSplash) {
    renderStartupSplash();
  } else {
    renderPage(currentPage);
  }

  delay(25);
}
