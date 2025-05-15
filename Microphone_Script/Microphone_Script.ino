#include <FastLED.h>

#define LED_PIN 2
#define NUM_LEDS 2
#define MIC1_PIN A0
#define MIC2_PIN A1

CRGB leds[NUM_LEDS];

const int threshold = 512; // Mid-point for zero-crossing

// State for Mic 1
unsigned long prevTime1 = 0, periodSum1 = 0;
int crossings1 = 0;
bool wasAbove1 = false;

// State for Mic 2
unsigned long prevTime2 = 0, periodSum2 = 0;
int crossings2 = 0;
bool wasAbove2 = false;

const int maxCrossings = 10;

void setup() {
  pinMode(MIC1_PIN, INPUT);
  pinMode(MIC2_PIN, INPUT);
  Serial.begin(115200);
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear();
  FastLED.show();
}

void loop() {
  handleMic(MIC1_PIN, 0, &prevTime1, &periodSum1, &crossings1, &wasAbove1);
  handleMic(MIC2_PIN, 1, &prevTime2, &periodSum2, &crossings2, &wasAbove2);
}

void handleMic(int micPin, int ledIndex,
               unsigned long* prevTime, unsigned long* periodSum,
               int* crossings, bool* wasAbove) {

  int sample = analogRead(micPin);
  bool isAbove = sample > threshold;

  if (isAbove && !(*wasAbove)) {
    unsigned long now = micros();

    if (*crossings > 0) {
      unsigned long period = now - *prevTime;
      *periodSum += period;
    }

    *prevTime = now;
    (*crossings)++;

    if (*crossings >= maxCrossings) {
      float avgPeriod = *periodSum / float(*crossings - 1);
      float frequency = 1000000.0 / avgPeriod;

      // Print debug info
      Serial.print("Mic ");
      Serial.print(micPin == MIC1_PIN ? "1" : "2");
      Serial.print(" - Freq: ");
      Serial.print(frequency);
      Serial.print(" Hz, Amp: ");
      Serial.println(sample);

      // Update LED for this mic
      updateLED(ledIndex, sample, frequency);

      // Reset for next cycle
      *periodSum = 0;
      *crossings = 0;
    }
  }

  *wasAbove = isAbove;
}

void updateLED(int ledIndex, int amplitude, float frequency) {
  // Constrain frequency and map to RGB colour (low = red, high = blue)
  frequency = constrain(frequency, 100, 1500);
  amplitude = constrain(amplitude, 200, 800);
  uint8_t red = map(frequency, 100, 1500, 255, 0);
  uint8_t blue = map(frequency, 100, 1500, 0, 255);

  // Map amplitude to brightness
  uint8_t brightness = map(amplitude, 300, 800, 20, 255);

  leds[ledIndex] = CRGB(red, 0, blue);
  FastLED.setBrightness(brightness);
  FastLED.show();
}
