/*
  bootloader update firmware

  this updates the bootloader on an AM32 ESC. It assumes the
  bootloader flash sectors are unlocked
 */
#include <main.h>
#include <stdio.h>

#include <version.h>
#include <stdbool.h>
#include "eeprom.h"

#pragma GCC optimize("O0")

#include <string.h>

#define GPIO_PORT_TYPE typeof(GPIOA)

// dummy pin and port so we can re-use blutil.h
static GPIO_PORT_TYPE input_port;
static uint32_t input_pin;

#define MCU_FLASH_START 0x08000000


#if !DRONECAN_SUPPORT
#define FIRMWARE_RELATIVE_START 0x1000
#else

#define FIRMWARE_RELATIVE_START 0x4000

#define APP_SIGNATURE_MAGIC1 0x68f058e6
#define APP_SIGNATURE_MAGIC2 0xafcee5a0

/*
  application signature, filled in by set_app_signature.py
 */
const struct {
  uint32_t magic1;
  uint32_t magic2;
  uint32_t fwlen; // total fw length in bytes
  uint32_t crc1; // crc32 up to start of app_signature
  uint32_t crc2; // crc32 from end of app_signature to end of fw
  char mcu[16];
  uint32_t unused[2];
} app_signature __attribute__((section(".app_signature"))) = {
  .magic1 = APP_SIGNATURE_MAGIC1,
  .magic2 = APP_SIGNATURE_MAGIC2,
  .fwlen = 0,
  .crc1 = 0,
  .crc2 = 0,
  .mcu = AM32_MCU,
};
#endif

/*
  use stringize to construct an include of the right bootloader header
 */
#define bl_header BL_HEADER_FILE
#define xstr(x) #x
#define str(x) xstr(x)
#include str(bl_header)

#define PORT_LETTER      0 // dummy

#include <blutil.h>

static void delayMicroseconds(uint32_t micros)
{
  while (micros > 0) {
    uint16_t us = micros>10000?10000:micros;
    const uint16_t us_start = bl_timer_us();
    while ((uint16_t)(bl_timer_us() - us_start) < us) ;
    micros -= us;
  }
}

static void flash_bootloader(void)
{
  uint32_t length = sizeof(bl_image);
  uint32_t address = MCU_FLASH_START;
  const uint8_t *bl = &bl_image[0];

  while (length > 0) {
    uint32_t chunk = 256;
    if (chunk > length) {
      chunk = length;
    }
    // loop until the flash succeeds. We expect it to pass
    // first time, so this is paranoia
    while (!save_flash_nolib(bl, chunk, address)) {
    }
    length -= chunk;
    address += chunk;
    bl += chunk;
  }
}

int main(void)
{
  bl_clock_config();
  bl_timer_init();

  // don't risk erasing the bootloader if it already matches
  if (memcmp((const void*)MCU_FLASH_START, bl_image, sizeof(bl_image)) != 0) {
    // give 1.5s for debugger to attach
    delayMicroseconds(1500000);

    // do the flash
    flash_bootloader();
  }

  // and reset
  NVIC_SystemReset();

  return 0;
}
