#ifndef FRONTPANEL_USBHOST_H
#define FRONTPANEL_USBHOST_H
#include "SDT.h"

/*
 * USB Host driver -- owns the USBHost / USBHub / parser singletons used by
 * the keyboard and mouse modules. Ported from T41_SDR/t41USBHost.cpp
 * (plumbing only; first-pass port).
 *
 * Gated by USB_HOST_INPUT_ENABLED in Config.h. When that flag is undefined
 * (default), this module compiles to no-ops and pulls no extra dependencies.
 *
 * Phoenix integration (when the flag is enabled):
 *   InitializeUSBHost() should be called once from setup() (Globals.cpp).
 *   TickUSBHost() should be called each iteration of the main loop
 *   (Loop.cpp). It runs the host stack and pumps the mouse driver.
 *
 * The keyboard buffer is drained by callers via FrontPanel_USBKeyboard.h
 * (KeyboardReadChar()).
 */

/**
 * @brief Initialize the USB host stack and any attached HID devices.
 * @note No-op when USB_HOST_INPUT_ENABLED is not defined in Config.h.
 *       When enabled, calls usbHost.begin() and KeyboardSetup().
 */
void InitializeUSBHost(void);

/**
 * @brief Pump the USB host stack and mouse polling loop.
 * @note Call once per main-loop iteration. No-op when USB_HOST_INPUT_ENABLED
 *       is not defined. When enabled, calls usbHost.Task() then MouseLoop().
 */
void TickUSBHost(void);

#endif /* FRONTPANEL_USBHOST_H */
