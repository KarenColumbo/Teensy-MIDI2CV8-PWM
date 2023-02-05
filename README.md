# Teensy-MIDI2CV8
8 voices polyphony MIDI to CV on a Teensy 4.1. Sends 8 on/off gates (bool), 8 note-on velocities (14 bit), channel pitchbend (14 bit mapped), channel modwheel (14 bit mapped) and channel aftertouch (14 bit mapped) as PWM voltages for use with "classic" analog VCOs. 

Uses two Adafruit MCP 4728 boards for 12 bit note CV voltages from 0V (MIDI note C1) to 5.0V (MIDI note C8).

Version 0.1 - UNTESTED!
