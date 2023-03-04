#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

/*
Note that the PDM output signal requires a low-pass filter to remove the high-frequency noise introduced by the delta-sigma modulation process. The cutoff frequency of the filter should be set to half the sample rate of the PDM output signal. In this example, the sample rate is set to 44100 Hz, so the cutoff frequency should be set to 22050 Hz.
*/

// Define the audio output object for PDM output
AudioOutputPDM pdm;

// Define the delta-sigma modulation state variables
float voltage_prev[8] = {0.0};
float integrator[8] = {0.0};

// Define the voltage range for the 1V/octave scale
const float VOLTAGE_MIN = 0.0;
const float VOLTAGE_MAX = 3.3;

// Define the voltage offset for the 1V/octave scale
const float VOLTAGE_OFFSET = 0.0;

// Define the PDM oversampling ratio
const int PDM_OVERSAMPLING_RATIO = 128;

// Define the delta-sigma modulation integration filter coefficient
const float FILTER_COEFF = 1.0 / PDM_OVERSAMPLING_RATIO;

// Define the PDM output buffer
const int PDM_BUFFER_SIZE = 256;
int pdm_buffer[8][PDM_BUFFER_SIZE];

// Define the PDM output frequency
const float PDM_FREQ = 44100.0;

// Define the delta-sigma modulation subroutine for a single voice
void deltaSigmaModulation(float frequency, int voice) {
  // Compute the desired voltage from the desired frequency using the 1V/octave scale
  float voltage = (log(frequency / 440.0) / log(2.0)) + 5.0;

  // Scale the voltage range to the 3.3V output range
  voltage = (voltage - VOLTAGE_OFFSET) / (VOLTAGE_MAX - VOLTAGE_MIN) * 3.3;

  // Compute the desired duty cycle from the desired voltage
  float duty_cycle = voltage / 3.3;

  // Oversample the duty cycle signal
  for (int i = 0; i < PDM_OVERSAMPLING_RATIO; i++) {
    // Compute the error signal
    float error = duty_cycle - voltage_prev[voice];

    // Update the integrator
    integrator[voice] += FILTER_COEFF * error;

    // Compute the output bit
    int bit = (integrator[voice] >= 0.0) ? 1 : 0;

    // Update the previous voltage value
    voltage_prev[voice] += 2.0 * (bit - 0.5) * FILTER_COEFF;

    // Write the PDM output buffer
    for (int j = 0; j < PDM_BUFFER_SIZE / PDM_OVERSAMPLING_RATIO; j++) {
      pdm_buffer[voice][i * PDM_BUFFER_SIZE / PDM_OVERSAMPLING_RATIO + j] = bit * 32767;
    }
  }
}

// Example usage:
void setup() {
  // Initialize the audio system
  AudioMemory(8);
  AudioNoInterrupts();
  pdm.begin();
  pdm.setGain(2);
  pdm.setSampleRate(PDM_FREQ);
  AudioInterrupts();
}

void loop() {
  // Generate the delta-sigma modulation output for all 8 voices
  for (int i = 0; i < 8; i++) {
    deltaSigmaModulation(noteFrequency[i], i);
  }

  // Write the PDM output buffer to the audio output for all 8 voices
  AudioNoInterrupts();
  for (int i = 0; i < 8; i++) {
    pdm.write(pdm_buffer[i], PDM_BUFFER_SIZE);
  }
  AudioInterrupts();
}
