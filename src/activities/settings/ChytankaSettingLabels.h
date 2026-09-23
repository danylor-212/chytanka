#pragma once

// Fork-only («Читанка» / Chytanka): option labels that differ from stock
// CrossInk. Kept out of the shared translation files so CrossInk rebases stay
// conflict-free; any UI language other than Ukrainian uses the English label.

#ifdef CHYTANKA

#include <I18n.h>

#include <cstring>

namespace chytanka {

// Chytanka's Dark and Light sleep screens show the embedded quote cards
// instead of CrossInk's logo, so they are labelled after the quotes. Returns
// nullptr when the stock label applies.
inline const char* settingOptionLabelOverride(const char* settingKey, const StrId option) {
  if (settingKey == nullptr || std::strcmp(settingKey, "sleepScreen") != 0) return nullptr;
  const bool uk = I18N.getLanguage() == Language::UK;
  if (option == StrId::STR_LIGHT) return uk ? "Цитати — світла тема" : "Quotes — light";
  if (option == StrId::STR_DARK) return uk ? "Цитати — темна тема" : "Quotes — dark";
  return nullptr;
}

}  // namespace chytanka

#endif  // CHYTANKA
