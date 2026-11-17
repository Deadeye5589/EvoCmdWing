#include "eepromStorage.h"
#include "utils.h"
#include <EEPROM.h>

// ================================
// CONFIGURATION DEFAULTS
// ================================

const ConfigData defaultConfig = {
  .signature = CONFIG_SIGNATURE,
  .version = 2,
  .relativeEncoderSensitivity = 5,
  .absoluteEncoderSensitivity = 5,
  .onBrightness = 1.0f,
  .offBrightness = 0.05f,
  .logoBrightness = 1.0f,
  .btnBrightness = 1.0f,
  .logoRed = 127,
  .logoGreen = 64,
  .logoBlue = 0,
  .btnRed = 0,
  .btnGreen = 127,
  .btnBlue = 64
};

// Global config instance
ConfigData config;

// ================================
// EEPROM FUNCTIONS
// ================================

void initializeEEPROM() {
  debugPrint("[EEPROM] Initializing configuration system");
  
  // Try to load config from EEPROM
  if (!loadConfig()) {
    // If load fails, use defaults and save them
    debugPrint("[EEPROM] No valid config found, using defaults");
    resetConfigToDefaults();
    saveConfig();
  } else {
    debugPrint("[EEPROM] Valid configuration loaded");
  }
  
  printConfig();
}

bool loadConfig() {
  // Read the entire struct from EEPROM
  EEPROM.get(0, config);
  
  // Validate signature
  if (config.signature != CONFIG_SIGNATURE) {
    debugPrintf("[EEPROM] Invalid signature: 0x%08X (expected 0x%08X)", config.signature, CONFIG_SIGNATURE);
    return false;
  }
  
  // Validate version (for future compatibility)
  if (config.version != defaultConfig.version) {
    debugPrintf("[EEPROM] Version mismatch: %d (expected %d)", config.version, defaultConfig.version);
    return false;
  }
  
  // Validate ranges for safety
  if (config.relativeEncoderSensitivity < 1 || config.relativeEncoderSensitivity > 8) {
    debugPrintf("[EEPROM] Invalid relativeEncoderSensitivity: %d", config.relativeEncoderSensitivity);
    return false;
  }
  
  if (config.absoluteEncoderSensitivity < 1 || config.absoluteEncoderSensitivity > 8) {
    debugPrintf("[EEPROM] Invalid absoluteEncoderSensitivity: %d", config.absoluteEncoderSensitivity);
    return false;
  }
  
  if (config.onBrightness < 0.0f || config.onBrightness > 1.0f) {
    debugPrintf("[EEPROM] Invalid onBrightness: %.2f", config.onBrightness);
    return false;
  }
  
  if (config.offBrightness < 0.0f || config.offBrightness > 1.0f) {
    debugPrintf("[EEPROM] Invalid offBrightness: %.2f", config.offBrightness);
    return false;
  }
  
  if (config.logoBrightness < 0.0f || config.logoBrightness > 1.0f) {
    debugPrintf("[EEPROM] Invalid logoBrightness: %.2f", config.logoBrightness);
    return false;
  }

  if (config.btnBrightness < 0.0f || config.btnBrightness > 1.0f) {
    debugPrintf("[EEPROM] Invalid btnBrightness: %.2f", config.btnBrightness);
    return false;
  }

  if (config.logoRed > 127 || config.logoGreen > 127 || config.logoBlue > 127 ||
      config.btnRed > 127 || config.btnGreen > 127 || config.btnBlue > 127) {
    debugPrint("[EEPROM] Invalid LED color value");
    return false;
  }
  
  debugPrint("[EEPROM] Configuration validation passed");
  return true;
}

void saveConfig() {
  // Ensure signature and version are set
  config.signature = CONFIG_SIGNATURE;
  config.version = defaultConfig.version;
  
  // Write the entire struct to EEPROM
  EEPROM.put(0, config);
  
  debugPrint("[EEPROM] Configuration saved");
  printConfig();
}

void resetConfigToDefaults() {
  config = defaultConfig;
  debugPrint("[EEPROM] Configuration reset to defaults");
}

void printConfig() {
  debugPrint("[CONFIG] Current settings:");
  debugPrintf("  Signature: 0x%08X", config.signature);
  debugPrintf("  Version: %d", config.version);
  debugPrintf("  Relative Encoder Sensitivity: %d", config.relativeEncoderSensitivity);
  debugPrintf("  Absolute Encoder Sensitivity: %d", config.absoluteEncoderSensitivity);
  debugPrintf("  On Brightness: %.2f", config.onBrightness);
  debugPrintf("  Off Brightness: %.2f", config.offBrightness);
  debugPrintf("  Logo Brightness: %.2f", config.logoBrightness);
  debugPrintf("  Logo Color (R,G,B): %d, %d, %d", config.logoRed, config.logoGreen, config.logoBlue);
  debugPrintf("  Button Brightness: %.2f", config.btnBrightness);
  debugPrintf("  Button Color (R,G,B): %d, %d, %d", config.btnRed, config.btnGreen, config.btnBlue);
}