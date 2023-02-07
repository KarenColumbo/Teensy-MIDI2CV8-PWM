# Teensy-MIDI2CV8 Version 0.1
## Still UNTESTED! Use at your own risk!
### A naive take on an 8-voices polyphonic MIDI to CV program for Teensy 4.1. 

## News:

### Feb 7th '23

- Defined MIDI controllers 70-79 to control analog stuff like ADSR, VCF and such from keyboard knobs instead of a gazillion analog pots. As long as there are enough Teensy GPIOs to write those values to ...

### Feb 6th '23:

- Set up first beginnings of a simple Arpeggiator. Has its own array, note order is note age, so I don't need patterns except maybe up or down.
- Simple sustain pedal routine. If velocity == 0, but pedal is on, note still on.

## Features:

I intend to use it to feed analog VCOs, VCAs, ADSRs and VCFs with the produced signals.

The routines and functions present are the result of months of incessantly surfing the World Wide Web with unparalleled mulishness. I just cobbled together what looked reasonable to me wee n00b eye and tried to fit it into a valid frame.

No idea yet if this monster is alive.

It should read incoming serial MIDI data (UART) into a voice buffer array with oldest note and note-off stealing, utilizing the millis() function as note age stamp.

After some calculations it should send

- 8 on/off gates (bool), 
- channel pitchbend (14 bit), 
- channel modwheel (14 bit mapped), and 
- channel aftertouch (14 bit mapped) 

as PWM voltages at 11 GPIO output pins. 


It also uses four Adafruit MCP 4728 boards with 5.0 volts Vdd reference to send 

- 8 12-bit note CV voltages from 0V (MIDI note C2) to 5.0V (MIDI note C7), and 
- 8 12-bit note-on velocities between 0.0 and 5.0-ish volts. I probably do some velocity curving later on.

Pitchbend, modwheel and aftertouch PWM will likely need buffering and effective low-pass filtering.

MIDI pitch bend is translated into 12-bit DAC voltage and added to/subtracted from the note frequencies. At 12 bit there's likely to be audible "digital steps", so we'll see, I feed it via CV In pin to the respective VCO anyway - and maybe VCF cut-off. Could be I need a fifth MCP4728 for the controller outputs - don't know how crappy the PWM stuff looks on scope.

Thought about implementing portamento (like PolyKit did with his DCO-8), but ... 12 bit. Can always do that in discrete hardware which WILL sound better anyway.

If you want to buy me a beer to get going (maybe my struggling helps other n00bs in need), you could do so here and earn my undying gratitude: https://www.buymeacoffee.com/synthiy
