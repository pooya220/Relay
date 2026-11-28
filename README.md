# Relay controller

STM32F1 firmware for the two-relay controller with Ethernet, OLED, EEPROM and W25 external flash.

## Sketches

- `Relay.ino` — main controller firmware.
- `write_to_w25/write_to_w25.ino` — writes the embedded web pages to the W25 flash.
- `factory_erase/factory_erase.ino` — utility sketch that erases the W25 flash. Use it only when you intend to clear that chip.

`build_opt.h` keeps the sketch's compiler options.

## Libraries and board

Use an STM32F1 board/core and install Ethernet, Adafruit GFX Library, and Adafruit SSD1306. SPI, EEPROM and Wire are supplied by the board core.

Upload the W25 writer when you need to update the stored web pages, then upload `Relay.ino` for normal operation.

## Security

The main sketch has default/fallback web-login behavior. Change the credentials before exposing the controller to a network.
