// ==========================================
// PROJECT: THE ESP32 MULTI-TOOL (FSM)
// Micro Step 1: Structure & Constants
// ==========================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <DHT.h>

// --- PIN DEFINITIONS ---
const int PIN_SERVO    = 23;
const int PIN_TRIG     = 26;
const int PIN_ECHO     = 34;
const int PIN_CLK      = 19;
const int PIN_DT       = 18;
const int PIN_SW       = 5;
const int PIN_DHT      = 4;
const int PIN_BUZZER   = 14;

float currentTemp = 0.0;

// --- HARDWARE OBJECTS ---
Servo myServo;
DHT dht(PIN_DHT, DHT11);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// --- THE STATE MACHINE ---
enum SystemMode {
  MODE_MENU,        // 0: Selecting which app to run
  MODE_DISTANCE,    // 1: Ultrasonic App
  MODE_SERVO,       // 2: Servo Control App
  MODE_TEMP,        // 3: Temperature/Thermostat App
  MODE_TIMER
};

// Variable to track where we are
SystemMode currentMode = MODE_MENU;

// --- GLOBAL VARIABLES ---
// 1. Menu Variables
int menuOption = 1; // 1=Dist, 2=Servo, 3=Temp

// 2. Servo Variables
int servoAngle = 90;

// 3. Temp Variables
float tempThreshold = 30.0; // Default alarm limit
bool isEditingTemp = false; // Are we currently changing the limit?

// 4. Timer Alarm Variables (NEW)
int timerOption = 1;                  // 1–4
bool timerRunning = false;
unsigned long timerStartTime = 0;
unsigned long timerDuration = 0;


// 4. Button Logic (Debounce & Long Press)
unsigned long buttonPressTime = 0;
bool buttonActive = false;
bool longPressDetected = false;

// --- TIMING (Replace Delay) ---
unsigned long lastUpdate = 0; // For OLED/Sensor refresh
const int INTERVAL = 100;     // Refresh every 100ms

// ==========================================
// Micro Step 2: Input Handling Logic
// ==========================================

// Variable to store "Rotation Events" (+1, -1, or 0)
// We reset this to 0 after we use it.
int rotateEvent = 0;
bool buttonClicked = false;
bool buttonLongPressed = false;

// --- READ THE ENCODER (Run this fast!) ---
void readRotary() {
  static int lastCLK = HIGH;
  int currentCLK = digitalRead(PIN_CLK);

  // If CLK changed from HIGH to LOW, a step happened
  if (currentCLK != lastCLK && currentCLK == LOW) {
    // Check DT pin to determine direction
    if (digitalRead(PIN_DT) != currentCLK) {
      rotateEvent = 1; // Clockwise (+)
    } else {
      rotateEvent = -1; // Counter-Clockwise (-)
    }
  }
  lastCLK = currentCLK;
}

// --- READ THE BUTTON (Distinguish Click vs Hold) ---
// --- READ THE BUTTON (With Debounce) ---
void readButton() {
  int reading = digitalRead(PIN_SW);
  static int lastReading = HIGH;       // Previous raw reading
  static unsigned long lastDebounceTime = 0;
  static int buttonState = HIGH;       // The "Clean" stable state

  // 1. Check if the physical pin changed (due to press OR noise)
  if (reading != lastReading) {
    lastDebounceTime = millis(); // Reset timer on any noise
  }

  // 2. Only update the "Real State" if the pin has been stable for 50ms
  if ((millis() - lastDebounceTime) > 50) {
    
    // If the stable state has changed...
    if (reading != buttonState) {
      buttonState = reading; // Update our "Clean" state

      // --- LOGIC FOR STABLE PRESS/RELEASE ---
      
      // A. Button Just Pressed (Stable LOW)
      if (buttonState == LOW) {
        buttonPressTime = millis();
        buttonActive = true;
        longPressDetected = false;
      }

      // B. Button Just Released (Stable HIGH)
      if (buttonState == HIGH) {
        // If we were pressing it, and it wasn't a long press...
        if (buttonActive && !longPressDetected) {
           buttonClicked = true; // VALID SHORT CLICK
           Serial.println("SHORT CLICK: Select");
        }
        buttonActive = false;
      }
    }
    
    // C. Check for Long Press (While holding Stable LOW)
    if (buttonState == LOW && buttonActive && !longPressDetected) {
       if (millis() - buttonPressTime > 1000) {
         longPressDetected = true;
         buttonLongPressed = true;
         Serial.println("LONG PRESS: Back to Menu");
       }
    }
  }

  lastReading = reading; // Save raw reading for next loop
}

void setup() {
  Serial.begin(115200);
  
  // Init Hardware
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  dht.begin();
  myServo.attach(PIN_SERVO);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Failed");
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();
}

void loop() {
  // 1. Read Inputs
  readRotary();
  readButton();

  // 2. STATE MACHINE
  switch (currentMode) {
    
    // --- STATE 0: THE MENU ---
    case MODE_MENU:
      // A. Navigation (Rotate)
      if (rotateEvent != 0) {
        menuOption += rotateEvent; // Up or Down
        
        // Wrap around logic (1 -> 2 -> 3 -> 4)
        if (menuOption > 4) menuOption = 1;
        if (menuOption < 1) menuOption = 4;

        
        rotateEvent = 0; // Reset
        displayMenu();   // Update screen immediately
      }

      // B. Selection (Click)
      if (buttonClicked) {
        Serial.print("Entering Mode: "); Serial.println(menuOption);
        
        if (menuOption == 1) currentMode = MODE_DISTANCE;
        if (menuOption == 2) currentMode = MODE_SERVO;
        if (menuOption == 3) currentMode = MODE_TEMP;
        if (menuOption == 4) currentMode = MODE_TIMER;

        
        display.clearDisplay(); // Wipe screen for the new app
        display.display();
        buttonClicked = false; // Reset
      }
      
      // C. Refresh Screen (if needed)
      // We only redraw on changes to save speed, but an occasional refresh is safe
      if (millis() - lastUpdate > 500) {
        displayMenu();
        lastUpdate = millis();
      }
      break;


    // --- STATE 1, 2, 3: PLACEHOLDERS ---
    // We will build these next. For now, they just let you go BACK.
    // ... inside switch(currentMode) ...

    case MODE_DISTANCE:
       runDistanceApp();
       break;

    case MODE_SERVO:
       runServoApp();
       break;

    case MODE_TEMP:
       runTempApp();
       break;
       // Temporary "App Running" screen
       display.setCursor(0,0);
       display.println("App Running...");
       display.println("Hold Knob to Exit");
       display.display();

       case MODE_TIMER:
       runTimerApp();
       break;


       // Universal "Back" Button
       if (buttonLongPressed) {
         currentMode = MODE_MENU;
         buttonLongPressed = false;
         displayMenu();
       }
       break;
  }
}

// --- DRAW THE MAIN MENU ---
void displayMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  // Title
  display.setCursor(20, 2);
  display.println("--- MAIN MENU ---");

  // Option 1: Distance
  display.setCursor(10, 15);
  if (menuOption == 1) display.print("> "); // Highlighter
  display.println("1. Distance Meter");

  // Option 2: Servo
  display.setCursor(10, 30);
  if (menuOption == 2) display.print("> ");
  display.println("2. Servo Control");

  // Option 3: Temp
  display.setCursor(10, 45);
  if (menuOption == 3) display.print("> ");
  display.println("3. Thermostat");

  // Option 4: Timer Alarm
  display.setCursor(10, 58);
  if (menuOption == 4) display.print("> ");
  display.println("4. Timer Alarm");


  display.display();
}

// --- HELPER: Read Ultrasonic Sensor ---
float getDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duration = pulseIn(PIN_ECHO, HIGH, 30000); // Timeout 30ms
  if (duration == 0) return 999; // No object found
  return duration * 0.034 / 2;
}

// --- APP 1: DISTANCE METER ---
void runDistanceApp() {
  // 1. Read Sensor (Only every 100ms to stop screen flickering)
  if (millis() - lastUpdate > 100) {
    float dist = getDistance();
    
    // 2. Update Screen
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("--- DISTANCE ---");
    
    display.setTextSize(2);
    display.setCursor(10, 25);
    display.print(dist, 1);
    display.print(" cm");
    
    // 3. Alarm Logic (Simple Threshold)
    if (dist < 10.0) {
      digitalWrite(PIN_BUZZER, HIGH); // Beep!
      display.setCursor(20, 50);
      display.setTextSize(1);
      display.print("! WARNING !");
    } else {
      digitalWrite(PIN_BUZZER, LOW);  // Silence
    }
    
    display.display();
    lastUpdate = millis();
  }
  
  // 4. Check for Exit (Long Press)
  if (buttonLongPressed) {
    digitalWrite(PIN_BUZZER, LOW); // Ensure buzzer is off when leaving
    currentMode = MODE_MENU;
    buttonLongPressed = false;
    displayMenu();
  }
}

// --- APP 2: SERVO CONTROL ---
void runServoApp() {
  // 1. Handle Knob Turning
  if (rotateEvent != 0) {
    servoAngle += (rotateEvent * 5); // Change by 5 degrees per click
    
    // Constraint: Keep between 0 and 180
    if (servoAngle > 180) servoAngle = 180;
    if (servoAngle < 0)   servoAngle = 0;
    
    rotateEvent = 0; // Reset event
  }

  // 2. Move Hardware
  myServo.write(servoAngle);

  // 3. Update Screen (Efficiently)
  if (millis() - lastUpdate > 100) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("--- SERVO ---");
    
    // Draw visual bar
    display.drawRect(10, 20, 108, 10, WHITE); // Frame
    int barWidth = map(servoAngle, 0, 180, 0, 106);
    display.fillRect(11, 21, barWidth, 8, WHITE); // Filler
    
    // Text Angle
    display.setTextSize(2);
    display.setCursor(40, 40);
    display.print(servoAngle);
    display.print((char)247); // Degree symbol
    
    display.display();
    lastUpdate = millis();
  }

  // 4. Check for Exit
  if (buttonLongPressed) {
    currentMode = MODE_MENU;
    buttonLongPressed = false;
    displayMenu();
  }
}

// --- APP 3: THERMOSTAT ---
void runTempApp() {
  // 1. Read Sensor (Every 2 seconds is enough for DHT, but we check fast for inputs)
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 2000) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) currentTemp = t; // Only update if valid
    lastSensorRead = millis();
  }

  // 2. Handle Inputs (Editing Threshold)
  if (buttonClicked) {
    isEditingTemp = !isEditingTemp; // Toggle Edit Mode
    buttonClicked = false;
  }

  if (rotateEvent != 0) {
    if (isEditingTemp) {
      tempThreshold += rotateEvent; // Change limit
    }
    rotateEvent = 0;
  }

  // 3. Alarm Logic
  if (currentTemp > tempThreshold) {
    digitalWrite(PIN_BUZZER, HIGH);
  } else {
    digitalWrite(PIN_BUZZER, LOW);
  }

  // 4. Update Screen
  if (millis() - lastUpdate > 100) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("--- THERMOSTAT ---");

    // Show Current Readings
    display.setCursor(0, 15);
    display.print("Now: "); display.print(currentTemp, 1); display.print("C");
    
    // Show Threshold (Highlight if editing)
    display.setCursor(0, 35);
    if (isEditingTemp) display.print("Set > "); // Arrow indicates editing
    else               display.print("Set:  ");
    
    display.print(tempThreshold, 1); display.print("C");

    // Alarm Status
    if (currentTemp > tempThreshold) {
      display.setCursor(60, 50);
      display.print("ALARM!");
    }

    display.display();
    lastUpdate = millis();
  }
  

  // 5. Exit Logic
  if (buttonLongPressed) {
    isEditingTemp = false; // Reset edit mode
    digitalWrite(PIN_BUZZER, LOW);
    currentMode = MODE_MENU;
    buttonLongPressed = false;
    displayMenu();
  }

  
  
}

// --- APP 4: TIMER ALARM (NEW) ---
void runTimerApp() {

  // 1. Select Timer (Only if not running)
  if (!timerRunning && rotateEvent != 0) {
    timerOption += rotateEvent;

    if (timerOption > 4) timerOption = 1;
    if (timerOption < 1) timerOption = 4;

    rotateEvent = 0;
  }

  // 2. Start Timer
  if (buttonClicked && !timerRunning) {
    timerStartTime = millis();

    if (timerOption == 1) timerDuration = 15000;  // 15 sec
    if (timerOption == 2) timerDuration = 30000;  // 30 sec
    if (timerOption == 3) timerDuration = 45000;  // 45 sec
    if (timerOption == 4) timerDuration = 60000;  // 60 sec (1 min)

    timerRunning = true;
    buttonClicked = false;
  }

  // 3. Alarm Trigger
  if (timerRunning && (millis() - timerStartTime >= timerDuration)) {
    digitalWrite(PIN_BUZZER, HIGH);
  }

  // 4. Display
  if (millis() - lastUpdate > 100) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("--- TIMER ALARM ---");

    display.setCursor(0,20);
    display.print("Set: ");

    if (timerOption == 1) display.print("15 sec");
    if (timerOption == 2) display.print("30 sec");
    if (timerOption == 3) display.print("45 sec");
    if (timerOption == 4) display.print("60 sec");

    if (timerRunning) {
      display.setCursor(0,40);
      display.print("RUNNING...");
    }

    display.display();
    lastUpdate = millis();
  }

  // 5. Exit (Long Press)
  if (buttonLongPressed) {
    timerRunning = false;
    digitalWrite(PIN_BUZZER, LOW);
    buttonLongPressed = false;
    currentMode = MODE_MENU;
    displayMenu();
  }
}
