#pragma once

// Fork-only («Читанка» / Chytanka): one-time welcome after Chytanka is
// installed over an existing CrossInk/CrossPoint setup. Offers the Ukrainian
// UI language and the recommended reading settings (Bitter, hyphenation on,
// text anti-aliasing off); «Застосувати» applies the ticked ones, «Пізніше»
// changes nothing. Either choice writes a marker so it never shows again.
// Only compiled in when the build defines CHYTANKA.

#include <cstdint>
#include <memory>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

namespace chytanka {

// Boot hook (main.cpp, routing): true when the welcome screen should replace
// Home for this boot. Writes the marker itself when the screen is not needed
// (fresh SD card, earlier Chytanka release, nothing to offer).
bool welcomeScreenNeeded();

class WelcomeActivity final : public Activity {
 public:
  WelcomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum Item : uint8_t { ITEM_LANGUAGE, ITEM_READING, ITEM_APPLY, ITEM_LATER };

  ButtonNavigator buttonNavigator;
  Item items[4] = {};
  uint8_t itemCount = 0;
  uint8_t focus = 0;
  bool offerLanguage = false;
  bool offerReading = false;
  bool applyLanguage = true;
  bool applyReading = true;
  // Input is ignored until every button has been seen released once, so a
  // button still held from boot cannot pick an option by itself.
  bool inputArmed = false;

  void activate();
  void finishWith(bool apply);
};

}  // namespace chytanka
