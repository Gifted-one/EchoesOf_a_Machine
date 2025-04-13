#include <arduinoFFT.h>

#define SAMPLES 128
#define SAMPLING_FREQUENCY 2048  // In Hz

// Create an FFT instance with double precision and 128 samples
ArduinoFFT<double> FFT;

unsigned int samplingPeriod;
unsigned long microSeconds;

double vReal[SAMPLES];
double vImag[SAMPLES];

void setup() {
  Serial.begin(115200);
  samplingPeriod = round(1000000.0 / SAMPLING_FREQUENCY);
}

void loop() {
  // Collect samples
  for (int i = 0; i < SAMPLES; i++) {
    microSeconds = micros();
    vReal[i] = analogRead(A0);  // Input from microphone module
    vImag[i] = 0;

    while (micros() < (microSeconds + samplingPeriod)) {
      // Wait to maintain sampling rate
    }
  }

  // Run FFT
  FFT.windowing(vReal, SAMPLES, FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(vReal, vImag, SAMPLES, FFTDirection::Forward);
  FFT.complexToMagnitude(vReal, vImag, SAMPLES);

  // Find and print the peak frequency
  double peak = FFT.majorPeak(vReal, SAMPLES, SAMPLING_FREQUENCY);
  Serial.print("Peak Frequency: ");
  Serial.print(peak);
  Serial.println(" Hz");

  delay(500);  // Optional: pause to make serial output readable
}
