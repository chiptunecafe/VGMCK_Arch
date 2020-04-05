# VGMCK_Arch
A fork of zzo38's VGMCK MML to VGM Compiler, doubling as an archive of the original version.

# Currently Supported Sound Chips
* SN76489 PSG (SMS/GG, BBC Micro, PCjr, Tandy)
* OPL2/YM3812
* HuC6280 (PC-Engine/TurboGrafx-16)
* RP2A03 (Famicom/NES)
* LR35902 (GameBoy)
* T6W28 (NeoGeo Pocket + Color)
* QSound
* OPLL/YM2413
* OPN2/YM2612
* Atari PoKEY
* OPL3/YMF262
* General Instruments AY-3-8910
* AY8930 (supported by the VGM format, but not by the emulator)
* OPL4/YMF278B

# Features
* Detune
* Transpose
* Text macros
* Track questioning
* Custom musical scales with up to ten letters
* Custom pitches for the notes of the scale
* Many (not all) chips can be doubled (as specified in VGM specification)
* Full GD3 support (including Unicode)
* Change the clock rate and options of any chips
* Change the frame rate
* Auto arpeggio
* Local desynchronized loops
* Entire song loops (which cannot be desynchronized, though)
* Sample list macros
* Auto track switching
* Direct register write
* Direct VGM write
* Fast forward
* Portamento
* And more...

# To Do
* Figure out the compilation dependencies
* Fix the Eof offset
