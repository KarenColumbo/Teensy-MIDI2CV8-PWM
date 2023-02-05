# Teensy-MIDI2CV8 Version 0.1 - Still UNTESTED! Use at your own risk!
A naive take on a 8-voices polyphonic MIDI to CV program for Teensy 4.1. 

I intend to use it to feed analog VCOs, VCAs, ADSRs and VCFs with the produced signals.

Should send
• 8 on/off gates (bool), 
• 8 note-on velocities (14 bit mapped), 
• channel pitchbend (14 bit), 
• channel modwheel (14 bit mapped) and 
• channel aftertouch (14 bit mapped) 
as PWM voltages for use with "classic" analog VCOs. 

Uses two Adafruit MCP 4728 boards to send 12 bit note CV voltages from 0V (MIDI note C1) to 5.0V (MIDI note C8).


