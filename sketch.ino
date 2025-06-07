
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif
#include <NewPing.h>// ultrasonic sensor library
 //Neopixel pins
#define PIXEL_PIN    6  // Digital IO pin connected to the NeoPixels.

#define PIXEL_COUNT 16  // Number of NeoPixels  
#define pixel_count 5 
 
// Motion pins(interrupts pins) 
#define pirLeftPin 2   // Must support interrupt
#define pirRightPin 3  // Must support interrupt 
//LED ARRAY FOR DEMONSTARTION
const int ledPins[] = {4, 5, 11, 7};  // Non-consecutive pins
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);

//Ultrasonic sensor(Bird dectection)
#define trigPin 12 
#define echoPin 13  
bool birdPresent = false; 
float threshold = 15.0; 

//Led array demonstartion 
const int chaseDelay = 150;   // delay between each chase step
const int holdDelay = 500;    // how long all LEDs stay on after chase
const int blinkCount = 3;     // number of blinks
const int blinkDelay = 250;   // delay between blinks

//Multiplexer pin outs 
 const int pin_Out_S0 = 8;  // S0
const int pin_Out_S1 = 9;  // S1
const int pin_Out_S2 = 10;  // S2
const int pin_In_Mux1 = A0;  // MUX analog input
const int LedAnalogOutPin = 5;  // LED PWM pin
 

//Motion Variables 
volatile unsigned long t1 = 0;
volatile unsigned long t2 = 0;
volatile bool leftTriggered = false;
volatile bool rightTriggered = false;

int lastState1 = LOW;
int lastState2 = LOW;

//const int threshold = 40;              // sensor 2 timestamp; initialize as 0
unsigned long start_time;  // time since program start 
//for bird direction function
unsigned long lastDirectionCheck = 0;
const unsigned long directionCooldown = 500; // in ms



int Mux1_State[5] = {0};
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);  
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)  
boolean oldState = HIGH;
int     mode     = 0; 
bool showAnimations = false;   // Currently-active animation mode, 0-9 
void setup() { 
  //Mux pins 
  pinMode(pin_Out_S0, OUTPUT);
  pinMode(pin_Out_S1, OUTPUT);
  pinMode(pin_Out_S2, OUTPUT);
  pinMode(LedAnalogOutPin, INPUT);     
  //pinMode(BirdSensor, INPUT);

//LED PINS 
for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
 
// Motion pins 
  pinMode(pirLeftPin, INPUT);
  pinMode(pirRightPin, INPUT);  


//Ultrasonic sensor pins 
pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

//Interrpts 
  attachInterrupt(digitalPinToInterrupt(pirLeftPin), leftISR, RISING);//Changes from low to high for Left sensor 
  attachInterrupt(digitalPinToInterrupt(pirRightPin), rightISR, RISING);// Changes from low to high for RightSensor

  Serial.begin(9600);   
  delay(1000);
  start_time = millis();  
  strip.begin() ;
}

void loop() { 
  updateMux1(); 
  strip.clear(); 

//BirdDirection  

  if (leftTriggered && rightTriggered) {
    noInterrupts(); // Temporarily disable to avoid changes mid-check
    unsigned long timeLeft = t1;
    unsigned long timeRight = t2;
    leftTriggered = false;
    rightTriggered = false;
    interrupts(); // Re-enable

    if (timeLeft < timeRight) {
      Serial.println("Motion: LEFT to RIGHT"); 
      RightLedpat();
      RightPattern(4000); 
     
    } 
    if(timeRight < timeLeft){
      Serial.println("Motion: RIGHT to LEFT"); 
      LeftLedpat();
      LeftPattern(4000); 
      
    }
  } 

// Bird in nest detection 
 CheckForBird(); 


  int sum = 0;
  for (int i = 0; i < 5; i++) { 
    sum += Mux1_State[i];
    Serial.print(Mux1_State[i]);
    Serial.print(i < 4 ? ", " : "\n");
  }

  int average = sum / 5;
  int outputValue = map(average, 0, 1023, 0, 255); 

  Serial.print("Average: ");
  Serial.print(average);
  Serial.print("\tPWM: ");
  Serial.println(outputValue); 

    // Set PWM output

  // Use average to select animation
 if (!showAnimations) {
  if (average <= 260) {
    theaterChase(strip.Color(127,   0,   0), 50); // Red Chase
  } else if (average <= 500) {
    colorWipe(strip.Color(0, 0, 255), 50); // Blue Wipe
  } else if (average <= 570) {
    ArrayNeoPixel(); 
  } else if (average <= 700) {
    rainbow(5); // Rainbow
  } else {
    theaterChaseRainbow(10); // Complex rainbow
  }
}
 // delay(200);
}



void updateMux1() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(pin_Out_S0, (i & B00000001) ? HIGH : LOW);
    digitalWrite(pin_Out_S1, (i & B00000010) ? HIGH : LOW);
    digitalWrite(pin_Out_S2, (i & B00000100) ? HIGH : LOW);
    
    delay(10);  // Allow signal to stabilize
    Mux1_State[i] = analogRead(pin_In_Mux1);
  } 
}
  // Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait) {
  for(int a=0; a<10; a++) {  // Repeat 10 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  // Hue of first pixel runs 3 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 3*65536. Adding 256 to firstPixelHue each time
  // means we'll make 3*65536/256 = 768 passes through this outer loop:
  for(long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait) 
{
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for(int a=0; a<30; a++) {  // Repeat 30 times...
    for(int b=0; b<3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for(int c=b; c<strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}  
void ArrayNeoPixel() 
{
  int sum = 0;

  // Sum LDR brightness from even-indexed sensors
  for (int i = 0; i < PIXEL_COUNT; i += 2) {
    int ldrValue = Mux1_State[i];
    int brightness = map(ldrValue, 0, 1023, 0, 255);
    sum += brightness;

    Serial.print("LDR ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(ldrValue);
    Serial.print(" -> Brightness: ");
    Serial.println(brightness);
  }

  int average = sum / (PIXEL_COUNT / 2);
  Serial.print("Average Brightness: ");
  Serial.println(average);

  const int flickerThreshold = 100;

  if (average < flickerThreshold) {
    // One flicker brightness for all pixels
    int flicker = random(180, 255);  // Flicker value for yellow

    for (int i = 0; i < PIXEL_COUNT; i+= 2) {
      strip.setPixelColor(i, strip.Color(flicker, flicker, 0)); // All same yellow flicker
    }
  } else {
    for (int i = 0; i < PIXEL_COUNT; i++) {
      strip.setPixelColor(i, 0);  // All off
    }
  }

  strip.show();
   //delay(2000); 
   // Flicker speed
} 

/*void BirdDirection() {
  d1 = sensor1.ping_cm(); // or .Distance() if using HC_SR04
  d2 = sensor2.ping_cm();

  Serial.print("d1: "); Serial.print(d1);
  Serial.print(" cm\t d2: "); Serial.println(d2);

  if (d1 <= max_distance) t1 = millis();
  if (d2 <= max_distance) t2 = millis();

  if (t1 > 0 && t2 > 0 && millis() - lastDirectionCheck > directionCooldown) {
    if (t1 < t2) {
      Serial.println("Left to right");
    } else if (t2 < t1) {
      Serial.println("Right to left");
    }
    t1 = 0;
    t2 = 0;
    lastDirectionCheck = millis(); // debounce
  }
}*/  

// ========= Our Interrupt Service Routines  ================

// For Left motion sensor 
void leftISR() {
  if (!leftTriggered) {
    t1 = millis();
    leftTriggered = true;
  }
}
// For Right Motion Sensor 
void rightISR() {
  if (!rightTriggered) {
    t2 = millis();
    rightTriggered = true;
  }
}  

// ================================================================ 

// Bird Direction Animations 
void RightPattern(unsigned long duration) {  // default to  4 seconds(set to duration to 4000)
  unsigned long startTime = millis(); // unsigned long for amount of milli seconds(interger and float cant hold, they are big numbers )
  while (millis() - startTime < duration) { 
    /*millis() gives the number of milliseconds since the Arduino started.
   When you save startTime = millis(), this marks a moment in time .
   millis() - startTime tells you how much time has passed since that moment.
   You compare the difference with the duration.*/ 
   int RightpixelsEnd  = PIXEL_COUNT - 9;  
    int RightpixelsStart = 2; 
    int flicker = random(180, 255);  // Flicker yellow
    for (int i = RightpixelsStart; i < RightpixelsEnd ; i++) {
      strip.setPixelColor(i, strip.Color(flicker, flicker, 0));
    }
    strip.show();
    delay(100); // Flicker speed
  }
  strip.clear();
  strip.show();
}
void LeftPattern(unsigned long duration) 
{
  unsigned long startTime = millis(); 
  while(millis() - startTime < duration ) 
  {
    int flicker = random(180,255);  
    int leftpixelsEnd  = PIXEL_COUNT - 1 ;  
    int leftpixelsStart = PIXEL_COUNT - 6;
    for(int i = leftpixelsStart; i< leftpixelsEnd ; i++) 
    {
      strip.setPixelColor(i, strip.Color(flicker, flicker, 0));
    } 
    strip.show(); 
    delay(100);//flicker speed 
  } 
  strip.clear() ;
  strip.show();
}
void RightLedpat() 
{
    for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(chaseDelay); 
    digitalWrite(ledPins[i], LOW);
  }

  // 2. Turn all LEDs ON together
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  delay(holdDelay); // Hold all ON for a moment

  // 3. Blink all LEDs together
  for (int i = 0; i < blinkCount; i++) {
    // Turn all OFF
    for (int j = 0; j < numLeds; j++) {
      digitalWrite(ledPins[j], LOW);
    }
    delay(blinkDelay);

    // Turn all ON
    for (int j = 0; j < numLeds; j++) {
      digitalWrite(ledPins[j], HIGH);
    }
    delay(blinkDelay);
  }

  // 4. Turn all OFF after blinking
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], LOW);
  }
} 
void LeftLedpat() 
{
for (int i = numLeds - 1; i >= 0; i--) {
    digitalWrite(ledPins[i], HIGH);
    delay(chaseDelay); 
    digitalWrite(ledPins[i], LOW);
  }

  // 2. Turn all LEDs ON together
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
  }
  delay(holdDelay); // Hold all ON for a moment

  // 3. Blink all LEDs together
  for (int i = 0; i < blinkCount; i++) {
    // Turn all OFF
    for (int j = 0; j < numLeds; j++) {
      digitalWrite(ledPins[j], LOW);
    }
    delay(blinkDelay);

    // Turn all ON
    for (int j = 0; j < numLeds; j++) {
      digitalWrite(ledPins[j], HIGH);
    }
    delay(blinkDelay);
  }

  // 4. Turn all OFF after blinking
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], LOW);
  }

} 
//======= Bird in nest detector =======//
void CheckForBird() {
   float distance = getDistance(); 
   int delayTime = 3000;

  if (distance > 0 && distance < threshold) {
    // Bird is close (in the nest)
    digitalWrite(ledPins[1], HIGH);
    digitalWrite(ledPins[2], HIGH); 

      for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0)); // Full green
  }
  strip.show();
  
  delay(delayTime);  
    Serial.println("Bird detected!");
  } else {
    digitalWrite(ledPins[1], LOW);
    digitalWrite(ledPins[2], LOW); 
    strip.clear(); 
  }
}
float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH);
  float dist = duration * 0.034 / 2.0;
  return dist;
}