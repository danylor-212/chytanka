#pragma once

// Fork-only («Читанка» / Chytanka): when to show the one-time welcome screen
// after Chytanka is installed over an existing CrossInk/CrossPoint setup.
// Hardware-free so the native test suite can check every case.

namespace chytanka {

struct WelcomeState {
  bool markerExists = false;          // a choice was already made (or not needed)
  bool settingsFileExists = false;    // CrossInk/CrossPoint settings on the SD card
  bool usedChytankaBefore = false;    // traces of 1.6.0.0/1.6.0.1 (quote history, thumbnail marker, OPDS flag)
  bool languageIsUkrainian = false;   // UI language already Українська
  bool readingIsRecommended = false;  // Bitter, hyphenation on, anti-aliasing off
};

enum class WelcomeDecision {
  None,      // marker present: nothing to do
  MarkOnly,  // write the marker silently and never ask
  Show,      // show the welcome screen; the marker is written after a choice
};

// A fresh SD card already gets Chytanka's defaults, an earlier Chytanka
// release already made its own first impression, and there is nothing to
// offer when both recommendations are already in place.
constexpr WelcomeDecision decideWelcome(const WelcomeState& s) {
  if (s.markerExists) return WelcomeDecision::None;
  if (!s.settingsFileExists || s.usedChytankaBefore) return WelcomeDecision::MarkOnly;
  if (s.languageIsUkrainian && s.readingIsRecommended) return WelcomeDecision::MarkOnly;
  return WelcomeDecision::Show;
}

}  // namespace chytanka
