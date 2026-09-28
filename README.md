# ESP32 Hebrew Birthday Display 🎂🎉

An interactive, premium birthday greeting display for **ESP32** using a **0.96" SSD1306 OLED** screen, a **Push Button**, and a **Passive Buzzer**. Features custom UTF-8 Hebrew font rendering, Right-to-Left (RTL) string handling, smooth vertical scrolling for long messages, dynamic filesystem loading, 3-second long-press musical melody playback, and auto-sleep mode.

---

## 🔌 Hardware Connections

### Pinout Table

| Component | ESP32 Pin | Function | Wiring Details |
|---|---|---|---|
| **BOOT Button** | `GPIO 0` | Page Toggle / 3s Melody Hold / Wake Input | Built-in on ESP32 board (**Zero external wiring!**) |
| **SSD1306 OLED** | `GPIO 14` (`D14`) | SDA (I2C Data) | Connected to OLED `SDA` |
| **SSD1306 OLED** | `GPIO 13` (`D13`) | SCL (I2C Clock) | Connected to OLED `SCL` |
| **Passive Buzzer** | `GPIO 4` (`D4`) | Melody Audio Line | Positive Pin (+) to `D4`, Negative Pin (-) to `GND` |
| **VCC** | `VIN` / `3.3V` | Power Supply | Connected to OLED `VCC` |
| **GND** | `GND` | Common Ground | Common Ground for OLED `GND` & Buzzer `(-)` |

### Visual Wiring Diagram (ASCII)

```text
                     +-------------------------------------------------+
                     |                 ESP32 Board                     |
                     |                                                 |
                     |  [BOOT Button] (Onboard GPIO 0 - No wiring!)    |
                     |                                                 |
                     |  [VIN]   [GND]   [D13]    [D14]    [D4]         |
                     +---|--------|-------|--------|--------|----------+
                         |        |       |        |        |
                         |        |       |        |        +------> (+) Buzzer Positive
                         |        |       |        +---------------> SDA (OLED Pin 3)
                         |        |       +------------------------> SCL (OLED Pin 4)
                         |        |
                         |        +--------------------+
                         |                             |
                         v                             v
                 +---------------+             +---------------+
                 |  SSD1306 OLED |             | Passive Buzzer|
                 |  (0.96" I2C)  |             | (Piezo)       |
                 +---------------+             +---------------+
                 | Pin 1: GND    |<-- GND      | (+): GPIO 4   |
                 | Pin 2: VCC    |<-- VIN/3.3V | (-): GND      |<-- GND
                 | Pin 3: SDA    |<-- D14 (14) +---------------+
                 | Pin 4: SCL    |<-- D13 (13)
                 +---------------+
```

---

## 🌟 Key Features

1. **Festive Birthday Cake Opening Animation**: On startup or wake-up, the OLED displays an animated birthday cake with flickering candle flames and festive sparkles.
2. **3-Second Hold Happy Birthday Melody**: Holding the push button for **3 seconds** plays a short fanfare followed by the full "Happy Birthday" song melody on the passive buzzer (`GPIO 26`) with an animated `* MUSIC MODE *` screen!
3. **Audio Interactive Feedback**:
   - **Short Click Chime**: Plays a quick, cheerful 2-note ascending chime when switching blessings.
   - **Wake-up Chime**: Plays a 3-note ascending melody when waking up from sleep.
   - **Long-Press Fanfare**: Triggers a fanfare sound right when the 3-second hold is recognized.
4. **Push-Button Navigation**:
   - **Short Click**: Switches to the next Hebrew blessing with a cheerful chime.
   - **Hold 3 Seconds**: Triggers the Happy Birthday song melody.
5. **Native Hebrew UTF-8 & RTL Engine**: Uses the `U8g2` library (`u8g2_font_cu12_t_hebrew`) combined with a custom multi-byte UTF-8 parser (`fixHebrewRTL`) to render Hebrew correctly Right-to-Left on Left-to-Right OLED screens.
6. **Smooth Vertical Scrolling**: Long multi-line blessings automatically wrap and smoothly scroll vertically with pause timers at the top and bottom.
7. **100% Dynamic Page Scaling**: Dynamically loads `.txt` files from `data/Blessings/`. Whether you have 3, 6, 10, or more blessings, **no code changes are needed**! Page indicator dots adjust automatically.
8. **Auto-Sleep Desk Companion**: Enters power-saving sleep mode after **3 minutes of inactivity** (showing a cozy `לילה טוב` message). Pressing the button instantly plays a wake-up chime and revives the display.

---

## 💌 How to Add / Manage Blessings

All blessings are stored as individual `.txt` text files inside the **`data/Blessings/`** folder:

```text
data/Blessings/
├── bless1.txt
├── bless2.txt
├── bless3.txt
├── bless4.txt
├── bless5.txt
└── bless6.txt
```

### Adding a New Blessing:
1. Create a new text file inside `data/Blessings/` (e.g., `bless7.txt`).
2. Write your Hebrew blessing inside the file (multi-line paragraphs and special characters like `♡` are fully supported).
3. Upload the filesystem image to your ESP32 via PlatformIO:
   ```bash
   platformio run --target uploadfs
   ```

> 💡 **No Code Modification Required**: The firmware dynamically scans the folder on startup, loads all text files alphabetically, and updates the display page counter automatically!

---

## ⚙️ Customizing Scroll Speed & Timing

You can adjust the scrolling speed and pause durations at the top of [`src/main.cpp`](src/main.cpp) or [`HappyBirthday.ino`](HappyBirthday.ino):

```cpp
#define SCROLL_SPEED     0.9f   // Speed in pixels/frame (e.g. 0.4f=slow, 0.9f=medium, 1.2f=fast)
#define PAUSE_TOP_MS     2000   // Pause at top of message (in milliseconds)
#define PAUSE_BOTTOM_MS  2800   // Pause at bottom of message (in milliseconds)
#define LONG_PRESS_MS    3000   // Hold button for 3 seconds to trigger melody
```

---

## 🛠️ How to Compile & Flash

### PlatformIO (VS Code)
1. **Flash Firmware Code**:
   ```bash
   platformio run --target upload
   ```
2. **Flash Blessings Filesystem**:
   ```bash
   platformio run --target uploadfs
   ```