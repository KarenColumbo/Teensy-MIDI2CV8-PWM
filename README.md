# Teensy-MIDI2CV8 Version a0.6
## A naive take on an 8-voices polyphonic MIDI to CV program for Teensy 4.1. 
### ********** Still UNTESTED! Do NOT use, this is just a "travelogue"! ***********

## History:

### Mar 3rd '23
- Read a tiny wee bit into that "PDM" matter. Concocted some test routines by fiddling with fragments i found. Still ... no time for anything besides this gig this month. But anything helps when facing this "chip crisis" and unobtainable 16 bit DACs. "Clean" CVs with simple PWM pins through simple R/C low-pass filters ... that'd be the ticket ...

### Feb 26th '23

- Got sidetracked with another go: I decided to put out the core triangle waves for an analog VCO through a couple of AD9833 DDS ICs (easily available as boards at the usual mass product websites). Take a peek at that repo: https://github.com/KarenColumbo/Teensy-DCO-progress
- This works, polyphonic portamento included, so I will stick with it for a while.
- Will include the good things that work there into this repo here in the future!

### Feb 14th '23

- Outsourcing the "modules" to https://github.com/KarenColumbo/Teensy-MIDICV8_testlab for extensive testing, then reassembling them here.

- Sustain logic working! Had approx. a gazillion iterations/variations including "sustain buffer" and flags and subroutines, even thought about some interrupt story. In the end it was just a matter of adding a variable "keyDown" that's independend from note on/off events. 

### Feb 13th '23

- Debugging piece by piece in another secret repo :) MIDI buffer note stealing and sustain logic working perfectly. Will swap the "modules" in piece by piece.

### Feb 12th '23

- Some major change: I decided to rewrite the note frequencies output to output a clock and a control voltage signal to control a DCO à la Juno 106. We'll see :)
- Had to disable the Load/Save routines, though - not enough pins left, maybe outsource velocity CVs to another 23017, they're 8 bit after all.

### Feb 11th '23

- Commented out "Arpeggiator" - not sure if I'm on the right track with my idea - gotta jot it down first.
- Built in a subroutine that saves/loads CC values 70-83 (chained in another MCP4728) to/from EEPROM. Included "Bounce2.h", reads momentary load and save switch, threshold: 3 sec.
- Some code "prettifying", yet there's still a long and winding road ahead of me …

### Feb 10th '23

- Spliced in an Adafruit TCA9548A I2C multiplexer to take care of the MCP4728s and the MCP23017 without having to individually flash addresses to the 4728s.

### Feb 9th '23

- Since I had used up ALL of the GPIO pins, I decided to put one of my MCP23017s to use. I chained them to the 4 DACs, so I have 8 GPIO pins free for eventual switcheroo or controleroo.

### Feb 8th '23

- Reconfigured GPIO stuff and switched GPIOs with DACs - let's see if I can get this right with analogWriteFrequency and the right bit number. May have to switch again if this fails.
- Heavy debugging going on in my chaotic n00b code. Always some crap to weed out.

### Feb 7th '23

- Defined MIDI controllers 70-79 to control analog stuff like ADSR, VCF and such from keyboard knobs instead of a gazillion analog pots. Putting them out at MCP4728 DACs to spare some level shifters.

### Feb 6th '23:

- Set up first beginnings of a simple Arpeggiator. Has its own array, note order is note age, so I don't need patterns except maybe up or down.
- Simple sustain pedal routine. If velocity == 0, but pedal is on, note still on.

## Features:

I intend to use it to feed analog VCOs, VCAs, ADSRs and VCFs with the produced signals.

The routines and functions present are the result of months of incessantly surfing the World Wide Web with unparalleled mulishness. I just cobbled together what looked reasonable to me wee n00b eye and tried to fit it into a valid frame.

No idea yet if this monster is alive.

It should read incoming serial MIDI data (UART) into a voice buffer array with oldest note and note-off stealing, utilizing the millis() function as note age stamp.

After some calculations it should send

- 8 note frequencies with software pitchbend (purportedly 14 bits by setting analogWriteFrequency and analogWriteResolution according to https://www.pjrc.com/teensy/td_pulse.html)
- 8 note velocities 
- pitchbender CV

as PWM voltages at 17 GPIO output pins and

- 8 on/off gates to MCP23017 

It uses four Adafruit MCP 4728 boards with 5.0 volts Vdd reference to send 

- channel modwheel (14 bits -> 12 bits)
- channel aftertouch (8 bits -> 12 bits)
- MIDI controllers 70–83 (8 bits -> 12 bits)

MCP boards are addressed by an Adafruit TCA9548A I2C multiplexer board.

Since setting PWM frequencies manually purportedly does higher precision I could probably get away with first order low-pass filtering. Gotta test this. If it doesn't work out I'll go the "bit spray" way. 

MIDI pitch bend is sent out as a CV additionally to do stuff with it - VCF reso comes to mind.

Thought about implementing portamento (like PolyKit did with his DCO-8), but thinking about it hurts my brain - not my forté, worst case I just steal it from PolyKit :) Nah, kidding. I think it sounds better in hardware anyway.

There are the roots of a Arpeggiator idea in the code. But it's time to get the whole monster alive, first.

###### If you want to buy me a beer to get going (maybe my struggling helps other n00bs in need), you could do so here and earn my undying gratitude: https://www.buymeacoffee.com/synthiy
