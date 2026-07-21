// SCRIPT: Engine RPM Signal Simulator (ECU Emulation)
// ROLE: Generates a variable-frequency PWM signal based on the position of
//       a virtual throttle pedal (potentiometer) to test the master system.

const int buttonPin = 3; // pin for button
const int portPin = A0; // pin for potentiometer
const int dataPin = 9; // pin to comunicate with ESP32 - D33 on ESP32

// variable used to simulate engine,pressing on pull-up button

bool engineRunning = false;
bool lastButtonState = HIGH;
int buttonState;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;


// --- Engine Configuration Parameters (VAG Platform Spec) ---
int idleRPM = 850;
int maxRPM = 6000;
int currentRPM = 0; 

unsigned long lastPrintTime = 0;
const unsigned long printInterval = 150; // print at every 150 ms

void setup() {
  Serial.begin(9600);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(dataPin,OUTPUT);

  Serial.println("=== Engine Simulator - Ready ===");
  Serial.println("Press the button to Start/Stop the engine...");
}

void loop() {
  int reading = digitalRead(buttonPin);

  // Button Debouncing Algorithm
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) { 
      buttonState = reading;

      // Trigger action only on the falling edge (Button Pressed -> LOW)

      if(buttonState == LOW){
        engineRunning = !engineRunning;

        if (engineRunning) {
          startEngineSound();
          Serial.println("--- MOTOR PORNIT ---");
        }else{
          currentRPM = 0;
          noTone(dataPin); 
          digitalWrite(dataPin, LOW);
          Serial.println("--- MOTOR OPRIT ---");
        }
      }
    }
  }

  lastButtonState = reading;

  // Run the continuous engine simulation loop if the engine is running
  if (engineRunning) {
    simulateEngine();
  }
}


// simulate start of the engine

void startEngineSound() {
  Serial.println("Cranking...");
  // "Start-up" simulation - crank + rapid acceleration to idle
  for (int freq = 50; freq <= 120; freq += 5) {
    Serial.print("  Cranking freq: ");
    Serial.print(freq);
    Serial.println(" Hz");
    delay(40);
  }
  delay(100);
  Serial.println("Motor pornit, trecere la ralanti...");
}


// simulate running of the enginer

void simulateEngine() {

  int portValue = analogRead(portPin);
  int targetRPM = map(portValue,0,1023,idleRPM,maxRPM);

    // Inertia Filter (Simulates flywheel mass and mechanical drag)
  if (currentRPM < targetRPM) {
    currentRPM += 25;
    if (currentRPM > targetRPM) currentRPM = targetRPM;
  } else if (currentRPM > targetRPM) {
    currentRPM -= 15; // decelerare putin mai lenta ca acceleratia
    if (currentRPM < targetRPM) currentRPM = targetRPM;
  }


// to avoid negative value,I put minimul at zero,and a variation with random function
  if (currentRPM < idleRPM) currentRPM = idleRPM;

  int jitter = random(-5, 6); 
  int displayRPM = currentRPM + jitter;
  
// Calculate Ignition Firing Frequency (4-stroke, 4-cylinder engine formula)
// Frequency (Hz) = (RPM / 60) * (No. Cylinders / 2) -> For 4 cylinders: (RPM / 60)
  int firingFreq = (displayRPM / 60) * 2; 


  if (firingFreq > 0) {
    tone(dataPin, firingFreq); 
  } else {
    noTone(dataPin);
  }


// print in serial monitor
  if (millis() - lastPrintTime >= printInterval) {
    Serial.print("RPM: ");
    Serial.print(currentRPM);
    Serial.print("  |  Frecventa: ");
    Serial.print(firingFreq);
    Serial.println(" Hz");
    lastPrintTime = millis();
  }

}