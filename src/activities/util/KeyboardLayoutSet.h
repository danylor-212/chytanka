#pragma once

#include <FreeInkUI.h>
#include <I18n.h>

#include <cstdint>

namespace keyboard_layouts {

struct LayoutInfo {
  freeink::ui::KeyboardLayoutId id;
  Language language;
};

// Table position is the persisted bit assignment. Append future layouts so
// SDK enum changes cannot reinterpret an existing settings file.
inline constexpr LayoutInfo ALL[] = {
    {freeink::ui::KeyboardLayoutId::QwertyEn, Language::EN},
    {freeink::ui::KeyboardLayoutId::AzertyFr, Language::FR},
    {freeink::ui::KeyboardLayoutId::QwertzDe, Language::DE},
    {freeink::ui::KeyboardLayoutId::SpanishEs, Language::ES},
    {freeink::ui::KeyboardLayoutId::CyrillicRu, Language::RU},
    {freeink::ui::KeyboardLayoutId::CyrillicUk, Language::UK},
    {freeink::ui::KeyboardLayoutId::CyrillicBe, Language::BE},
    {freeink::ui::KeyboardLayoutId::CyrillicKk, Language::KK},
    {freeink::ui::KeyboardLayoutId::HebrewIl, Language::HE},
};
inline constexpr uint8_t COUNT = sizeof(ALL) / sizeof(ALL[0]);
static_assert(COUNT <= 16, "keyboard layout mask is uint16_t");

inline constexpr uint16_t bitAt(const uint8_t i) { return static_cast<uint16_t>(1u << i); }
inline constexpr uint16_t LATIN_BITS = bitAt(0) | bitAt(1) | bitAt(2) | bitAt(3);

// Layouts this build offers. Chytanka builds keep only English and Ukrainian;
// the persisted bit positions above stay unchanged so a settings file written
// by stock CrossInk still reads correctly (other bits are ignored).
#ifdef CHYTANKA
static_assert(ALL[0].id == freeink::ui::KeyboardLayoutId::QwertyEn, "bit 0 must stay English");
static_assert(ALL[5].id == freeink::ui::KeyboardLayoutId::CyrillicUk, "bit 5 must stay Ukrainian");
inline constexpr uint16_t AVAILABLE_BITS = bitAt(0) | bitAt(5);
#else
inline constexpr uint16_t AVAILABLE_BITS = static_cast<uint16_t>((uint32_t{1} << COUNT) - 1);
#endif
inline constexpr bool isAvailable(const uint8_t i) { return (AVAILABLE_BITS & bitAt(i)) != 0; }

uint16_t enabled();
freeink::ui::KeyboardLayoutId startingLayout();
freeink::ui::KeyboardLayoutId next(freeink::ui::KeyboardLayoutId current);

}  // namespace keyboard_layouts
