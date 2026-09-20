# ESP32 Hebrew Birthday Display 🎂🎉

An interactive, premium birthday greeting display for **ESP32** using a **0.96" SSD1306 OLED** screen, a **Push Button**, and a **Passive Buzzer**. Features custom UTF-8 Hebrew font rendering, Right-to-Left (RTL) string handling, smooth vertical scrolling for long messages, dynamic filesystem loading, 3-second long-press musical melody playback, and auto-sleep mode.

---

## 🔌 Hardware Connections

| Component | ESP32 Pin | Function | Wiring Details |
|---|---|---|---|
| **Push Button** | `GPIO 25` | Page Toggle / 3s Melody Hold / Wake Input | Terminal 1 to GPIO 25, Terminal 2 to GND (Internal Pull-Up) |
| **Passive Buzzer** | `GPIO 26` | Melody Audio Line | Positive Pin (+) to GPIO 26, Negative Pin (-) to GND |
| **SSD1306 OLED** | `GPIO 27` | SDA (I2C Data) | Data Line |
| **SSD1306 OLED** | `GPIO 33` | SCL (I2C Clock) | Clock Line |
| **VCC** | `3.3V` / `5V` | Power Supply | Power Line |
| **GND** | `GND` | Common Ground | Ground Line |

---

## 🌟 Key Features

1. **Festive Birthday Cake Opening Animation**: On startup or wake-up, the OLED displays an animated birthday cake with flickering candle flames and festive sparkles.
2. **3-Second Hold Happy Birthday Melody**: Holding the push button for **3 seconds** plays the full "Happy Birthday" song melody on the passive buzzer (`GPIO 26`) with an animated `* MUSIC MODE *` screen!
3. **Push-Button Navigation**:
   - **Short Click**: Switches to the next Hebrew blessing.
   - **Hold 3 Seconds**: Triggers the Happy Birthday song melody.
4. **Native Hebrew UTF-8 & RTL Engine**: Uses the `U8g2` library (`u8g2_font_cu12_t_hebrew`) combined with a custom multi-byte UTF-8 parser (`fixHebrewRTL`) to render Hebrew correctly Right-to-Left on Left-to-Right OLED screens.
5. **Smooth Vertical Scrolling**: Long multi-line blessings automatically wrap and smoothly scroll vertically with pause timers at the top and bottom.
6. **100% Dynamic Page Scaling**: Dynamically loads `.txt` files from `data/Blessings/`. Whether you have 3, 6, 10, or more blessings, **no code changes are needed**! Page indicator dots adjust automatically.
7. **Auto-Sleep Desk Companion**: Enters power-saving sleep mode after **3 minutes of inactivity** (showing a cozy `לילה טוב` message). Pressing the button instantly wakes the device up.

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