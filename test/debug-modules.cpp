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

#define NUM_VOICES 8
#define MIDI_CHANNEL 1
#define PITCH_POS 2
#define PITCH_NEG -2
#define CC_TEMPO 5
#define A4 440
uint16_t benderValue = 0;
uint8_t midiTempo;
uint8_t midiController[10];
bool susOn = false;

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

// ------------------------ Voice buffer subroutines 
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

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// ************************************************
// ******************** SETUP *********************
// ************************************************

void setup() {
	Serial.begin(9600);
}

// ************************************************
// ******************** MAIN **********************
// ************************************************

void loop() {


if (MIDI.read()) {

    // -------------------- Note On/Off
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
    
    // ------------------ Pitchbend 
    if (MIDI.getType() == midi::PitchBend && MIDI.getChannel() == MIDI_CHANNEL) {
      uint16_t pitchBend = MIDI.getData1() | (MIDI.getData2() << 7);
    }

    // ------------------ Aftertouch 
    if (MIDI.getType() == midi::AfterTouchChannel && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t aftertouch = MIDI.getData1();
    }

    // ------------------ Modwheel 
    if (MIDI.getType() == midi::ControlChange && MIDI.getData1() == 1 && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t modulationWheel = MIDI.getData1() | (MIDI.getData2() << 7);
		}

    // ------------------ MIDI tempo 
    if (MIDI.getType() == midi::ControlChange && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t ccNumber = MIDI.getData1();
      uint8_t ccValue = MIDI.getData2();
      if (ccNumber == CC_TEMPO) {
        midiTempo = ccValue;
      }
		}

		// ------------------ Sustain
    if (MIDI.getType() == midi::ControlChange && MIDI.getData1() == 64 && MIDI.getChannel() == MIDI_CHANNEL) {
      uint8_t sustainPedal = MIDI.getData2();
			if (sustainPedal > 63) {
        susOn = true;
      } else {
        susOn = false;
      }
    }
	}

	// ****************************************************************
	// ***************************** DEBUG ****************************
	// ****************************************************************
	
	for (int i = 0; i < NUM_VOICES; i++) {
		Serial.println("Note Age: "+voices[i].noteAge);
    Serial.println("MIDI Note: "+voices[i].midiNote);
    Serial.println("On/Off: "+voices[i].noteOn);
    Serial.println("Velo: "+voices[i].velocity);
    Serial.println("Bend: "+voices[i].pitchBend);
    Serial.println("Touch: "+voices[i].channelPressure);
    Serial.println("Wheel: "+voices[i].modulationWheel);
    Serial.println("Prev Note: "+voices[i].prevNote);
    Serial.println("Final Note: "+voices[i].bentNote);
    Serial.println("Note Freq: "+voices[i].bentNoteFreq);
	}  
}


