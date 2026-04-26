# USBHost_t36 (vendored)

This is a vendored snapshot of Paul Stoffregen's Teensy USB Host library,
the same one that ships with Teensyduino. Vendoring it pins the version so
Phoenix builds reproducibly regardless of the developer's installed
Teensyduino release.

## Provenance

- **Upstream repo**: <https://github.com/PaulStoffregen/USBHost_t36>
- **Branch**: `master`
- **Commit SHA**: `0463fc1f6591c0d303698d7454f6a130cf84dd67`
- **Vendored on**: 2026-04-25
- **License**: MIT (see header comments in `USBHost_t36.h`)

## Phoenix usage

Used by the optional USB-host input subsystem (gated by `USB_HOST_INPUT_ENABLED`
in `Config.h`). Three Phoenix files include this library:

- `FrontPanel_USBHost.cpp`     - top-level driver (USBHost, USBHub, parsers)
- `FrontPanel_USBKeyboard.cpp` - HID keyboard ring buffer + char accessor
- `FrontPanel_USBMouse.cpp`    - HID mouse polling

When the flag is undefined (default), all three compile to no-ops and this
library isn't pulled into the link.

## Updating

To refresh from upstream:

```sh
git clone https://github.com/PaulStoffregen/USBHost_t36.git /tmp/usbhost
rm -rf code/src/USBHost_t36
cp -a /tmp/usbhost code/src/USBHost_t36
rm -rf code/src/USBHost_t36/.git
```

Then update the **Commit SHA** + **Vendored on** lines above to reflect the
new snapshot.

## Location and build behavior

This library lives at `code/src/USBHost_t36/`, as a sibling of the
PhoenixSketch sketch folder. Phoenix's three USB-related files
(`FrontPanel_USBHost.cpp`, `FrontPanel_USBKeyboard.cpp`,
`FrontPanel_USBMouse.cpp`) include it via the standard Arduino library
form, `#include <USBHost_t36.h>`, so the Arduino IDE finds it via its
library search path.

For that include to resolve, **the IDE must see this library on its
library search path.** Two ways to arrange that:

### Option 1 (Recommended): Symlink into the Arduino libraries path

This makes Phoenix's vendored copy the single source of truth while
letting the Arduino IDE find it via the normal library mechanism:

```sh
ln -s /Users/jpwatters/src/PhoenixVxxV02/code/src/USBHost_t36 \
      ~/Documents/Arduino/libraries/USBHost_t36
```

After symlinking, the Arduino IDE will compile this library's `.cpp` files
when it builds Phoenix (because the library is now on its library path),
and the symlink ensures any updates to the vendored copy take effect on
the next build with no further action.

### Option 2: Use Teensyduino's bundled copy

If the developer hasn't symlinked, the Arduino IDE will fall back to
whatever USBHost_t36 is installed via Teensyduino. In that case this
vendored copy serves as a reference / version-pinning artifact only --
it documents which version of the library Phoenix was developed against
even when the build uses a different one.

### Option 3: CMake build

Phoenix's `code/test/` directory uses CMake. A non-IDE build (CMake
configured for the Teensy toolchain) can add `code/src/USBHost_t36/`
explicitly to its source list. The angle-bracket include still works as
long as the CMake build adds `code/src/USBHost_t36/` to the include path
via `target_include_directories`.

## Files

The full upstream tree is preserved including `examples/` (53 demo
sketches) and `utility/` (internal headers).
