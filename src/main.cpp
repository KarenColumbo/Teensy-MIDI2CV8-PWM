//*********************** https://www.pjrc.com/teensy/td_pulse.html ********************************
// Language C/C++
// Board Teensy 4.1
#include <stdint.h>
#include <Arduino.h>
#include <MIDI.h>
#include <Adafruit_MCP4728.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

#define NUM_VOICES 8
#define MIDI_CHANNEL 1
#define PITCH_POS 2 // Pitchbend range in +/- benderValue
#define PITCH_NEG -2
#define CC_TEMPO 5
#define A4 440
#define DAC_ADDRESS1 0x60
#define DAC_ADDRESS2 0x61
#define DAC_ADDRESS3 0x62
#define MCP_ADDRESS 0x20

#define GATE_01 0
#define GATE_02 1
#define GATE_03 2
#define GATE_04 3
#define GATE_05 4
#define GATE_06 5
#define GATE_07 6
#define GATE_08 7

uint8_t midiTempo;
uint8_t midiController[10];
uint16_t benderValue = 0;
bool susOn = false;
int arpIndex = 0;
int numArpNotes = 0;
int arpNotes[NUM_VOICES];
uint16_t eighthNoteDuration = 0;
uint16_t sixteenthNoteDuration = 0; 

// --------------------------------- 12 bit Velocity Voltages - linear 
const float veloVoltLin[128]={
  0, 128, 256, 384, 512, 640, 768, 896, 1024, 1152, 1280, 1408, 1536, 1664, 1792, 1920, 2048, 2176, 2304, 2432, 2560, 2688, 2816, 2944, 3072, 3200, 3328, 3456, 3584, 3712, 3840, 3968, 4096, 4224, 4352, 4480, 4608, 4736, 4864, 4992, 5120, 5248, 5376, 5504, 5632, 5760, 5888, 6016, 6144, 6272, 6400, 6528, 6656, 6784, 6912, 7040, 7168, 7296, 7424, 7552, 7680, 7808, 7936, 8064, 8192, 8320, 8448, 8576, 8704, 8832, 8960, 9088, 9216, 9344, 9472, 9600, 9728, 9856, 9984, 10112, 10240, 10368, 10496, 10624, 10752, 10880, 11008, 11136, 11264, 11392, 11520, 11648, 11776, 11904, 12032, 12160, 12288, 12416, 12544, 12672, 12800, 12928, 13056, 13184, 13312, 13440, 13568, 13696, 13824, 13952, 14080, 14208, 14336, 14464, 14592, 14720, 14848, 14976, 15104, 15232, 15360, 15488, 15616, 15744, 15872, 16000, 16128, 16256
};

// ----------------------------- MIDI note frequencies C1-C7
float midiNoteFrequency [73] = {
  32.7032, 34.6478, 36.7081, 38.8909, 41.2034, 43.6535, 46.2493, 48.9994, 51.9131, 55, 58.2705, 61.7354, 
  65.4064, 69.2957, 73.4162, 77.7817, 82.4069, 87.3071, 92.4986, 97.9989, 103.826, 110, 116.541, 123.471, 
  130.813, 138.591, 146.832, 155.563, 164.814, 174.614, 184.997, 195.998, 207.652, 220, 233.082, 246.942, 
  261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995, 415.305, 440, 466.164, 493.883, 
  523.251, 554.365, 587.33, 622.254, 659.255, 698.456, 739.989, 783.991, 830.609, 880, 932.328, 987.767, 
  1046.5, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760, 1864.66, 1975.53, 2093
};

// ----------------------------- 14 bit note frequency voltages C1-C7
const unsigned int noteVolt[73] = {
  0, 15, 32, 49, 68, 87, 108, 130, 153, 177, 203, 231, 
  260, 291, 324, 358, 395, 434, 476, 519, 566, 615, 667, 722, 
  780, 842, 908, 977, 1051, 1129, 1211, 1299, 1391, 1489, 1593, 1704, 
  1820, 1944, 2075, 2214, 2361, 2517, 2682, 2857, 3043, 3239, 3447, 3667, 
  3901, 4148, 4411, 4688, 4982, 5294, 5625, 5974, 6345, 6738, 7154, 7595, 
  8062, 8557, 9081, 9637, 10225, 10849, 11509, 12209, 12950, 13736, 14568, 15450, 16383
  };

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
  uint16_t bentNote;
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
    voices[i].bentNote = 0x2000;
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
Adafruit_MCP23X17 mcp;

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// ******************************************************************************************************
// ************************************************************************ MAIN SETUP 
void setup() {
  for (int i = 0; i < NUM_VOICES; i++) {
    arpNotes[i] = -1;
  }

  // ****************** WARNING: Connect DACs VDD to 5 volts!!! 
  
  // ****************** DAC Wiring:
  // Teensy 4.1 --> DAC1, DAC2
  // Pin 16 (SCL) --> SCL
  // Pin 17 (SDA) --> SDA
  
  // ****************** MCP Wiring:
    // Pin 24 (SCL2) --> MCP SCL
  // Pin 25 (SDA2) --> MCP SDA
  // Initialize I2C communication
  dac1.begin(DAC_ADDRESS1);
  dac2.begin(DAC_ADDRESS2);
  dac3.begin(DAC_ADDRESS3);
  mcp.begin_I2C(MCP_ADDRESS);
  Wire.begin(400000);
  
  // Set 14 bits Hardware PWM for pitchbender and 8 note voltage outputs
  analogWriteResolution(14);
  pinMode(2, OUTPUT); // Note 01
  analogWriteFrequency(2, 9155.27);
  pinMode(3, OUTPUT); // Note 02
  analogWriteFrequency(3, 9155.27);
  pinMode(4, OUTPUT); // Note 03
  analogWriteFrequency(4, 9155.27);
  pinMode(5, OUTPUT); // Note 04
  analogWriteFrequency(5, 9155.27);
  pinMode(6, OUTPUT); // Note 05
  analogWriteFrequency(6, 9155.27);
  pinMode(9, OUTPUT); // Note 06
  analogWriteFrequency(7, 9155.27);
  pinMode(22, OUTPUT); // Note 07
  analogWriteFrequency(22, 9155.27);
  pinMode(23, OUTPUT); // Note 08
  analogWriteFrequency(23, 9155.27);
  
  pinMode(10, OUTPUT); // Velocity 01
  pinMode(11, OUTPUT); // Velocity 02
  pinMode(12, OUTPUT); // Velocity 03
  pinMode(13, OUTPUT); // Velocity 04
  pinMode(14, OUTPUT); // Velocity 05
  pinMode(15, OUTPUT); // Velocity 06
  pinMode(18, OUTPUT); // Velocity 07
  pinMode(19, OUTPUT); // Velocity 08

  pinMode(33, OUTPUT); // Pitchbender
  analogWriteFrequency(33, 9155.27);

  mcp.pinMode(0, OUTPUT);
  mcp.pinMode(1, OUTPUT);
  mcp.pinMode(2, OUTPUT);
  mcp.pinMode(3, OUTPUT);
  mcp.pinMode(4, OUTPUT);
  mcp.pinMode(5, OUTPUT);
  mcp.pinMode(6, OUTPUT);
  mcp.pinMode(7, OUTPUT);
}

// *****************************************************************************************************
//******************************************** MAIN LOOP *********************************************** 
void loop() {
  
  // ---------------------- Read ------------------------------------------------------------------------ 
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
      benderValue = map(pitchBend, 0, 16383, PITCH_NEG, PITCH_POS);
      analogWrite(33, benderValue);
    }

    // ----------------------- Check for and write incoming Aftertouch 
    if (MIDI.getType() == midi::AfterTouchChannel && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t aftertouch = MIDI.getData1();
      int channelPressurePWM = map(aftertouch, 0, 127, 0, 4095);
      dac1.setChannelValue(MCP4728_CHANNEL_A, channelPressurePWM);
    }

    // ------------------------- Check for and write incoming Modulation Wheel 
    if (MIDI.getType() == midi::ControlChange && MIDI.getData1() == 1 && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t modulationWheel = MIDI.getData2();
      int modulationWheelPWM = map(modulationWheel, 0, 127, 0, 4095);
      dac1.setChannelValue(MCP4728_CHANNEL_B, modulationWheelPWM);
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
    
    //------------------------ Read MIDI controller 70-79, write to DACs
    if (MIDI.getType() == midi::ControlChange && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t ccNumber = MIDI.getData1();
      uint8_t ccValue = MIDI.getData2();
      switch (ccNumber) {
        case 70:
        dac1.setChannelValue(MCP4728_CHANNEL_C, ccValue);
        break;
        case 71:
        dac1.setChannelValue(MCP4728_CHANNEL_D, ccValue);
        break;
        case 72:
        dac2.setChannelValue(MCP4728_CHANNEL_A, ccValue);
        break;
        case 73:
        dac2.setChannelValue(MCP4728_CHANNEL_B, ccValue);
        break;
        case 74:
        dac2.setChannelValue(MCP4728_CHANNEL_C, ccValue);
        break;
        case 75:
        dac2.setChannelValue(MCP4728_CHANNEL_D, ccValue);
        break;
        case 76:
        dac3.setChannelValue(MCP4728_CHANNEL_A, ccValue);
        break;
        case 77:
        dac3.setChannelValue(MCP4728_CHANNEL_B, ccValue);
        break;
        case 78:
        dac3.setChannelValue(MCP4728_CHANNEL_C, ccValue);
        break;
        case 79:
        dac3.setChannelValue(MCP4728_CHANNEL_D, ccValue);
        break;
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
  }

  // ---------------------------------- Write -------------------------------------------------------------------- 

    // ----------------------- Write gates and velocity outputs, bend notes 
    for (int i = 0; i < NUM_VOICES; i++) {
      // Calculate pitchbender factor
      int midiNoteVoltage = noteVolt[voices[i].midiNote];
      double semitones = (double)benderValue / (double)16383 * 2.0;
      double factor = pow(2.0, semitones / 12.0);
      voices[i].bentNote = midiNoteVoltage * factor;
      if (voices[i].bentNote < 0) {
        voices[i].bentNote = 0;
      }
      if (voices[i].bentNote > 16383) {
        voices[i].bentNote = 16383;
      }
    }
  
  // -------------------- Write bent note frequency voltages to Note GPIOs
  analogWrite(2, voices[0].bentNote);
  analogWrite(3, voices[1].bentNote);
  analogWrite(4, voices[2].bentNote);
  analogWrite(5, voices[3].bentNote);
  analogWrite(6, voices[4].bentNote);
  analogWrite(9, voices[5].bentNote);
  analogWrite(22, voices[6].bentNote);
  analogWrite(23, voices[7].bentNote);

  // ---------------------- Write Gates
  mcp.digitalWrite(0, voices[0].noteOn ? HIGH : LOW); // Gate 01
  mcp.digitalWrite(1, voices[1].noteOn ? HIGH : LOW); // Gate 01
  mcp.digitalWrite(2, voices[2].noteOn ? HIGH : LOW); // Gate 01
  mcp.digitalWrite(3, voices[3].noteOn ? HIGH : LOW); // Gate 01
  mcp.digitalWrite(4, voices[4].noteOn ? HIGH : LOW); // Gate 01
  mcp.digitalWrite(5, voices[5].noteOn ? HIGH : LOW); // Gate 01
  mcp.digitalWrite(6, voices[6].noteOn ? HIGH : LOW); // Gate 01
  mcp.digitalWrite(7, voices[7].noteOn ? HIGH : LOW); // Gate 01
  
  //-------------------------- Fill Arpeggio buffer
  fillArpNotes();

  // --------------------- Write velocity voltages to velocity GPIOs
  analogWrite(10,veloVoltLin[voices[0].velocity]);
  analogWrite(11,veloVoltLin[voices[1].velocity]);
  analogWrite(12,veloVoltLin[voices[2].velocity]);
  analogWrite(13,veloVoltLin[voices[3].velocity]);
  analogWrite(14,veloVoltLin[voices[4].velocity]);
  analogWrite(15,veloVoltLin[voices[5].velocity]);
  analogWrite(18,veloVoltLin[voices[6].velocity]);
  analogWrite(19,veloVoltLin[voices[7].velocity]);
}
