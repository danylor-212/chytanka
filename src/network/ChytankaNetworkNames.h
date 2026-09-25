#pragma once

// Fork-only («Читанка» / Chytanka): the names the device uses on the network.
// Only meaningful when the build defines CHYTANKA; each call site keeps
// CrossInk's own value in its #else branch, so stock builds are unchanged.
//
//   mDNS hostname   chytanka          -> http://chytanka.local/
//   Wi-Fi AP SSID   Chytanka          (stock: CrossPoint-Reader, no MAC)
//   DHCP hostname   Chytanka-<MAC12>  (stock: CrossPoint-Reader-<MAC12>)
//   User-Agent      Chytanka/<version> (CrossInk)
//
// Length limits: an SSID is at most 32 bytes (802.11) and the DHCP hostname
// at most 32 (lwIP/esp_netif), so "Chytanka-" + 12 hex digits (21 bytes)
// fits both. mDNS labels allow 63 bytes.

#ifdef CHYTANKA

#include <AppVersion.h>

#define CHYTANKA_MDNS_HOSTNAME "chytanka"
#define CHYTANKA_AP_SSID "Chytanka"
#define CHYTANKA_DHCP_HOSTNAME_PREFIX "Chytanka-"
#define CHYTANKA_USER_AGENT "Chytanka/" CROSSINK_VERSION " (CrossInk)"

#endif  // CHYTANKA
