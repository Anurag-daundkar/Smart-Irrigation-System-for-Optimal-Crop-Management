#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
#define WIFI_SSID "OnePlus Nord CE 3 Lite 5G"
#define WIFI_PASSWORD "yyyyyyyyyy"

// Firebase credentials
#define FIREBASE_EMAIL "your-email@example.com"
#define FIREBASE_PASSWORD "your-password"
#define FIREBASE_API_KEY "AIzaSyBRSAAoh-CsH_-9ZOetcX-FZEwBetabg14" // Replace with your Firebase API key

// GSM setup using SoftwareSerial on pins 2 (RX), 3 (TX)
SoftwareSerial gsm(2, 3); // RX, TX

// Firebase objects
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;
WiFiClient client;

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2);

void sendSMS(String mobileNumber, int msgVal) {
  if (msgVal < 1 || msgVal > 5 || mobileNumber.length() < 10) return;

  String cmd = "AT+CMGS=\"" + mobileNumber + "\"";
  String message;

  switch (msgVal) {
    case 1: message = "Automation mode started."; break;
    case 2: message = "Automation mode ended."; break;
    case 3: message = "Optimal mode started."; break;
    case 4: message = "Optimal mode Ended."; break;
    case 5: message = "Fertilizer recommendation is ready."; break;
  }

  Serial.println("Sending SMS to: " + mobileNumber);

  gsm.println("AT+CMGF=1"); // Set SMS text mode
  delay(1000);

  gsm.println(cmd); // Set recipient
  delay(1000);

  gsm.println(message); // Message body
  delay(500);

  gsm.write(26); // Ctrl+Z to send SMS
  delay(3000);
}

void checkAndSendSMS() {
  // Get mobile number from Firebase
  if (!Firebase.getString(firebaseData, "/Smart_Irrigation_System/Display/MobileNumber")) {
    Serial.println("Failed to get mobile number from Firebase");
    return;
  }
  String mobileNumber = firebaseData.stringData();
  mobileNumber.replace("\"", "");
  mobileNumber.replace("\\", "");
  mobileNumber.trim();

  // Get message value from Firebase
  if (!Firebase.getInt(firebaseData, "/Smart_Irrigation_System/Display/MessageValue")) {
    Serial.println("Failed to get message value from Firebase");
    return;
  }
  int msgVal = firebaseData.intData();

  if (msgVal >= 1 && msgVal <= 5 && mobileNumber.length() >= 10) {
    sendSMS(mobileNumber, msgVal);
    // Reset message value in Firebase to avoid multiple sends
    Firebase.setInt(firebaseData, "/Smart_Irrigation_System/Display/MessageValue", 0);
  }
}

void setup() {
  Serial.begin(115200);
  gsm.begin(115200); // Most GSM modules use 9600 baud, change if needed

  delay(1000);
  Serial.println("Initializing GSM module...");
  gsm.println("AT");
  delay(1000);
  if (gsm.available()) {
    String response = gsm.readString();
    Serial.println("GSM Module response: " + response);
  } else {
    Serial.println("No response from GSM module.");
  }

  Serial.println("Setting SMS mode to text...");
  gsm.println("AT+CMGF=1");
  delay(1000);
  if (gsm.available()) {
    String response = gsm.readString();
    Serial.println("GSM Module response: " + response);
  } else {
    Serial.println("No response when setting SMS mode.");
  }

  // WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Initialize Firebase
  config.api_key = FIREBASE_API_KEY;
  config.database_url = "https://smart-irrigation-system-d2662-default-rtdb.firebaseio.com/";
  auth.user.email = FIREBASE_EMAIL;
  auth.user.password = FIREBASE_PASSWORD;
  Firebase.begin(&config, &auth);

  if (Firebase.ready()) {
    Serial.println("Firebase initialized successfully");
  } else {
    Serial.println("Failed to initialize Firebase");
    return;
  }

  // Initialize LCD
  lcd.init();
  lcd.backlight();
}

void loop() {
  checkAndSendSMS();

  // Display Mode
  if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Display/Mode")) {
    String Mode = firebaseData.stringData();
    Mode.replace("\"", "");
    Mode.replace("\\", "");
    Mode.trim();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(Mode);
  }

  // Display Running Pump
  if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Display/Running")) {
    String Running = firebaseData.stringData();
    Running.replace("\"", "");
    Running.replace("\\", "");
    Running.trim();

    lcd.setCursor(0, 1);
    lcd.print("Pump " + Running + " is On");
  }

  delay(2000); // Add a small delay to avoid spamming Firebase too often
}
