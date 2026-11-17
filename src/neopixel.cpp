#include "neopixel.h"
#include "utils.h"
#include <algorithm>
#include <cmath>

// ================================
// NEOPIXEL GLOBAL VARIABLES
// ================================

// Default LED mapping - can be manually adjusted if physical layout differs
// X-key LEDs follow the logo LEDs contiguously (pixel indices 6-21)
int xkeyPixelMap[NUM_XKEYS] = {
  6,  //XKEY1
  7,  //XKEY2
  8,  //XKEY3 
  9,  //XKEY4
  
  10,  //XKEY5 
  11,  //XKEY6
  12,  //XKEY7 
  13,  //XKEY8
  
  14,  //XKEY9
  15,  //XKEY10 
  16,  //XKEY11 
  17,  //XKEY12
  
  18,  //XKEY13 
  19,  //XKEY14 
  20,  //XKEY15 
  21   //XKEY16
};

int keyboardPixelMap[NUM_BTN_LEDS] = {
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
  42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
  62, 63, 64, 65, 66, 67, 68, 69, 70, 71,
  72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
  82, 83, 84, 85, 86, 87, 88, 89, 90
};

// Note: Brightness controls moved to config struct in eeprom.h

// LED Update Debounce System
unsigned long lastMidiUpdateTime = 0;
bool hasPendingLEDUpdate = false;

// NeoPixel strip object
Adafruit_NeoPixel strip(TOTAL_PIXELS, LED_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel tooglebutton(LATCH_LED_COUNT, LATCH_LED_PIN, NEO_RGB + NEO_KHZ800);

// ================================
// NEOPIXEL LED CONTROL FUNCTIONS
// ================================

// Initialize the NeoPixel LED strip
// Sets up the strip and turns all LEDs off
void initializeLEDs() {
  strip.begin();
  strip.setBrightness(255);

  tooglebutton.begin();
  tooglebutton.setBrightness(255);

  clearAllLEDs();
  
  strip.show();
  tooglebutton.show();
  
  debugPrintf("[LED] Initialized %d pixel strip on pin %d", TOTAL_PIXELS, LED_PIN);
  debugPrintf("[Toggle LED] Initialized %d LED on pin %d", LATCH_LED_COUNT, LATCH_LED_PIN);
}

// Clear all LEDs on the PCB (set to black/off)
void clearAllLEDs() {
  for (int i = 0; i < TOTAL_PIXELS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  for (int j = 0; j < LATCH_LED_COUNT; j++) {
    tooglebutton.setPixelColor(j, tooglebutton.Color(0,0,0));
  }
  tooglebutton.show();
  debugPrint("[LED] All LEDs cleared");
}



void setLogoPixels(uint8_t red, uint8_t green, uint8_t blue, float brightness) {
  uint8_t scaledRed = (uint8_t)((red * 2) * brightness);
  uint8_t scaledGreen = (uint8_t)((green * 2) * brightness);
  uint8_t scaledBlue = (uint8_t)((blue * 2) * brightness);
  
  for (int i = 0; i < NUM_LOGO_PIXELS; i++) {
    // Since the Logo LEDs have a different color order, we need to swap them here to be correct again
    strip.setPixelColor(i, strip.Color(scaledGreen, scaledRed, scaledBlue));
  }
  
  strip.show();

}

void setKeyboardPixels(uint8_t red, uint8_t green, uint8_t blue, float brightness) {
  uint8_t scaledRed = (uint8_t)((red * 2) * brightness);
  uint8_t scaledGreen = (uint8_t)((green * 2) * brightness);
  uint8_t scaledBlue = (uint8_t)((blue * 2) * brightness);
  
  for (int i = TOTAL_PIXELS - NUM_BTN_LEDS; i < TOTAL_PIXELS; i++) {
    strip.setPixelColor(i, strip.Color(scaledRed, scaledGreen, scaledBlue));
  }
  
  strip.show();

}

void updateToggleButtonLED() {
  uint8_t red = latchButtonState ? LATCH_LED_ON_RED : LATCH_LED_OFF_RED;
  uint8_t green = latchButtonState ? LATCH_LED_ON_GREEN : LATCH_LED_OFF_GREEN;
  uint8_t blue = latchButtonState ? LATCH_LED_ON_BLUE : LATCH_LED_OFF_BLUE;

  for (int i = 0; i < LATCH_LED_COUNT; i++) {
    tooglebutton.setPixelColor(i, tooglebutton.Color(
        (uint8_t)(red * config.btnBrightness),
        (uint8_t)(green * config.btnBrightness),
        (uint8_t)(blue * config.btnBrightness)));
  }
  tooglebutton.show();
}

// Converts RGB to HSV and scales value, then returns scaled RGB color
// This preserves hue and saturation while scaling brightness properly
uint32_t getScaledColor(uint8_t red, uint8_t green, uint8_t blue, float brightness) {
  // Scale MIDI values (0-127) to full RGB range (0-255) first
  red = red * 2;
  green = green * 2;
  blue = blue * 2;
  
  // Special case: if original color is black (0,0,0), keep it black
  if (red == 0 && green == 0 && blue == 0) {
    return strip.Color(0, 0, 0);  // Always return black regardless of brightness
  }

  float r = red / 255.0f;
  float g = green / 255.0f;
  float b = blue / 255.0f;

  float cmax = std::max(r, std::max(g, b));
  float cmin = std::min(r, std::min(g, b));
  float delta = cmax - cmin;

  float h = 0, s = 0;

  // Calculate hue
  if (delta != 0) {
    if (cmax == r) h = fmodf(((g - b) / delta), 6.0f);
    else if (cmax == g) h = ((b - r) / delta) + 2.0f;
    else h = ((r - g) / delta) + 4.0f;

    h *= 60.0f;
    if (h < 0) h += 360.0f;
  }

  // Calculate saturation
  if (cmax != 0) s = delta / cmax;

  // Use brightness parameter as the 'value' in HSV
  float scaledV = brightness;

  // Convert HSV back to RGB with scaled brightness
  float c = scaledV * s;
  float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
  float m = scaledV - c;

  float r1 = 0, g1 = 0, b1 = 0;

  if (h < 60)      { r1 = c; g1 = x; }
  else if (h < 120){ r1 = x; g1 = c; }
  else if (h < 180){ g1 = c; b1 = x; }
  else if (h < 240){ g1 = x; b1 = c; }
  else if (h < 300){ r1 = x; b1 = c; }
  else             { r1 = c; b1 = x; }

  return strip.Color(
    (uint8_t)((r1 + m) * 255),
    (uint8_t)((g1 + m) * 255),
    (uint8_t)((b1 + m) * 255)
  );
}

// Set the color and brightness for a specific X-key's LEDs
// xkeyIndex: 0-15 for XKey 1-16
// red, green, blue: 0-127 (MIDI range) - will be scaled to 0-255
// brightness: 0.0-1.0 multiplier for the color intensity using HSV scaling
void setXKeyLED(int xkeyIndex, uint8_t red, uint8_t green, uint8_t blue, float brightness) {

  if (xkeyIndex < 0 || xkeyIndex >= NUM_XKEYS) {
    debugPrintf("[LED] Invalid XKey index: %d", xkeyIndex);
    return;
  }
  
  // Use HSV scaling
  uint32_t scaledColor = getScaledColor(red, green, blue, brightness);
  
  strip.setPixelColor(xkeyPixelMap[xkeyIndex], scaledColor);
  
}

void updateXKeyLEDs() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  
  // Don't update XKey LEDs when in sensitivity mode (allow brightness adjustments to show)
  if (sensitivityMode) {
    return;
  }
  
  // Wait enough time to get all 3 values rgb, for cleaner color changes
  if (now - lastUpdate < 50) {
    return;
  }
  lastUpdate = now;
  
  bool ledChanged = false;
  
  for (int i = 0; i < NUM_XKEYS; i++) {
    ExecutorStatus* status = &xkeyStatus[i];
    
    if (!status->isPopulated) {
      // Key not populated - turn LEDs off
      setXKeyLED(i, 0, 0, 0, 0.0);
      ledChanged = true;
    } else if (status->isPopulated && !status->isOn) {
      // Key populated but not on - use offBrightness
      setXKeyLED(i, status->red, status->green, status->blue, config.offBrightness);
      ledChanged = true;
    } else if (status->isPopulated && status->isOn) {
      // Key populated and on - use onBrightness
      setXKeyLED(i, status->red, status->green, status->blue, config.onBrightness);
      ledChanged = true;
    }
  }
  
  if (ledChanged) {
    strip.show();
  }
}

void showStrip() {
  strip.show();
}


// ================================
// FADE SEQUENCE GROUP DEFINITIONS
// ================================
// Each group lists raw strip pixel indices (0..TOTAL_PIXELS-1) in any order and any count.
// The wave runs through the groups in the order they appear in fadeGroups[].
static const uint16_t fadeGroup1[]  = { 22, 27, 32, 3, 2 };
static const uint16_t fadeGroup2[]  = { 4, 1, 67, 72, 81};
static const uint16_t fadeGroup3[]  = { 23, 28, 33, 5, 0 };
static const uint16_t fadeGroup4[]  = { 24, 29, 34, 37, 40, 50, 65, 73, 83 };
static const uint16_t fadeGroup5[]  = { 25, 30, 35, 38, 41, 51, 60, 74 };
static const uint16_t fadeGroup6[]  = { 26, 31, 36, 39, 42, 52, 61, 75 };
static const uint16_t fadeGroup7[]  = { 6, 14, 43, 53, 62, 76, 85 };
static const uint16_t fadeGroup8[]  = { 7, 15, 44, 54, 63, 68, 77, 86 };
static const uint16_t fadeGroup9[]  = { 8, 16, 45, 55, 64 ,69, 78, 87 };
static const uint16_t fadeGroup10[]  = { 9, 17, 46, 56, 65, 70, 79, 88 };
static const uint16_t fadeGroup11[] = { 10, 18, 57, 66, 71, 80, 89 };
static const uint16_t fadeGroup12[] = { 11, 19, 47 };
static const uint16_t fadeGroup13[] = { 12, 20, 58, 81, 90 };
static const uint16_t fadeGroup14[] = { 13, 21, 49, 91 };

struct FadeGroup {
  const uint16_t* pixels;
  uint8_t count;
  bool includeToggle;  // also drive all pixels of the tooglebutton strip
};

#define FADE_GROUP(arr) { arr, (uint8_t)(sizeof(arr) / sizeof(arr[0])), false }
#define FADE_GROUP_WITH_TOGGLE(arr) { arr, (uint8_t)(sizeof(arr) / sizeof(arr[0])), true }

static const FadeGroup fadeGroups[] = {
  FADE_GROUP_WITH_TOGGLE(fadeGroup1),
  FADE_GROUP(fadeGroup2),
  FADE_GROUP(fadeGroup3),
  FADE_GROUP(fadeGroup4),
  FADE_GROUP(fadeGroup5),
  FADE_GROUP(fadeGroup6),
  FADE_GROUP(fadeGroup7),
  FADE_GROUP(fadeGroup8),
  FADE_GROUP(fadeGroup9),
  FADE_GROUP(fadeGroup10),
  FADE_GROUP(fadeGroup11),
  FADE_GROUP(fadeGroup12),
  FADE_GROUP(fadeGroup13),
  FADE_GROUP(fadeGroup14)
};

static const int NUM_FADE_GROUPS = sizeof(fadeGroups) / sizeof(fadeGroups[0]);

static void setFadeGroupColor(const FadeGroup& group, uint32_t color) {
  for (uint8_t i = 0; i < group.count; i++) {
    if (group.pixels[i] < TOTAL_PIXELS) {
      strip.setPixelColor(group.pixels[i], color);
    }
  }
  if (group.includeToggle) {
    for (int j = 0; j < LATCH_LED_COUNT; j++) {
      tooglebutton.setPixelColor(j, color);
    }
  }
}

static uint32_t scaleColor(uint32_t c, float factor) {
  return strip.Color((uint8_t)(((c >> 16) & 0xFF) * factor),
                     (uint8_t)(((c >> 8) & 0xFF) * factor),
                     (uint8_t)((c & 0xFF) * factor));
}

// Startup fade sequence with rainbow wave effect AND bouncing over the hand-defined groups
void xkeyFadeSequenceBounce(unsigned long STAGGER_DELAY, unsigned long COLOR_CYCLE_TIME, int cycles, int bounces) {

  const unsigned long FADE_OUT_DURATION = 250;
  unsigned long startTime = millis();

  unsigned long singleWaveTime = (NUM_FADE_GROUPS - 1) * STAGGER_DELAY + COLOR_CYCLE_TIME + FADE_OUT_DURATION;
  unsigned long wavesPerCycle = 1 + (bounces * 2);
  unsigned long timePerCycle = wavesPerCycle * singleWaveTime;
  unsigned long fullSequenceTime = timePerCycle * cycles;

  // Remember the current color of every pixel so it can be restored afterwards
  static uint32_t originalColors[TOTAL_PIXELS];
  for (int i = 0; i < TOTAL_PIXELS; i++) {
    originalColors[i] = strip.getPixelColor(i);
  }
  static uint32_t originalToggleColors[LATCH_LED_COUNT];
  for (int j = 0; j < LATCH_LED_COUNT; j++) {
    originalToggleColors[j] = tooglebutton.getPixelColor(j);
  }

  for (int g = 0; g < NUM_FADE_GROUPS; g++) { 
    setFadeGroupColor(fadeGroups[g], 0);
  }
  strip.show();
  tooglebutton.show();

  while (true) {
    unsigned long now = millis();
    unsigned long totalElapsed = now - startTime;

    if (totalElapsed >= fullSequenceTime) {
      break;
    }

    unsigned long cycleElapsed = totalElapsed % timePerCycle;
    unsigned long currentWaveInCycle = cycleElapsed / singleWaveTime;
    unsigned long waveElapsed = cycleElapsed % singleWaveTime;
    bool isReverse = (currentWaveInCycle % 2 == 1);

    for (int g = 0; g < NUM_FADE_GROUPS; g++) {
      const FadeGroup& group = fadeGroups[g];
      int effectiveGroup = isReverse ? (NUM_FADE_GROUPS - 1 - g) : g;
      unsigned long groupStartOffset = effectiveGroup * STAGGER_DELAY;

      if (waveElapsed < groupStartOffset) {
        setFadeGroupColor(group, 0);
        continue;
      }

      unsigned long elapsed = waveElapsed - groupStartOffset;

      if (elapsed < COLOR_CYCLE_TIME) {
        float progress = elapsed / (float)COLOR_CYCLE_TIME;
        float hue = progress * 360.0f;
        float x = 1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f);

        float r1, g1, b1;
        if (hue < 60)       { r1 = 1; g1 = x; b1 = 0; }
        else if (hue < 120) { r1 = x; g1 = 1; b1 = 0; }
        else if (hue < 180) { r1 = 0; g1 = 1; b1 = x; }
        else if (hue < 240) { r1 = 0; g1 = x; b1 = 1; }
        else if (hue < 300) { r1 = x; g1 = 0; b1 = 1; }
        else                { r1 = 1; g1 = 0; b1 = x; }

        float breatheValue = (sinf(progress * PI * 2) + 1.0f) / 2.0f;
        float fadeIn = min(1.0f, progress / 0.3f);
        float brightness = breatheValue * fadeIn * 255.0f;

        setFadeGroupColor(group, strip.Color((uint8_t)(r1 * brightness),
                                             (uint8_t)(g1 * brightness),
                                             (uint8_t)(b1 * brightness)));
      } else {
        float fadeProgress = min(1.0f, (elapsed - COLOR_CYCLE_TIME) / (float)FADE_OUT_DURATION);

        // Fade each pixel back in to its own original color
        for (uint8_t i = 0; i < group.count; i++) {
          uint16_t pixel = group.pixels[i];
          if (pixel >= TOTAL_PIXELS) continue;
          strip.setPixelColor(pixel, scaleColor(originalColors[pixel], fadeProgress));
        }
        if (group.includeToggle) {
          for (int j = 0; j < LATCH_LED_COUNT; j++) {
            tooglebutton.setPixelColor(j, scaleColor(originalToggleColors[j], fadeProgress));
          }
        }
      }
    }

    strip.show();
    tooglebutton.show();
    delay(10);
  }

  for (int i = 0; i < TOTAL_PIXELS; i++) {
    strip.setPixelColor(i, originalColors[i]);
  }
  for (int j = 0; j < LATCH_LED_COUNT; j++) {
    tooglebutton.setPixelColor(j, originalToggleColors[j]);
  }
  strip.show();
  tooglebutton.show();
}

// ================================
// SENSITIVITY LED FEEDBACK FUNCTIONS
// ================================


void updateSensitivityLEDs() {
  // Show relative sensitivity on XKeys 1-8 (green)
  for (int i = 0; i < 8; i++) {
    if (i < config.relativeEncoderSensitivity) {
      setXKeyLED(i, 0, 127, 0, config.onBrightness);      // Green for active levels
    } else {
      setXKeyLED(i, 0, 0, 0, 0.0);                 // Off for inactive levels
    }
  }
  
  // Show absolute sensitivity on XKeys 9-16 (blue)
  for (int i = 8; i < 16; i++) {
    if ((i - 8) < config.absoluteEncoderSensitivity) {
      setXKeyLED(i, 0, 0, 127, config.onBrightness);      // Blue for active levels
    } else {
      setXKeyLED(i, 0, 0, 0, 0.0);                 // Off for inactive levels
    }
  }
  
  showStrip();
}
