/*
 * ESP32 Hebrew Birthday Message Display - Smooth Vertical Scrolling Engine
 * 
 * Hardware Wiring:
 * -------------------------------------------------------------
 * Component          ESP32 Pin       Description
 * -------------------------------------------------------------
 * Push Button        GPIO 25         Toggle page (Terminal 2 to GND)
 * SSD1306 OLED SDA   GPIO 27         I2C Data Line
 * SSD1306 OLED SCL   GPIO 33         I2C Clock Line
 * VCC                3.3V / 5V       Power
 * GND                GND             Common Ground
 * -------------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>

// --- Pin Definitions ---
#define BUTTON_PIN    25
#define OLED_SDA_PIN  27
#define OLED_SCL_PIN  33

// --- Scroll Speed & Timing Configuration (EDIT THESE VALUES TO CHANGE SPEED) ---
#define SCROLL_SPEED     0.8f  // Scrolling speed in pixels/frame (Higher = Faster. e.g. 0.4f=slow, 0.8f=medium, 1.2f=fast)
#define PAUSE_TOP_MS     2200  // Pause duration at the top of a blessing (in milliseconds)
#define PAUSE_BOTTOM_MS  2800  // Pause duration at the bottom of a blessing (in milliseconds)
#define DEBOUNCE_DELAY_MS 200  // 200 ms button debounce

// --- U8g2 OLED Initialization (Hardware I2C) ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- Dynamic Blessings List ---
std::vector<String> birthdayMessages;

// --- Navigation & State Variables ---
int currentPage = 0;
unsigned long lastButtonPressTime = 0;
bool lastButtonState = HIGH;

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
 *        on Left-to-Right (LTR) OLED displays.
 */
String fixHebrewRTL(const String& input) {
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
 * @brief Loads blessing text files from LittleFS filesystem (/Blessings folder)
 */
void loadBlessingsFromLittleFS() {
  birthdayMessages.clear();

  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Error mounting LittleFS filesystem!");
  } else {
    Serial.println("[LittleFS] LittleFS mounted successfully. Reading /Blessings...");

    struct BlessingItem {
      String filename;
      String content;
    };
    std::vector<BlessingItem> items;

    const char* targetDirs[] = {"/Blessings", "/data/Blessings", "/"};
    for (const char* dirPath : targetDirs) {
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
              Serial.printf("[LittleFS] Loaded (%s): %s\n", fname.c_str(), content.c_str());
            }
          }
          file = dir.openNextFile();
        }
        dir.close();
      }
      if (!items.empty()) break;
    }

    // Sort files alphabetically (bless1.txt, bless2.txt, etc.)
    std::sort(items.begin(), items.end(), [](const BlessingItem& a, const BlessingItem& b) {
      return a.filename < b.filename;
    });

    for (const auto& item : items) {
      birthdayMessages.push_back(item.content);
    }
  }

  // Fallback to default blessings if LittleFS is empty
  if (birthdayMessages.empty()) {
    Serial.println("[LittleFS] No files found in LittleFS. Using fallbacks:");
    birthdayMessages.push_back("יום הולדת שמח");
    birthdayMessages.push_back("מזל טוב עד 120!");
    birthdayMessages.push_back("בריאות, אושר ושמחה!");
    birthdayMessages.push_back("הגשמת כל החלומות!");
  }

  Serial.printf("Total blessings loaded: %d\n", (int)birthdayMessages.size());
}

/**
 * @brief Splits a full blessing text (including newlines) into display-wrapped lines
 */
std::vector<String> prepareBlessingLines(const String& fullText) {
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

  for (const String& para : paragraphs) {
    if (para.length() == 0) {
      lines.push_back(""); // Empty line for spacing
      continue;
    }

    std::vector<String> words;
    int wordStart = 0;
    int spaceIdx = para.indexOf(' ');
    while (spaceIdx != -1) {
      String w = para.substring(wordStart, spaceIdx);
      w.trim();
      if (w.length() > 0) words.push_back(w);
      wordStart = spaceIdx + 1;
      spaceIdx = para.indexOf(' ', wordStart);
    }
    String lastW = para.substring(wordStart);
    lastW.trim();
    if (lastW.length() > 0) words.push_back(lastW);

    String currentLine = "";
    for (const String& word : words) {
      String testLine = (currentLine.length() == 0) ? word : currentLine + " " + word;
      String rtlTest = fixHebrewRTL(testLine);
      int width = u8g2.getUTF8Width(rtlTest.c_str());

      if (width <= maxLineWidth) {
        currentLine = testLine;
      } else {
        if (currentLine.length() > 0) lines.push_back(currentLine);
        currentLine = word;
      }
    }
    if (currentLine.length() > 0) lines.push_back(currentLine);
  }

  return lines;
}

/**
 * @brief Updates vertical scroll animation state
 */
void updateScrollAnimation(int totalContentHeight, int viewportHeight) {
  unsigned long now = millis();
  int maxScroll = totalContentHeight - viewportHeight + 6;
  if (maxScroll < 0) maxScroll = 0;

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
      scrollY -= 1.5f; // Faster reset scroll back to top
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

  // Page Indicator Dots at bottom
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
  if (birthdayMessages.empty()) return;
  int totalPages = (int)birthdayMessages.size();
  page = page % totalPages;

  u8g2.clearBuffer();

  drawFrame(page, totalPages);

  u8g2.setFont(u8g2_font_cu12_t_hebrew);
  u8g2.setFontDirection(0);

  // Prepare wrapped lines for the current blessing
  std::vector<String> lines = prepareBlessingLines(birthdayMessages[page]);
  
  int lineHeight = 14;
  int totalContentHeight = (int)lines.size() * lineHeight;
  int viewportHeight = 44; // Available height inside box

  // Update smooth vertical scroll animation position
  updateScrollAnimation(totalContentHeight, viewportHeight);

  // Calculate vertical offset
  int startY = 22 - (int)scrollY;

  // Center vertically if content fits completely on screen without scrolling
  if (totalContentHeight <= viewportHeight) {
    startY = 32 - (totalContentHeight / 2) + 10;
  }

  // Draw lines visible within the viewport (Y=12 to Y=55)
  for (size_t i = 0; i < lines.size(); i++) {
    int currentY = startY + ((int)i * lineHeight);

    if (currentY >= 12 && currentY <= 55) {
      if (lines[i].length() > 0) {
        String rtlLine = fixHebrewRTL(lines[i]);
        int lineWidth = u8g2.getUTF8Width(rtlLine.c_str());
        int lineX = (128 - lineWidth) / 2;
        if (lineX < 4) lineX = 4;
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
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  u8g2.begin();
  u8g2.enableUTF8Print();

  loadBlessingsFromLittleFS();

  resetScrollPosition();
  renderPage(currentPage);
}

void loop() {
  unsigned long currentMillis = millis();
  int totalPages = (int)birthdayMessages.size();
  if (totalPages == 0) return;

  // --- Push Button Handling (GPIO 25) ---
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  // Active LOW button press detection
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    if (currentMillis - lastButtonPressTime > DEBOUNCE_DELAY_MS) {
      lastButtonPressTime = currentMillis;
      
      // Switch to Next Page ONLY on button press
      currentPage = (currentPage + 1) % totalPages;
      Serial.print("Button Pressed! Page switched to: ");
      Serial.println(currentPage);
      
      // Reset vertical scroll animation to top of new blessing
      resetScrollPosition();
    }
  }
  lastButtonState = currentButtonState;

  // Render current frame with continuous smooth vertical scrolling animation (~30 FPS)
  renderPage(currentPage);

  delay(25);
}
