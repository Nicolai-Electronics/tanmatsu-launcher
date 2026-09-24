# `esp_hal_security` (G1 component)

The `esp_hal_security` component provides a **Hardware Abstraction Layer** for security-related peripherals across all targets supported by ESP-IDF.

This component contains HAL implementations for the following security peripherals:

- **AES** (Advanced Encryption Standard)
- **SHA** (Secure Hash Algorithm)
- **HMAC** (Hash-based Message Authentication Code)
- **MPI** (Modular Polynomial Integer - RSA operations)
- **ECC** (Elliptic Curve Cryptography)
- **ECDSA** (Elliptic Curve Digital Signature Algorithm)
- **DS** (Digital Signature)
- **Key Manager** (Hardware key management)
- **HUK** (Hardware Unique Key)
- **APM** (Access Permission Manager)
- **MPU** (Memory Protection Unit)

## Structure

Similar to the main `hal` component, this component follows the same structure:

- **HAL layer** (`include/esp_hal_security/<periph>_hal.h`): High-level abstraction for peripheral operations
- **LL layer** (`<target>/include/esp_hal_security/<periph>_ll.h`): Low-level register access functions
- **Types** (`include/esp_hal_security/<periph>_types.h`): Type definitions and constants shared across layers

## Usage

This component is automatically included when you depend on components that use security peripherals, such as:
- `esp_security`
- `mbedtls`
- `bootloader_support` (for secure boot and flash encryption)

You typically don't need to explicitly add this component to your `CMakeLists.txt` unless you're directly using security HAL APIs.

## Note

This component was split from the main `hal` component to better organize security-related functionality and manage dependencies. Components that previously depended on `hal` for security features should now depend on `esp_hal_security` instead.

## Vendored override

This copy overrides the stock ESP-IDF v6.0.2 `esp_hal_security` component
(via ESP-IDF's same-name component precedence: project `components/` wins
over `$IDF_PATH/components/`) to re-enable the ESP32-P4 hardware ECDSA
peripheral on chip revisions before 3.0 ("eco5"). Stock ESP-IDF disables it
on those revisions due to a known side-channel key-extraction vulnerability;
this project's target hardware (rev 1.0-1.3) does not need ECDSA to resist
physical attacks and wants the hardware acceleration instead of the mbedtls
software fallback.

Changes vs. upstream, both confined to
`esp32p4/include/hal/ecdsa_ll.h`:
- `ecdsa_ll_is_supported()` unconditionally returns `true` instead of
  gating on chip revision >= eco5 (300). (Not Kconfig-gated: this
  component's `Kconfig.hal_security` is not reachable through the
  same-name component override - `components/hal/Kconfig` sources it via
  a path relative to `hal`'s own, non-overridden directory
  [`orsource "../esp_hal_security/Kconfig.hal_security"`], so it always
  resolves to the stock ESP-IDF copy regardless of this override. A
  Kconfig toggle here would silently do nothing.)
- `ecdsa_ll_set_curve()` now asserts instead of silently corrupting when
  `ECDSA_CURVE_SECP384R1` is requested on rev <3.0 silicon, because that
  register layout has no P-384 curve-select bit (pure correctness guard,
  unrelated to the security tradeoff above).

Everything else in this component is an unmodified copy of ESP-IDF v6.0.2
(`components/esp_hal_security`), kept in sync wholesale so other peripherals
(AES/SHA/HMAC/DS/ECC/MPI/Key Manager/HUK/MPU/APM) continue to work exactly
as upstream.

