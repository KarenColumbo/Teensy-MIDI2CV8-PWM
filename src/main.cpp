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
#include "TCA9548.h"
#include <EEPROM.h>
#include "Bounce2.h"

// MIDI stuff
#define NUM_VOICES 8
#define MIDI_CHANNEL 1
#define PITCH_POS 2
#define PITCH_NEG -2
#define CC_TEMPO 5
#define A4 440
uint8_t midiTempo;
uint8_t midiController[10];
uint16_t benderValue = 0;
bool susOn = false;
int arpIndex = 0;
int numArpNotes = 0;
int arpNotes[NUM_VOICES];
uint16_t eighthNoteDuration = 0;
uint16_t sixteenthNoteDuration = 0;

// Save/Load switches
//const int EEPROM_ADDRESS = 0;
//const int DEBOUNCE_DELAY = 20;
//const int THRESHOLD = 3000;
//Bounce saveSwitch = Bounce();
//Bounce loadSwitch = Bounce();
//unsigned long startTime = 0;
//bool saveInProgress = false;
//bool loadInProgress = false;
int CCValue[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// GPIO Pins
const int controlPin[8] = {2, 3, 4, 5, 6, 9, 22, 23};
const int veloPin[8] = {10, 11, 12, 13, 14, 15, 18, 19};
const int notePin[8] = {7, 8, 24, 25, 28, 29, 36, 37};
//const int SAVE_SWITCH_PIN = 24;
//const int LOAD_SWITCH_PIN = 25;
// I2C pins: ----------------> 16, 17
// Pitchbender pin: ---------> 33

// --------------------------------- 14 bit Velocity Voltages - linear 
const float veloVoltLin[128]={
  0, 128, 256, 384, 512, 640, 768, 896, 
  1024, 1152, 1280, 1408, 1536, 1664, 1792, 1920, 
  2048, 2176, 2304, 2432, 2560, 2688, 2816, 2944, 
  3072, 3200, 3328, 3456, 3584, 3712, 3840, 3968, 
  4096, 4224, 4352, 4480, 4608, 4736, 4864, 4992, 
  5120, 5248, 5376, 5504, 5632, 5760, 5888, 6016, 
  6144, 6272, 6400, 6528, 6656, 6784, 6912, 7040, 
  7168, 7296, 7424, 7552, 7680, 7808, 7936, 8064, 
  8192, 8320, 8448, 8576, 8704, 8832, 8960, 9088, 
  9216, 9344, 9472, 9600, 9728, 9856, 9984, 10112, 
  10240, 10368, 10496, 10624, 10752, 10880, 11008, 11136, 
  11264, 11392, 11520, 11648, 11776, 11904, 12032, 12160, 
  12288, 12416, 12544, 12672, 12800, 12928, 13056, 13184, 
  13312, 13440, 13568, 13696, 13824, 13952, 14080, 14208, 
  14336, 14464, 14592, 14720, 14848, 14976, 15104, 15232, 
  15360, 15488, 15616, 15744, 15872, 16000, 16128, 16256
};

// ----------------------------- MIDI note frequencies C1-C7
float midiNoteFrequency [73] = {
  32.7032, 34.6478, 36.7081, 38.8909, 41.2034, 43.6535, 46.2493, 48.9994, 51.9131, 55, 58.2705, 61.7354, 
  65.4064, 69.2957, 73.4162, 77.7817, 82.4069, 87.3071, 92.4986, 97.9989, 103.826, 110, 116.541, 123.471, 
  130.813, 138.591, 146.832, 155.563, 164.814, 174.614, 184.997, 195.998, 207.652, 220, 233.082, 246.942, 
  261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995, 415.305, 440, 466.164, 493.883, 
  523.251, 554.365, 587.33, 622.254, 659.255, 698.456, 739.989, 783.991, 830.609, 880, 932.328, 987.767, 
  1046.5, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760, 1864.66, 1975.53, 
  2093
};

// ----------------------------- 14 bit note frequency voltages C1-C7
const unsigned int noteVolt[73] = {
  0, 15, 32, 49, 68, 87, 108, 130, 153, 177, 203, 231, 
  260, 291, 324, 358, 395, 434, 476, 519, 566, 615, 667, 722, 
  780, 842, 908, 977, 1051, 1129, 1211, 1299, 1391, 1489, 1593, 1704, 
  1820, 1944, 2075, 2214, 2361, 2517, 2682, 2857, 3043, 3239, 3447, 3667, 
  3901, 4148, 4411, 4688, 4982, 5294, 5625, 5974, 6345, 6738, 7154, 7595, 
  8062, 8557, 9081, 9637, 10225, 10849, 11509, 12209, 12950, 13736, 14568, 15450, 
  16383
  };

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
  uint16_t bentNoteFreq;
  unsigned long lastTime;
  bool clockState;
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
    voices[i].bentNoteFreq = 0x2000;
    voices[i].lastTime = 0;
    voices[i].clockState = false;
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
//void fillArpNotes() {
//  arpIndex = 0;
//  for (int i = 0; i < NUM_VOICES; i++) {
//    if (voices[i].noteOn) {
//      arpNotes[arpIndex++] = voices[i].midiNote;
//    }
//  }
//  numArpNotes = arpIndex;
//  for (int i = 0; i < NUM_VOICES; i++) {
//    for (int j = 0; j < arpIndex - 1; j++) {
//      if (voices[i].noteAge < voices[j].noteAge) {
//        int temp = arpNotes[j];
//        arpNotes[j] = arpNotes[j + 1];
//        arpNotes[j + 1] = temp;
//      }
//    }
//  }
//}

// ------------------------------------ Initialize multiplexer, 4728s and 23017
TCA9548 tca = TCA9548(0x70);
Adafruit_MCP4728 dac_0, dac_1, dac_2, dac_3, dac_4;
Adafruit_MCP23X17 mcp_1, mcp_2;

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

void tcaselect(uint8_t i) {
  if (i > 7) return;
 
  Wire.beginTransmission(0x70);
  Wire.write(1 << i);
  Wire.endTransmission();  
}

// ******************************************************************************************************
// ******************************************************************************************* MAIN SETUP 
void setup() {
//  for (int i = 0; i < NUM_VOICES; i++) {
//    arpNotes[i] = -1;
//  }

  //pinMode(SAVE_SWITCH_PIN, INPUT_PULLUP);
  //pinMode(LOAD_SWITCH_PIN, INPUT_PULLUP);
  //saveSwitch.attach(SAVE_SWITCH_PIN);
  //loadSwitch.attach(LOAD_SWITCH_PIN);
  //saveSwitch.interval(DEBOUNCE_DELAY);
  //loadSwitch.interval(DEBOUNCE_DELAY);

  // ****************** WARNING: Connect DACs VDD to 5 volts!!! 

  // Pin 16 (SCL) --> SCL multiplexer
  // Pin 17 (SDA) --> SDA multiplexer
  
  Wire.begin(200000);
  tcaselect(0);
  dac_0.begin();
  tcaselect(1);
  dac_1.begin();
  tcaselect(2);
  dac_2.begin();
  tcaselect(3);
  dac_3.begin();
  tcaselect(5);
  mcp_1.begin_I2C();
  tcaselect(6);
  mcp_2.begin_I2C();
  
  // Set up and pull low 14 bits Hardware PWM for note frequencies, velocities, pitchbender, and gates
  analogWriteResolution(14);
  for (int i = 0; i < NUM_VOICES; i++) {
    pinMode(controlPin[i], OUTPUT); // Note 01
    analogWriteFrequency(controlPin[i], 9155.27);
    digitalWrite(controlPin[i], LOW);
    pinMode(veloPin[i], OUTPUT); // Velocity 01
    digitalWrite(veloPin[i], LOW);
    pinMode(veloPin[i], OUTPUT); // Velocity 02
  }
  pinMode(33, OUTPUT); // Pitchbender
  analogWriteFrequency(33, 9155.27);
  digitalWrite(33, LOW);
  
	tcaselect(5);
	for (int i = 0; i < 8; i++) {
  	mcp_1.pinMode(i, OUTPUT);
  	mcp_1.digitalWrite(i, LOW);	
	}
}

// ********************************************************************************************************
// ********************************************************************************************** MAIN LOOP 
void loop() {
  
  // ------------------------------------------------------------------------------------------------- Read 
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
      analogWrite(33, map(pitchBend, 0, 16383, PITCH_NEG, PITCH_POS));
    }

    // ----------------------- Check for and write incoming Aftertouch 
    if (MIDI.getType() == midi::AfterTouchChannel && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t aftertouch = MIDI.getData1();
      tcaselect(0);
			dac_0.setChannelValue(MCP4728_CHANNEL_A, map(aftertouch, 0, 127, 0, 4095));
    }

    // ------------------------- Check for and write incoming Modulation Wheel 
    if (MIDI.getType() == midi::ControlChange && MIDI.getData1() == 1 && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t modulationWheel = MIDI.getData1() | (MIDI.getData2() << 7);
			tcaselect(0);
      dac_0.setChannelValue(MCP4728_CHANNEL_B, map(modulationWheel, 0, 16383, 0, 4095));
    }

    // ------------------------- Check for and write incoming MIDI tempo 
    if (MIDI.getType() == midi::ControlChange && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t ccNumber = MIDI.getData1();
      uint8_t ccValue = MIDI.getData2();
      if (ccNumber == CC_TEMPO) {
        midiTempo = ccValue;
      }
    //  eighthNoteDuration = (60 / midiTempo) * 1000 / 2;
    //  sixteenthNoteDuration = (60 / midiTempo) * 1000 / 4;
    }
    
    //------------------------ Read MIDI controller 70-83, write to DACs
    if (MIDI.getType() == midi::ControlChange && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t ccNumber = MIDI.getData1();
      uint8_t ccValue = MIDI.getData2();
      if (ccNumber > 69 && ccNumber < 84) {
        switch (ccNumber) {
          case 70:
				  tcaselect(0);
          dac_0.setChannelValue(MCP4728_CHANNEL_C, map(ccValue, 0, 127, 0, 4095));
          CCValue[0] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 71:
          tcaselect(0);
      	  dac_0.setChannelValue(MCP4728_CHANNEL_D, map(ccValue, 0, 127, 0, 4095));
          CCValue[1] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 72:
          tcaselect(1);
      	  dac_1.setChannelValue(MCP4728_CHANNEL_A, map(ccValue, 0, 127, 0, 4095));
          CCValue[2] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 73:
          tcaselect(1);
      	  dac_1.setChannelValue(MCP4728_CHANNEL_B, map(ccValue, 0, 127, 0, 4095));
          CCValue[3] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 74:
          tcaselect(1);
      	  dac_1.setChannelValue(MCP4728_CHANNEL_C, map(ccValue, 0, 127, 0, 4095));
          CCValue[4] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 75:
          tcaselect(1);
      	  dac_1.setChannelValue(MCP4728_CHANNEL_D, map(ccValue, 0, 127, 0, 4095));
          CCValue[5] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 76:
          tcaselect(2);
      	  dac_2.setChannelValue(MCP4728_CHANNEL_A, map(ccValue, 0, 127, 0, 4095));
          CCValue[6] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 77:
          tcaselect(2);
      	  dac_2.setChannelValue(MCP4728_CHANNEL_B, map(ccValue, 0, 127, 0, 4095));
          CCValue[7] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 78:
          tcaselect(2);
      	  dac_2.setChannelValue(MCP4728_CHANNEL_C, map(ccValue, 0, 127, 0, 4095));
          CCValue[8] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 79:
          tcaselect(2);
      	  dac_2.setChannelValue(MCP4728_CHANNEL_D, map(ccValue, 0, 127, 0, 4095));
          CCValue[9] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 80:
          tcaselect(3);
      	  dac_3.setChannelValue(MCP4728_CHANNEL_A, map(ccValue, 0, 127, 0, 4095));
          CCValue[10] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 81:
          tcaselect(3);
      	  dac_3.setChannelValue(MCP4728_CHANNEL_B, map(ccValue, 0, 127, 0, 4095));
          CCValue[11] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 82:
          tcaselect(3);
      	  dac_3.setChannelValue(MCP4728_CHANNEL_C, map(ccValue, 0, 127, 0, 4095));
          CCValue[12] = map(ccValue, 0, 127, 0, 4095);
          break;
          case 83:
          tcaselect(3);
      	  dac_3.setChannelValue(MCP4728_CHANNEL_D, map(ccValue, 0, 127, 0, 4095));
          CCValue[13] = map(ccValue, 0, 127, 0, 4095);
          break;
        }
      }
    }
/*
    // ------------------------------------------------------------- Save/Load presets
    // Check if the save switch is pressed
    saveSwitch.update();
    if (saveSwitch.fallingEdge() && !loadInProgress) {
      startTime = millis();
      saveInProgress = true;
    }

    // Check if the load switch is pressed
    loadSwitch.update();
    if (loadSwitch.fallingEdge() && !saveInProgress) {
      startTime = millis();
      loadInProgress = true;
    }

    // Save values to NVM if the save switch has been pressed for more than THRESHOLD milliseconds
    if (saveInProgress && (millis() - startTime >= THRESHOLD)) {
      // Save values
      tcaselect(0);
      EEPROM.write(EEPROM_ADDRESS, CCValue[0]);
      EEPROM.write(EEPROM_ADDRESS + 1, CCValue[1]);
      tcaselect(1);
      EEPROM.write(EEPROM_ADDRESS + 2, CCValue[2]);
      EEPROM.write(EEPROM_ADDRESS + 3, CCValue[3]);
      EEPROM.write(EEPROM_ADDRESS + 4, CCValue[4]);
      EEPROM.write(EEPROM_ADDRESS + 5, CCValue[5]);
      tcaselect(2);
      EEPROM.write(EEPROM_ADDRESS + 6, CCValue[6]);
      EEPROM.write(EEPROM_ADDRESS + 7, CCValue[7]);
      EEPROM.write(EEPROM_ADDRESS + 8, CCValue[8]);
      EEPROM.write(EEPROM_ADDRESS + 9, CCValue[9]);
      tcaselect(3);
      EEPROM.write(EEPROM_ADDRESS + 10, CCValue[10]);
      EEPROM.write(EEPROM_ADDRESS + 11, CCValue[11]);
      EEPROM.write(EEPROM_ADDRESS + 12, CCValue[12]);
      EEPROM.write(EEPROM_ADDRESS + 13, CCValue[13]);
      saveInProgress = false;
    }

    // Load values from NVM if the load switch has been pressed for more than THRESHOLD milliseconds
    if (loadInProgress && (millis() - startTime >= THRESHOLD)) {
      // Load values
      uint16_t value;
      tcaselect(0);
      value = EEPROM.read(EEPROM_ADDRESS);
      dac_0.setChannelValue(MCP4728_CHANNEL_C, value);
      value = EEPROM.read(EEPROM_ADDRESS + 1);
      dac_0.setChannelValue(MCP4728_CHANNEL_D, value);
      tcaselect(1);
      value = EEPROM.read(EEPROM_ADDRESS + 2);
      dac_1.setChannelValue(MCP4728_CHANNEL_A, value);
      value = EEPROM.read(EEPROM_ADDRESS + 3);
      dac_1.setChannelValue(MCP4728_CHANNEL_B, value);
      value = EEPROM.read(EEPROM_ADDRESS + 4);
      dac_1.setChannelValue(MCP4728_CHANNEL_C, value);
      value = EEPROM.read(EEPROM_ADDRESS + 5);
      dac_1.setChannelValue(MCP4728_CHANNEL_D, value);
      tcaselect(2);
      value = EEPROM.read(EEPROM_ADDRESS + 6);
      dac_2.setChannelValue(MCP4728_CHANNEL_A, value);
      value = EEPROM.read(EEPROM_ADDRESS + 7);
      dac_2.setChannelValue(MCP4728_CHANNEL_B, value);
      value = EEPROM.read(EEPROM_ADDRESS + 8);
      dac_2.setChannelValue(MCP4728_CHANNEL_C, value);
      value = EEPROM.read(EEPROM_ADDRESS + 9);
      dac_2.setChannelValue(MCP4728_CHANNEL_D, value);
      tcaselect(3);
      value = EEPROM.read(EEPROM_ADDRESS + 10);
      dac_3.setChannelValue(MCP4728_CHANNEL_A, value);
      value = EEPROM.read(EEPROM_ADDRESS + 11);
      dac_3.setChannelValue(MCP4728_CHANNEL_B, value);
      value = EEPROM.read(EEPROM_ADDRESS + 12);
      dac_3.setChannelValue(MCP4728_CHANNEL_C, value);
      value = EEPROM.read(EEPROM_ADDRESS + 13);
      dac_3.setChannelValue(MCP4728_CHANNEL_D, value);
      loadInProgress = false;
    }
*/
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

  // ---------------------------------------------------------------------------------------------------- Write
  //  // ----------------------- Write notes, velocities, and gates
  //  tcaselect(5);
  //  for (int i = 0; i < NUM_VOICES; i++) {
  //    // Calculate pitchbender factor
  //    int midiNoteVoltage = noteVolt[voices[i].midiNote];
  //    double semitones = (double)benderValue / (double)16383 * 2.0;
  //    double factor = pow(2.0, semitones / 12.0);
  //    voices[i].bentNote = midiNoteVoltage * factor;
  //    voices[i].bentNoteFreq = midiNoteFrequency[i] * factor;
  //    if (voices[i].bentNote < 0) {
  //      voices[i].bentNote = 0;
  //    }
  //    if (voices[i].bentNote > 16383) {
  //      voices[i].bentNote = 16383;
  //    }
  //    analogWrite(controlPin[i], voices[i].bentNote);
  //    analogWrite(veloPin[i],veloVoltLin[voices[i].velocity]);
  //    mcp_1.digitalWrite(i, voices[i].noteOn ? HIGH : LOW);
  //  }
  for (int i = 0; i < NUM_VOICES; i++) {
  // Calculate pitchbender factor
  int midiNoteVoltage = noteVolt[voices[i].midiNote];
  double semitones = (double)benderValue / (double)16383 * 2.0;
  double factor = pow(2.0, semitones / 12.0);
  voices[i].bentNote = midiNoteVoltage * factor;
  voices[i].bentNoteFreq = midiNoteFrequency[i] * factor;
  if (voices[i].bentNote < 0) {
    voices[i].bentNote = 0;
  }
  if (voices[i].bentNote > 16383) {
    voices[i].bentNote = 16383;
  }

  // Generate a square wave with the desired frequency
  double period = 1.0 / voices[i].bentNoteFreq;
  double halfPeriod = period / 2.0;
  unsigned long currentTime = millis();
  if (currentTime - voices[i].lastTime > halfPeriod) {
    voices[i].lastTime = currentTime;
    voices[i].clockState = !voices[i].clockState;
  }
  tcaselect(6);
  mcp_2.digitalWrite(i, voices[i].clockState ? HIGH : LOW);

  // Calculate the control voltage
  double controlVoltage = map(voices[i].bentNote, 0, 16384, 0, 3.3);
  analogWrite(controlPin[i], controlVoltage);
  analogWrite(notePin[i], voices[i].bentNote);

  // Write the velocity voltage
  analogWrite(veloPin[i],veloVoltLin[voices[i].velocity]);
  mcp_1.digitalWrite(i, voices[i].noteOn ? HIGH : LOW);
  }

  //-------------------------- Fill Arpeggio buffer
  //fillArpNotes();
}
