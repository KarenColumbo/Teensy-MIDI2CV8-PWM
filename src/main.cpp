// Language C/C++
// Board Teensy 4.1
#include <stdint.h>
#include <Arduino.h>
#include <MIDI.h>
#include <Adafruit_MCP4728.h>
#include <SoftwareSerial.h>

#define NUM_VOICES 8
#define MIDI_CHANNEL 1
#define PITCH_POS 2 // Pitchbend range in +/- benderValue
#define PITCH_NEG -2
#define CC_TEMPO 5

uint8_t midiTempo;
uint8_t midiController[10];
uint16_t benderValue = 0;
uint16_t eighthNoteDuration = 0;
uint16_t sixteenthNoteDuration = 0; 
bool susOn = false;
int arpIndex = 0;
int numArpNotes = 0;
int arpNotes[NUM_VOICES];
int octSwitch = 0;

// --------------------------------- Velocity Voltages 
const float veloVoltLin[128]={
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

const float veloVoltLog[128]={
1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 
5, 5, 11, 11, 15, 16, 16, 17, 21, 22, 23, 39, 41, 42, 
48, 49, 55, 57, 59, 66, 93, 96, 104, 106, 115, 118, 127, 130, 
139, 181, 185, 195, 206, 211, 222, 226, 238, 250, 309, 323, 337, 343, 
357, 371, 386, 393, 408, 496, 513, 521, 538, 556, 574, 592, 611, 629, 
741, 762, 783, 804, 826, 848, 870, 892, 915, 1054, 1078, 1103, 1129, 1154, 
1180, 1206, 1245, 1272, 1441, 1470, 1512, 1542, 1572, 1602, 1646, 1677, 1709, 1924, 
1958, 1992, 2041, 2075, 2125, 2161, 2196, 2248, 2500, 2539, 2593, 2633, 2689, 2729, 
2785, 2826, 2884, 3176, 3220, 3282, 3343, 3389, 3451, 3497, 3561, 3626, 3961, 4028
};

// ----------------------------- 12 bit DAC for 1V/oct C2-C7
const unsigned int noteVolt[61] = {
  0, 68, 137, 205, 273, 341, 410, 478, 546, 614, 683, 751, 
  819, 887, 956, 1024, 1092, 1160, 1229, 1297, 1365, 1433, 1502, 1570, 
  1638, 1706, 1775, 1843, 1911, 1979, 2048, 2116, 2184, 2252, 2321, 2389, 
  2457, 2525, 2594, 2662, 2730, 2798, 2867, 2935, 3003, 3071, 3140, 3208, 
  3276, 3344, 3413, 3481, 3549, 3617, 3686, 3754, 3822, 3890, 3959, 4027, 4095
  };

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// ------------------------------------- Voice buffer init 
struct Voice {
  unsigned long noteAge;
  uint8_t midiNote;
  bool noteOn;
  uint8_t velocity;
  uint16_t pitchBend;
  uint8_t channelPressure;
  uint8_t modulationWheel;
  uint8_t prevNote;
  uint16_t bendedNote;
};

Voice voices[NUM_VOICES];

void initializeVoices() {
  for (int i = 0; i < NUM_VOICES; i++) {
    voices[i].noteAge = 0;
    voices[i].midiNote = 0;
    voices[i].noteOn = false;
    voices[i].velocity = 0;
    voices[i].pitchBend = 0x2000;
    voices[i].channelPressure = 0;
    voices[i].modulationWheel = 0;
    voices[i].prevNote = 0;
    voices[i].bendedNote = 0x2000;
  }
}

// ------------------------------------------ Voice buffer subroutines 
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

int findVoice(uint8_t midiNote) {
  int foundVoice = -1;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].noteOn && voices[i].midiNote == midiNote) {
      foundVoice = i;
      break;
    }
  }
  return foundVoice;
}

void noteOn(uint8_t midiNote, uint8_t velocity) {
  int voice = findVoice(midiNote);
  if (voice == -1) {
    voice = findOldestVoice();
    voices[voice].prevNote = voices[voice].midiNote;
  }
  voices[voice].noteAge = millis();
  voices[voice].midiNote = midiNote;
  voices[voice].noteOn = true;
  voices[voice].velocity = velocity;
}

void noteOff(uint8_t midiNote) {
  int voice = findVoice(midiNote);
  if (voice != -1) {
    voices[voice].noteOn = false;
    voices[voice].velocity = 0;
  }
}

//-------------------------------- Fill Arpeggio buffer from oldest to youngest note
void fillArpNotes() {
  arpIndex = 0;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].noteOn) {
      arpNotes[arpIndex++] = voices[i].midiNote;
    }
  }
  numArpNotes = arpIndex;
  for (int i = 0; i < NUM_VOICES; i++) {
    for (int j = 0; j < arpIndex - 1; j++) {
      if (voices[i].noteAge < voices[j].noteAge) {
        int temp = arpNotes[j];
        arpNotes[j] = arpNotes[j + 1];
        arpNotes[j + 1] = temp;
      }
    }
  }
}

// ------------------------------------ Initialize DACs
Adafruit_MCP4728 dac1;
Adafruit_MCP4728 dac2;
Adafruit_MCP4728 dac3;
Adafruit_MCP4728 dac4;

// ----------------------------------------------- MAIN SETUP 
void setup() {
  dac1.begin(0x60);
  dac2.begin(0x61);
  dac1.begin(0x62);
  dac2.begin(0x63);

  for (int i = 0; i < NUM_VOICES; i++) {
    arpNotes[i] = -1;
  }

  // ****************** WARNING: Connect VDD to 5 volts!!! 
  // Wiring:
  // Teensy 4.1 --> DAC1, DAC2
  // Pin 16 (SCL) --> SCL
  // Pin 17 (SDA) --> SDA
  // Teensy 4.1 --> DAC3, DAC4
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
  pinMode(18, OUTPUT); // Controller 10
  pinMode(15, OUTPUT); // Controller 09
  pinMode(14, OUTPUT); // Controller 08
  pinMode(13, OUTPUT); // Controller 07
  pinMode(12, OUTPUT); // Controller 06
  pinMode(11, OUTPUT); // Controller 05
  pinMode(10, OUTPUT); // Controller 04
  pinMode(9, OUTPUT); // Controller 03
  pinMode(8, OUTPUT); // Controller 02
  pinMode(7, OUTPUT); // Controller 01
  pinMode(6, OUTPUT); // Modwheel out
  pinMode(5, OUTPUT); // Aftertouch out
  pinMode(4, OUTPUT); // Pitchbend out
}

//------------------------------------ MAIN LOOP 
void loop() {
  
  // ---------------------- Serial MIDI stuff 
  if (MIDI.read()) {

    // ------------------------Check for and buffer incoming Note On message
    if (MIDI.getType() == midi::NoteOn && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t midiNote = MIDI.getData1();
      uint8_t velocity = MIDI.getData2();
      if (velocity > 0) {
        noteOn(midiNote, velocity);
      } 
      if (velocity == 0 && susOn == false) {
        noteOff(midiNote);
      }
    }
    
    // ------------------ Check for and write incoming Pitch Bend, map bend factor 
    if (MIDI.getType() == midi::PitchBend && MIDI.getChannel() == MIDI_CHANNEL) {
      uint16_t pitchBend = MIDI.getData1() | (MIDI.getData2() << 7);
      int pitchBendPWM = map(pitchBend, 0, 16383, 0, 16383 << 2);
      benderValue = map(pitchBend, 0, 16383 << 2, PITCH_NEG, PITCH_POS);
      analogWrite(4, pitchBendPWM);
    }

    // ----------------------- Check for and write incoming Aftertouch 
    if (MIDI.getType() == midi::AfterTouchChannel && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t aftertouch = MIDI.getData1();
      int channelPressurePWM = map(aftertouch, 0, 127, 0, 8191 << 2);
      analogWrite(5, channelPressurePWM);
    }

    // ------------------------- Check for and write incoming Modulation Wheel 
    if (MIDI.getType() == midi::ControlChange && MIDI.getData1() == 1 && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t modulationWheel = MIDI.getData2();
      int modulationWheelPWM = map(modulationWheel, 0, 127, 0, 8191 << 2);
      analogWrite(6, modulationWheelPWM);
    }

    // ------------------------- Check for and write incoming MIDI tempo 
    if (MIDI.getType() == midi::ControlChange && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t ccNumber = MIDI.getData1();
      uint8_t ccValue = MIDI.getData2();
      if (ccNumber == CC_TEMPO) {
        midiTempo = ccValue;
      }
      eighthNoteDuration = (60 / midiTempo) * 1000 / 2;
      sixteenthNoteDuration = (60 / midiTempo) * 1000 / 4;
    }
    
    if (MIDI.getType() == midi::ControlChange && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t ccNumber = MIDI.getData1();
      uint8_t ccValue = MIDI.getData2();
      if (ccNumber >= 70 && ccNumber <= 79) {
        uint16_t mappedValue = map(ccValue, 0, 127, 0, 16383);
        switch (ccNumber) {
          case 79:
            analogWrite(18, mappedValue >> 7);
          break;
          case 78:
            analogWrite(15, mappedValue >> 7);
          break;
          case 77:
            analogWrite(14, mappedValue >> 7);
          break;
          case 76:
            analogWrite(13, mappedValue >> 7);
          break;
          case 75:
            analogWrite(12, mappedValue >> 7);
          break;
          case 74:
            analogWrite(11, mappedValue >> 7);
          break;
          case 73:
            analogWrite(10, mappedValue >> 7);
          break;
          case 72:
            analogWrite(9, mappedValue >> 7);
          break;
          case 71:
            analogWrite(8, mappedValue >> 7);
          break;
          case 70:
            analogWrite(7, mappedValue >> 7);
          break;
        }
      }
    }

    // ---------------------------- Read and store sustain pedal status
    if (MIDI.getType() == midi::ControlChange && MIDI.getData1() == 64 && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t sustainPedal = MIDI.getData2();
      if (sustainPedal > 63) {
         susOn = true;
      } else {
         susOn = false;
      }
    }

    // ----------------------- Write gates and velocity outputs, bend notes 
    for (int i = 0; i < NUM_VOICES; i++) {
      // Output gate
      digitalWrite(30 - i, voices[i].noteOn ? HIGH : LOW);
      voices[i].bendedNote = noteVolt[voices[0].midiNote] + (benderValue * 67.9);
      if (voices[i].bendedNote < 0) {
        voices[i].bendedNote = 0;
      }
      if (voices[i].bendedNote > 4095) {
        voices[i].bendedNote = 4095;
      }
    }
  }

  //-------------------------- Fill Arpeggio buffer
  fillArpNotes();

  // --------------------- Write velocity voltages to DAC boards, Vref = Vdd 
  dac3.setChannelValue(MCP4728_CHANNEL_A, veloVoltLin[voices[0].velocity]);
  dac3.setChannelValue(MCP4728_CHANNEL_B, veloVoltLin[voices[1].velocity]);
  dac3.setChannelValue(MCP4728_CHANNEL_C, veloVoltLin[voices[2].velocity]);
  dac3.setChannelValue(MCP4728_CHANNEL_D, veloVoltLin[voices[3].velocity]);
  dac4.setChannelValue(MCP4728_CHANNEL_A, veloVoltLin[voices[4].velocity]);
  dac4.setChannelValue(MCP4728_CHANNEL_B, veloVoltLin[voices[5].velocity]);
  dac4.setChannelValue(MCP4728_CHANNEL_C, veloVoltLin[voices[6].velocity]);
  dac4.setChannelValue(MCP4728_CHANNEL_D, veloVoltLin[voices[7].velocity]);
  
  // -------------------- Write bended note frequency voltages to DAC boards, , Vref = Vdd 
  dac1.setChannelValue(MCP4728_CHANNEL_A, voices[0].bendedNote);
  dac1.setChannelValue(MCP4728_CHANNEL_B, voices[1].bendedNote);
  dac1.setChannelValue(MCP4728_CHANNEL_C, voices[2].bendedNote);
  dac1.setChannelValue(MCP4728_CHANNEL_D, voices[3].bendedNote);
  dac2.setChannelValue(MCP4728_CHANNEL_A, voices[4].bendedNote);
  dac2.setChannelValue(MCP4728_CHANNEL_B, voices[5].bendedNote);
  dac2.setChannelValue(MCP4728_CHANNEL_C, voices[6].bendedNote);
  dac2.setChannelValue(MCP4728_CHANNEL_D, voices[7].bendedNote);
}
