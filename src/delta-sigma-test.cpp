// Define the voltage range for the 1V/octave scale
const float VOLTAGE_MIN = 0.0;
const float VOLTAGE_MAX = 3.3;

// Define the voltage offset for the 1V/octave scale
const float VOLTAGE_OFFSET = 0.0;

// Define the delta-sigma modulation oversampling ratio
const int OVERSAMPLING_RATIO = 64;

// Define the delta-sigma modulation integration filter coefficient
const float FILTER_COEFF = 1.0 / OVERSAMPLING_RATIO;

// Define the delta-sigma modulation state variables
float voltage_prev[8] = {0.0};
float integrator[8] = {0.0};

// Define the delta-sigma modulation subroutine
void deltaSigmaModulation(float frequency, int notePin, int controlPin) {
  // Compute the desired voltage from the desired frequency using the 1V/octave scale
  float voltage = (log(frequency / 440.0) / log(2.0)) + 5.0;

  // Scale the voltage range to the 3.3V output range
  voltage = (voltage - VOLTAGE_OFFSET) / (VOLTAGE_MAX - VOLTAGE_MIN) * 3.3;

  // Compute the desired duty cycle from the desired voltage
  float duty_cycle = voltage / 3.3 * 255.0;

  // Oversample the duty cycle signal
  for (int i = 0; i < OVERSAMPLING_RATIO; i++) {
    // Compute the error signal
    float error = duty_cycle - voltage_prev[notePin];

    // Update the integrator
    integrator[notePin] += FILTER_COEFF * error;

    // Compute the output bit
    int bit = (integrator[notePin] >= 0.0) ? 1 : 0;

    // Update the previous voltage value
    voltage_prev[notePin] += 2.0 * (bit - 0.5) * FILTER_COEFF;

    // Write the PWM output signals
    analogWrite(notePin, bit * 255);
    analogWrite(controlPin, (1 - bit) * 255);
  }

  // Apply an RC filter to the PWM output signals to remove high-frequency noise
  float voltage_filtered = 0.0;
  float alpha = 0.05; // RC filter smoothing factor
  voltage_filtered = alpha * voltage_prev[notePin] + (1 - alpha) * voltage_filtered;
  voltage_prev[notePin] = voltage_filtered;
}

// Example usage:
void loop() {
  for (int i = 0; i < 8; i++) {
    deltaSigmaModulation(noteFrequency[i], notePin[i], controlPin[i]);
  }
}
