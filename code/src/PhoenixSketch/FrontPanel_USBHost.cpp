#include "SDT.h"
#include "FrontPanel_USBHost.h"
#include "FrontPanel_USBKeyboard.h"
#include "FrontPanel_USBMouse.h"

/*
 * USB host driver -- ported from T41_SDR/t41USBHost.cpp. Plumbing only:
 * everything is gated by USB_HOST_INPUT_ENABLED so the default Phoenix build
 * has no library dependency.
 */

#ifdef USB_HOST_INPUT_ENABLED

#include <USBHost_t36.h>  // vendored at code/src/USBHost_t36/ (symlinked or via Teensyduino); see VENDORED.md

// Singletons. Owned by this translation unit; referenced by the keyboard and
// mouse drivers via 'extern' in their .cpp files.
USBHost           usbHost;
USBHub            usbHub(usbHost);
USBHIDParser      hkbParser(usbHost);    // each HID device needs its own parser
KeyboardController kbController(usbHost);
USBHIDParser      mouseParser(usbHost);
MouseController   mouseController(usbHost);

void InitializeUSBHost(void) {
    usbHost.begin();
    KeyboardSetup();
    MouseSetup();
    delay(1000);  // matches T41 -- give attached devices a moment to enumerate
}

void TickUSBHost(void) {
    usbHost.Task();
    MouseLoop();
}

#else  /* USB_HOST_INPUT_ENABLED */

void InitializeUSBHost(void) {
    // USB host input disabled. Define USB_HOST_INPUT_ENABLED in Config.h
    // to enable.
}

void TickUSBHost(void) {
    // USB host input disabled.
}

#endif  /* USB_HOST_INPUT_ENABLED */
