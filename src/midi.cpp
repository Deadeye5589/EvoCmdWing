#include "midi.h"
#include "neopixel.h"
#include "utils.h"
#include <MIDIUSB.h>
#include <string.h>

// ================================
// MIDI GLOBAL VARIABLES
// ================================

// Current-page storage for 16 XKeys
ExecutorStatus xkeyStatus[16] = {0};

// Current page tracking (1-based)
int currentPage = 1;

// Track 14-bit page number parts from MIDI Channel 3
static uint8_t pendingPageLsb = 0;
static uint8_t pendingPageMsb = 0;

// ================================
// MIDI COMMUNICATION FUNCTIONS
// ================================

void handleIncomingMIDI() {
  // Limit messages processed per loop
  // This prevents buffer overflow from locking up the system while still processing rapidly, probably overkill
  int messageCount = 0;
  
  while (usbMIDI.read() && messageCount < MAX_MESSAGES_PER_LOOP) {
    messageCount++;
    
    byte type = usbMIDI.getType();
    byte ch = usbMIDI.getChannel();
    byte d1 = usbMIDI.getData1();
    byte d2 = usbMIDI.getData2();

    if (type == usbMIDI.ControlChange) {
      if (ch == midiCh) {
        // Channel 1: Encoder feedback
        for (int i = 5; i < N_ENCODERS; i++) {
          if (d1 == ENCODER_NOTES[i]) {
            encoderValues[i] = constrain(d2, 0, 127);
            debugPrintf("[MIDI IN CH1] CC Update - Encoder %d | CC: %d | Value: %d", (i + 1), d1, encoderValues[i]);
            break;
          }
        }
      } else if (ch == 2) {
        // Channel 2: XKey status data
        handleStatusMIDI(ch, d1, d2);
      } else if (ch == 3) {
        // Channel 3: Page changes
        handlePageMIDI(ch, d1, d2);
      }
    }

// =====================================
// EXTRA DEBUG NO LONGER NEEDED
// =====================================
  //   const char* typeStr;
  // switch (type) {
  //   case usbMIDI.NoteOff: typeStr = "Note Off   "; break;
  //   case usbMIDI.NoteOn: typeStr = "Note On    "; break;
  //   case usbMIDI.AfterTouchPoly: typeStr = "Aftertouch "; break;
  //   case usbMIDI.ControlChange: typeStr = "CC         "; break;
  //   case usbMIDI.ProgramChange: typeStr = "Program    "; break;
  //   case usbMIDI.AfterTouchChannel: typeStr = "Channel Pressure"; break;
  //   case usbMIDI.PitchBend: typeStr = "Pitch Bend "; break;
  //   default: typeStr = "Other      "; break;
  // }
  //   debugPrintf("[MIDI IN] Type: %s Ch: %d D1: %d D2: %d", typeStr, ch, d1, d2);
  
  }
  
  // Alert if we hit the message limit (indicates potential MIDI flooding)
  if (messageCount >= MAX_MESSAGES_PER_LOOP) {
    debugPrintf("[MIDI FLOOD] Processed %d messages (limit reached) - more pending", messageCount);
  }
}

// Handles MIDI Channel 3 data for page changes
// CC 1/2 = Current page number as 14-bit value (LSB/MSB, zero-based on wire)
// Lua sends MSB first, then LSB. Apply the page change when LSB arrives.
// When page changes, keep the current LED/state until the full resend from grandMA3 arrives.
void handlePageMIDI(byte ch, byte cc, byte value) {
  if (ch == 3 && (cc == 1 || cc == 2)) {
    if (cc == 2) {
      pendingPageMsb = constrain(value, 0, 127);
      return;
    }

    pendingPageLsb = constrain(value, 0, 127);

    int newPage = 1 + pendingPageLsb + (pendingPageMsb * 128);
    if (newPage != currentPage) {
      int oldPage = currentPage;
      currentPage = newPage;
      debugPrintf("[PAGE CHANGE] %d → %d (waiting for full resend, preserving current LEDs)", oldPage, newPage);
      debugPrintf("[PAGE] Now on page %d", newPage);
    } else {
      debugPrintf("[PAGE] Already on page %d", newPage);
    }
  } else {
    debugPrintf("[MIDI CH3] Unknown CC: %d Value: %d (Expected: CC 1/2 for page changes)", cc, value);
  }
}

// ================================
// EXECUTOR STATUS MIDI HANDLER  
// ================================
// Handles MIDI Channel 2 data for executor status information
// OPTIMIZED Protocol from updated grandMA3 Lua script:
//
// STATUS DATA (Combined populated/on state):
//   XKeys 1-16: CC 1-16 
//     0 = Not populated (empty/off)
//     65 = Populated but off 
//     127 = Populated and on
//
// RGB COLOR DATA (only sent when populated and color changes):
//   XKeys 1-16: CC 17-64 (3 CCs each)
//     XKey 1: Red=CC17, Green=CC18, Blue=CC19
//     XKey 2: Red=CC20, Green=CC21, Blue=CC22
//     XKey 3: Red=CC23, Green=CC24, Blue=CC25
//     ...
//     XKey 16: Red=CC62, Green=CC63, Blue=CC64
//   Pattern: XKey N uses CC (16 + (N-1)*3 + 1) through CC (16 + N*3)
void handleStatusMIDI(byte ch, byte cc, byte value) {
  int xkeyIndex = -1;
  const char* dataType = "Unknown";
  int executorNumber = -1;
  
  if (cc >= 1 && cc <= 16) {
    // Status data: CC 1-16 for XKeys 1-16
    xkeyIndex = cc - 1;
    int xkeyNumber = cc;
    dataType = "Status";
    
    // Determine executor number based on XKey number
    if (xkeyNumber >= 1 && xkeyNumber <= 8) {
      // XKeys 1-8 = Executors 291-298
      executorNumber = 290 + xkeyNumber;
    } else if (xkeyNumber >= 9 && xkeyNumber <= 16) {
      // XKeys 9-16 = Executors 191-198  
      executorNumber = 182 + xkeyNumber;  // 191 = 182 + 9
    }
    
    // Store in current page data
    ExecutorStatus* status = &xkeyStatus[xkeyIndex];
    
    // Decode combined status value
    if (value == 0) {
      // Not populated
      status->isPopulated = false;
      status->isOn = false;
    } else if (value == 65) {
      // Populated but off
      status->isPopulated = true;
      status->isOn = false;
    } else if (value == 127) {
      // Populated and on
      status->isPopulated = true;
      status->isOn = true;
    } else {
      // Invalid value - treat as not populated
      status->isPopulated = false;
      status->isOn = false;
    }
    
    debugPrintf("[MIDI CH2] Page %d XKey %d (Exec %d) %s: %d (Pop=%s On=%s)", 
                currentPage, xkeyNumber, executorNumber, dataType, value,
                status->isPopulated ? "YES" : "NO",
                status->isOn ? "ON" : "OFF");
                
  } else if (cc >= 17 && cc <= 64) {
    // RGB color data: CC 17-64 for XKeys 1-16 (3 CCs each)
    // Pattern: XKey N uses CC (16 + (N-1)*3 + 1) through CC (16 + N*3)
    int colorCC = cc - 17;  // 0-47 (48 total CCs for 16 XKeys * 3 colors)
    xkeyIndex = colorCC / 3;  // XKey index 0-15
    int colorComponent = colorCC % 3;  // 0=Red, 1=Green, 2=Blue
    
    if (xkeyIndex >= 0 && xkeyIndex < 16) {
      int xkeyNumber = xkeyIndex + 1;  // XKey 1-16
      
      // Determine executor number
      if (xkeyNumber >= 1 && xkeyNumber <= 8) {
        executorNumber = 290 + xkeyNumber;
      } else if (xkeyNumber >= 9 && xkeyNumber <= 16) {
        executorNumber = 182 + xkeyNumber;
      }
      
      // Store in current page data
      ExecutorStatus* status = &xkeyStatus[xkeyIndex];
      
      // Set color component
      switch (colorComponent) {
        case 0:  // Red
          dataType = "Red";
          status->red = value;
          break;
        case 1:  // Green
          dataType = "Green";
          status->green = value;
          break;
        case 2:  // Blue
          dataType = "Blue";
          status->blue = value;
          break;
      }
      
      debugPrintf("[MIDI CH2] Page %d XKey %d (Exec %d) %s: %d (CC:%d)", 
                  currentPage, xkeyNumber, executorNumber, dataType, value, cc);
    }
  } else {
    debugPrintf("[MIDI CH2] Unknown CC: %d Value: %d (Valid range: CC 1-64)", cc, value);
  }

  // Update LED for changed XKey and display complete status
  if (xkeyIndex >= 0 && xkeyIndex < 16) {
    ExecutorStatus* status = &xkeyStatus[xkeyIndex];
    //int xkeyNumber = xkeyIndex + 1;
    
    // Update LED immediately for this XKey
    if (!status->isPopulated) {
      // Key not populated - turn LEDs off
      setXKeyLED(xkeyIndex, 0, 0, 0, 0.0);
    } else if (status->isPopulated && !status->isOn) {
      // Key populated but not on - 5% brightness
      setXKeyLED(xkeyIndex, status->red, status->green, status->blue, config.offBrightness);
    } else if (status->isPopulated && status->isOn) {
      // Key populated and on - full brightness
      setXKeyLED(xkeyIndex, status->red, status->green, status->blue, config.onBrightness);
    }
    
    // Mark that we have pending LED updates
    // Instead of calling showStrip() immediately, wait for a brief pause
    // Update on interval for cleaner led updates
    hasPendingLEDUpdate = true;
    lastMidiUpdateTime = millis();
    
    // ===================================
    // Redundant debug output for testing
    // ====================================
    // debugPrintf("[STATUS] Page %d XKey %d (Exec %d): On=%s Pop=%s RGB=(%d,%d,%d)", 
    //             currentPage + 1, xkeyNumber, executorNumber,
    //             status->isOn ? "ON" : "OFF",
    //             status->isPopulated ? "YES" : "NO",
    //             status->red, status->green, status->blue);
  }
}
