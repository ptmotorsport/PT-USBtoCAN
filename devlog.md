__01/04/2026:__
* Set up hardware filtering for whitelist
* Set up software filtering for blacklist
* Debugged USBtoCAN GUI (solved empty list & overlapping entries)
* Currently debugging whitelist/blacklist error

__15/04/2026:__
* Continued debugging whitelist/blacklist error
  * Tried different filter settings
  * Tried different function to update filter
* Added 'reset file count' and 'apply changes' button

__16/04/2026:__
* Fixed whitelist/blacklist error (trying to receive CAN messages while changing filters)
* Fixed EEPROM values not updating
* Started writing Arduino code for Neopixel lights on USBtoCAN

__29/04/2026:__
* Soldered remaining USBtoCAN boards
* Created bootloader file for initial USBtoCAN EEPROM values
* Created KiCAD schematic & pcb for Volvo tuning harness

__01/05/2026:__
* Finished KiCAD pcb to order from JLCPCB (fixed R/C values & exported to .Gerber)
* Finished writing Arduino code for Neopixel lights on USBtoCAN
* Finished latest version of the USBtoCAN project (bootloader, GUI, and datalogger)

__05/05/2026:__
* Fixed whitelist always on at start error (added check for filterState)
* Designed 12-position rotary switch PCB
* Debugged and fixed USBtoCAN boards (SD card readers & NeoPixels)
* Took one USBtoCAN for uni motorsports team to test

__08/05/2026:__
* Designed 8-position (circle) and 8-position (square) switch PCBs
* Started working on configuration files for DingoPDM Configurator on PTPDSS
* Fixed tolerance issues on rotary switch boards

__14/05/2026:__
* Started work on designing buck-boost converter pcb
* Looked at porting Dingo_FW over to renesas chips to work with ptpdss boards

__15/05/2026:__
* Implemented reverse polarity protection, load dump protection, etc. for buck-boost converter
* Continued porting Dingo_FW to renesas
* Looked at using CAN msgs to communicate between Dingo and PTPDSS
