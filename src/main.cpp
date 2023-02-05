// Language C/C++
// Board Teensy 4.1
#include <stdint.h>
#include <Arduino.h>
#include <MIDI.h>
#include <Adafruit_MCP4728.h>
#include <SoftwareSerial.h>

#define NUM_VOICES 8
#define MIDI_CHANNEL 1
#define PITCH_BEND_RANGE = 2; // Pitchbend range in +/- semitones
const float noteFreq[85]={ // MIDI notes from C1 to C8
  32.7032, 34.6478, 36.7081, 38.8909, 41.2034, 43.6535, 46.2493, 48.9994, 51.9131, 55.0000, 58.2705, 61.7354, 
  65.4064, 69.2957, 73.4162, 77.7817, 82.4069, 87.3071, 92.4986, 97.9989, 103.8262, 110.0000, 116.5409, 123.4708, 
  130.8128, 138.5913, 146.8324, 155.5635, 164.8138, 174.6141, 184.9972, 195.9977, 207.6523, 220.0000, 233.0819, 246.9417, 
  261.6256, 277.1826, 293.6648, 311.1270, 329.6276, 349.2282, 369.9944, 391.9954, 415.3047, 440.0000, 466.1638, 493.8833, 
  523.2511, 554.3653, 587.3295, 622.2540, 659.2551, 698.4565, 739.9888, 783.9909, 830.6094, 880.0000, 932.3275, 987.7666, 
  1046.5023, 1108.7305, 1174.6591, 1244.5079, 1318.5102, 1396.9129, 1479.9777, 1567.9817, 1661.2188, 1760.0000, 1864.6550, 1975.5332, 
  2093.0045, 2217.4610, 2349.3181, 2489.0159, 2637.0205, 2793.8259, 2959.9554, 3135.9635, 3322.4376, 3520.0000, 3729.3100, 3951.0664,
  4186.0090
};

const float veloVolt[128]={
  0.000000, 0.039063, 0.078125, 0.117188, 0.156250, 0.195313, 0.234375, 0.273438,
  0.312500, 0.351563, 0.390625, 0.429688, 0.468750, 0.507813, 0.546875, 0.585938,
  0.625000, 0.664063, 0.703125, 0.742188, 0.781250, 0.820313, 0.859375, 0.898438,
  0.937500, 0.976563, 1.015625, 1.054688, 1.093750, 1.132813, 1.171875, 1.210938,
  1.250000, 1.289063, 1.328125, 1.367188, 1.406250, 1.445313, 1.484375, 1.523438,
  1.562500, 1.601563, 1.640625, 1.679688, 1.718750, 1.757813, 1.796875, 1.835938,
  1.875000, 1.914063, 1.953125, 1.992188, 2.031250, 2.070313, 2.109375, 2.148438,
  2.187500, 2.226563, 2.265625, 2.304688, 2.343750, 2.382813, 2.421875, 2.460938,
  2.500000, 2.539063, 2.578125, 2.617188, 2.656250, 2.695313, 2.734375, 2.773438,
  2.812500, 2.851563, 2.890625, 2.929688, 2.968750, 3.007813, 3.046875, 3.085938,
  3.125000, 3.164063, 3.203125, 3.242188, 3.281250, 3.320313, 3.359375, 3.398438,
  3.437500, 3.476563, 3.515625, 3.554688, 3.593750, 3.632813, 3.671875, 3.710938,
  3.750000, 3.789063, 3.828125, 3.867188, 3.906250, 3.945313, 3.984375, 4.023438,
  4.062500, 4.101563, 4.140625, 4.179688, 4.218750, 4.257813, 4.296875, 4.335938,
  4.375000, 4.414063, 4.453125, 4.492188, 4.531250, 4.570313, 4.609375, 4.648438
};

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

float noteVolt[85];

void FillNoteVoltArray(){ // Calculate 12 bit DAC voltages
  noteVolt[0] = 0.0;
  for (int i = 1; i < 85; i++) {
    float frequency = noteFreq[i];
    int twelveBitVoltage = 4095 * (log2(frequency) - log2(32.7032));
    noteVolt[i] = (float)twelveBitVoltage/4095.0 * 5.0; // DAC Vref = 5.0 volts!
  }
}

// ------------------------------------- Voice buffer init --------------------------------------------------------------

struct Voice {
  unsigned long noteAge;
  uint8_t noteNumber;
	bool noteOn;
  uint8_t velocity;
  uint16_t pitchBend;
  uint8_t channelPressure;
  uint8_t modulationWheel;
  uint8_t prevNoteNumber;
};

Voice voices[NUM_VOICES];

void initializeVoices() {
  for (int i = 0; i < NUM_VOICES; i++) {
    voices[i].noteAge = 0;
    voices[i].noteNumber = 0;
    voices[i].noteOn = false;
    voices[i].velocity = 0;
    voices[i].pitchBend = 0x2000;
    voices[i].channelPressure = 0;
    voices[i].modulationWheel = 0;
    voices[i].prevNoteNumber = 0;
  }
}

// ------------------------------------------ Voice buffer subroutines --------------------------------------------------------

int findOldestVoice() {
  int oldestVoice = 0;
  unsigned long oldestAge = 0xFFFFFFFF;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (!voices[i].noteOn && voices[i].noteAge < oldestAge) {
      oldestVoice = i;
      oldestAge = voices[i].noteAge;
    }
  }
  return oldestVoice;
}

int findVoice(uint8_t noteNumber) {
  int foundVoice = -1;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].noteOn && voices[i].noteNumber == noteNumber) {
      foundVoice = i;
      break;
    }
  }
  return foundVoice;
}

void noteOn(uint8_t noteNumber, uint8_t velocity) {
  int voice = findVoice(noteNumber);
  if (voice == -1) {
    voice = findOldestVoice();
    voices[voice].prevNoteNumber = voices[voice].noteNumber;
  }
  voices[voice].noteAge = millis();
  voices[voice].noteNumber = noteNumber;
  voices[voice].noteOn = true;
  voices[voice].velocity = velocity;
}

void noteOff(uint8_t noteNumber) {
  int voice = findVoice(noteNumber);
  if (voice != -1) {
    voices[voice].noteOn = false;
    voices[voice].velocity = 0;
  }
}

Adafruit_MCP4728 dac1;
Adafruit_MCP4728 dac2;
Adafruit_MCP4728 dac3;
Adafruit_MCP4728 dac4;

// ----------------------------------------------- MAIN SETUP ------------------------------------------
void setup() {
  dac1.begin(0x60);
  dac2.begin(0x61);
  dac1.begin(0x62);
  dac2.begin(0x63);
 

  // ****************** WARNING: Connect VDD to 5 volts!!! **********************
  // Wiring:
  // Teensy 4.1 --> DAC1
  // Pin 16 (SCL) --> SCL
  // Pin 17 (SDA) --> SDA
  // Teensy 4.1 --> DAC2
  // Pin 20 (SCL1) --> SCL
  // Pin 21 (SDA1) --> SDA
  // Initialize I2C communication
  Wire.begin(400000);
  
  // Set 14 bits for bender, modwheel, and aftertouch
  analogWriteResolution(14);
  pinMode(30, OUTPUT); // Gate 08
  pinMode(29, OUTPUT); // Gate 07
  pinMode(28, OUTPUT); // Gate 06
  pinMode(27, OUTPUT); // Gate 05
  pinMode(26, OUTPUT); // Gate 04
  pinMode(25, OUTPUT); // Gate 03
  pinMode(24, OUTPUT); // Gate 02
  pinMode(23, OUTPUT); // Gate 01
  pinMode(6, OUTPUT); // Modwheel out
  pinMode(5, OUTPUT); // Aftertouch out
  pinMode(4, OUTPUT); // Pitchbend out
}

//------------------------------------ MAIN LOOP ----------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------

void loop() {
  
  // ---------------------- Serial MIDI stuff --------------------
  if (MIDI.read()) {

    // ------------------------Check for and buffer incoming Note On message
    if (MIDI.getType() == midi::NoteOn && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t noteNumber = MIDI.getData1();
      uint8_t velocity = MIDI.getData2();
      if (velocity > 0) {
        noteOn(noteNumber, velocity);
      } else {
        noteOff(noteNumber);
      }
    }

    // ------------------------Check for and write incoming Pitch Bend 
    if (MIDI.getType() == midi::PitchBend && MIDI.getChannel() == MIDI_CHANNEL) {
      uint16_t pitchBend = MIDI.getData1() | (MIDI.getData2() << 7);
      int pitchBendPWM = map(pitchBend, 0, 16383, 0, 16383 << 2);
      analogWrite(4, pitchBendPWM);
    }

    // -----------------------Check for and write incoming Aftertouch 
    if (MIDI.getType() == midi::AfterTouchChannel && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t aftertouch = MIDI.getData1();
      int channelPressurePWM = map(aftertouch, 0, 127, 0, 8191 << 2);
      analogWrite(5, channelPressurePWM);
    }

    // -------------------------Check for and write incoming Modulation Wheel 
    if (MIDI.getType() == midi::ControlChange && MIDI.getData1() == 1 && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t modulationWheel = MIDI.getData2();
      int modulationWheelPWM = map(modulationWheel, 0, 127, 0, 8191 << 2);
      analogWrite(6, modulationWheelPWM);
    }

    // ----------------------- Write gates and velocity outputs ----------------
    for (int i = 0; i < NUM_VOICES; i++) {
      // Output gate
      digitalWrite(30 - i, voices[i].noteOn ? HIGH : LOW);
    }
  }

  // --------------------- Write velocity voltages to DAC boards -----------------
  dac3.setChannelValue(MCP4728_CHANNEL_A, veloVolt[voices[0].velocity], MCP4728_VREF_VDD);
  dac3.setChannelValue(MCP4728_CHANNEL_B, veloVolt[voices[1].velocity], MCP4728_VREF_VDD);
  dac3.setChannelValue(MCP4728_CHANNEL_C, veloVolt[voices[2].velocity], MCP4728_VREF_VDD);
  dac3.setChannelValue(MCP4728_CHANNEL_D, veloVolt[voices[3].velocity], MCP4728_VREF_VDD);
  dac4.setChannelValue(MCP4728_CHANNEL_A, veloVolt[voices[4].velocity], MCP4728_VREF_VDD);
  dac4.setChannelValue(MCP4728_CHANNEL_B, veloVolt[voices[5].velocity], MCP4728_VREF_VDD);
  dac4.setChannelValue(MCP4728_CHANNEL_C, veloVolt[voices[6].velocity], MCP4728_VREF_VDD);
  dac4.setChannelValue(MCP4728_CHANNEL_D, veloVolt[voices[7].velocity], MCP4728_VREF_VDD);
  
  // -------------------- Write note frequency voltages to DAC boards ---------------------
  dac1.setChannelValue(MCP4728_CHANNEL_A, noteVolt[voices[0].noteNumber], MCP4728_VREF_VDD);
  dac1.setChannelValue(MCP4728_CHANNEL_B, noteVolt[voices[1].noteNumber], MCP4728_VREF_VDD);
  dac1.setChannelValue(MCP4728_CHANNEL_C, noteVolt[voices[2].noteNumber], MCP4728_VREF_VDD);
  dac1.setChannelValue(MCP4728_CHANNEL_D, noteVolt[voices[3].noteNumber], MCP4728_VREF_VDD);
  dac2.setChannelValue(MCP4728_CHANNEL_A, noteVolt[voices[4].noteNumber], MCP4728_VREF_VDD);
  dac2.setChannelValue(MCP4728_CHANNEL_B, noteVolt[voices[5].noteNumber], MCP4728_VREF_VDD);
  dac2.setChannelValue(MCP4728_CHANNEL_C, noteVolt[voices[6].noteNumber], MCP4728_VREF_VDD);
  dac2.setChannelValue(MCP4728_CHANNEL_D, noteVolt[voices[7].noteNumber], MCP4728_VREF_VDD);
}
