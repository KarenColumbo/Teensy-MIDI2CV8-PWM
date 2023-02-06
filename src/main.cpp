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

// --------------------------------- Velocity Voltages -------------------------------------------
const float veloVolt[128]={
  0, 32, 64, 96, 128, 160, 192, 224, 
  256, 288, 320, 352, 384, 416, 448, 480, 
  512, 544, 576, 608, 640, 672, 704, 736, 
  768, 800, 832, 864, 896, 928, 960, 992, 
  1024, 1056, 1088, 1120, 1152, 1184, 1216, 1248, 
  1280, 1312, 1344, 1376, 1408, 1440, 1472, 1504, 
  1536, 1568, 1600, 1632, 1664, 1696, 1728, 1760, 
  1792, 1824, 1856, 1888, 1920, 1952, 1984, 2016, 
  2048, 2080, 2112, 2144, 2176, 2208, 2240, 2272, 
  2304, 2336, 2368, 2400, 2432, 2464, 2496, 2528, 
  2560, 2592, 2624, 2656, 2688, 2720, 2752, 2784, 
  2816, 2848, 2880, 2912, 2944, 2976, 3008, 3040, 
  3072, 3104, 3136, 3168, 3200, 3232, 3264, 3296, 
  3328, 3360, 3392, 3424, 3456, 3488, 3520, 3552, 
  3584, 3616, 3648, 3680, 3712, 3744, 3776, 3808, 
  3840, 3872, 3904, 3936, 3968, 4000, 4032, 4064
};

//const int MIDI_NOTE_C1 = 24;
//const int NUM_NOTES = 90;
//const float SEMITONE_PITCH_CHANGE = 0.055;
//const float VREF = 5.0;
//const int DAC_BITS = 12;
//const float A4_FREQUENCY = 440.0;
//void fill_note_freq_array(unsigned int noteVolt[], float tuning_frequency) {
//  float semitone_pitch_change = (tuning_frequency / A4_FREQUENCY) * SEMITONE_PITCH_CHANGE;
//  for (int i = 0; i < NUM_NOTES; i++) {
//    float vcontrol = i * semitone_pitch_change;
//    noteVolt[i] = (unsigned int)round(vcontrol / VREF * (pow(2, DAC_BITS) - 1));
//  }
//}

const unsigned int noteVolt[90] = {
  0, 45, 90, 135, 180, 225, 270, 315, 
  360, 405, 450, 495, 541, 586, 631, 676, 
  721, 766, 811, 856, 901, 946, 991, 1036, 
  1081, 1126, 1171, 1216, 1261, 1306, 1351, 1396, 
  1441, 1486, 1532, 1577, 1622, 1667, 1712, 1757, 
  1802, 1847, 1892, 1937, 1982, 2027, 2072, 2117, 
  2162, 2207, 2252, 2297, 2342, 2387, 2432, 2477, 
  2523, 2568, 2613, 2658, 2703, 2748, 2793, 2838, 
  2883, 2928, 2973, 3018, 3063, 3108, 3153, 3198, 
  3243, 3288, 3333, 3378, 3423, 3468, 3514, 3559, 
  3604, 3649, 3694, 3739, 3784, 3829, 3874, 3919, 
  3964, 4009
  };

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

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
