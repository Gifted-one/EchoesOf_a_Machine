#include <driver/adc.h>
#include <kiss_fft.h>
#include <kiss_fftr.h>
#include <FastLED.h>
#include <Wire.h>

#define NUM_MICS 5
#define SAMPLES 256
#define SAMPLE_RATE 8000
#define N_FFT_FRAMES 1

// Mic ADC pins
const int micPins[NUM_MICS] = {34, 35, 32, 33, 39};

// WS2812B config
#define LED_PIN 4
#define NUM_LEDS NUM_MICS
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

// Notch filter coefficients (50Hz)
float a0 = 0.978030f, a1 = -1.618034f, a2 = 0.978030f;
float b1 = -1.591300f, b2 = 0.661310f;

struct NotchState {
  float x1 = 0, x2 = 0;
  float y1 = 0, y2 = 0;
};

NotchState notchStates[NUM_MICS];

// LED states
CRGB currentColor[NUM_LEDS], targetColor[NUM_LEDS];
uint8_t currentBrightness[NUM_LEDS] = {0}, targetBrightness[NUM_LEDS] = {0};

// Buffers
float raw_signal[NUM_MICS][SAMPLES];
float filtered_signal[NUM_MICS][SAMPLES];
kiss_fft_scalar fft_input[NUM_MICS][SAMPLES];
kiss_fft_cpx fft_output[NUM_MICS][SAMPLES / 2 + 1];
float magnitude_history[NUM_MICS][N_FFT_FRAMES][SAMPLES / 2 + 1];

float applyNotch(float x, NotchState &s) {
  float y = a0 * x + a1 * s.x1 + a2 * s.x2 - b1 * s.y1 - b2 * s.y2;
  s.x2 = s.x1;
  s.x1 = x;
  s.y2 = s.y1;
  s.y1 = y;
  return y;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  Wire.begin(21, 22);  // SDA, SCL

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(0);
  FastLED.clear();
  FastLED.show();

  Serial.println("Mic1_PeakFreqHz,...,Mic5_Brightness");
}

void loop() {
  kiss_fftr_cfg fft_cfg[NUM_MICS];
  for (int i = 0; i < NUM_MICS; i++) {
    fft_cfg[i] = kiss_fftr_alloc(SAMPLES, 0, 0, 0);
  }

  for (int f = 0; f < N_FFT_FRAMES; f++) {
    for (int i = 0; i < SAMPLES; i++) {
      for (int m = 0; m < NUM_MICS; m++) {
        int val = analogRead(micPins[m]);
        float x = val - 2048;
        float y = applyNotch(x, notchStates[m]);

        raw_signal[m][i] = x;
        filtered_signal[m][i] = y;
        fft_input[m][i] = y;
      }
      delayMicroseconds(1000000 / SAMPLE_RATE);
    }

    for (int m = 0; m < NUM_MICS; m++) {
      kiss_fftr(fft_cfg[m], fft_input[m], fft_output[m]);
      for (int i = 0; i < SAMPLES / 2 + 1; i++) {
        magnitude_history[m][f][i] = sqrtf(fft_output[m][i].r * fft_output[m][i].r +
                                           fft_output[m][i].i * fft_output[m][i].i);
      }
    }
  }

  for (int i = 0; i < NUM_MICS; i++) {
    free(fft_cfg[i]);
  }

  // Respond to peak frequency
  for (int m = 0; m < NUM_MICS; m++) {
    float maxMag = 0;
    int peakIndex = 0;

    // Ignore first ~150Hz (bins 0-4)
    for (int i = 5; i < SAMPLES / 2 + 1; i++) {
      float mag = magnitude_history[m][0][i];
      if (mag > maxMag) {
        maxMag = mag;
        peakIndex = i;
      }
    }

    float peakFreq = peakIndex * ((float)SAMPLE_RATE / SAMPLES);

    // Brightness scaled from magnitude
    targetBrightness[m] = constrain((int)(maxMag * 2.0), 0, 255);

    // Colour mapped from frequency
    targetColor[m] = frequencyToColor(peakFreq);

    // Debug (optional)
    // Serial.printf("Mic %d: Freq %.1f Hz, Mag %.2f\n", m + 1, peakFreq, maxMag);
  }

  // Smooth LED updates
  for (int step = 0; step < 50; step++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      nblend(currentColor[i], targetColor[i], 10);
      leds[i] = currentColor[i];

      if (currentBrightness[i] < targetBrightness[i]) currentBrightness[i]++;
      else if (currentBrightness[i] > targetBrightness[i]) currentBrightness[i]--;
    }

    FastLED.setBrightness(*std::max_element(currentBrightness, currentBrightness + NUM_LEDS));
    FastLED.show();
    delay(2);
  }

  // Request data from slave Arduino
  Wire.requestFrom(8, 10);
  while (Wire.available()) {
    int c = Wire.read();
    Serial.println(c);
  }

  //delay(2);
}

// HSV-based frequency to colour
CRGB frequencyToColor(float freqHz) {
  // Clamp 100–3000 Hz range
  uint8_t hue = map(constrain(freqHz, 100, 3000), 100, 3000, 0, 255);
  return CHSV(hue, 255, 255);
}
