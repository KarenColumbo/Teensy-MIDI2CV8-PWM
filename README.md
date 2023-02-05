# Teensy-MIDI2CV8 Version 0.1 - Still UNTESTED! Use at your own risk!
A naive take on a 8-voices polyphonic MIDI to CV program for Teensy 4.1. 

I intend to use it to feed analog VCOs, VCAs, ADSRs and VCFs with the produced signals.

The routines and functions present are the result of surfing the World Wide Web with unparalleled mulishness. I just cobbled together what looked reasonable and tried to fit it into a valid frame.

No idea yet if this monster is alive.

Should send

• 8 on/off gates (bool), 

• 8 note-on velocities (14 bit mapped), 

• channel pitchbend (14 bit), 

• channel modwheel (14 bit mapped) and 

• channel aftertouch (14 bit mapped) 

as PWM voltages at 19 GPIO output pins. 

Uses two Adafruit MCP 4728 boards with 5.0 volts Vdd reference to send 8 12-bit note CV voltages from 0V (MIDI note C1) to 5.0V (MIDI note C8).

Will likely need buffering and filtering.
