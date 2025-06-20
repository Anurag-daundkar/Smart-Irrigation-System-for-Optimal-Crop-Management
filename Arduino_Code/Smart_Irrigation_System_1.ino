#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <stack>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <WiFiClient.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

using namespace std;

// WiFi credentials
#define WIFI_SSID "YOUR WIFI" --------------------------------------------------------------------------------
#define WIFI_PASSWORD "YOUR PASSWORD"-------------------------------------------------------------------------

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // Adjust the server & offset as needed

//Water level Sensor
const int sensorPin = A0;
int sensorValue = 0;
float waterLevelPercentage = 0.0;

// Pump pins for each line
#define PUMP1_PIN D1
#define PUMP2_PIN D5
#define PUMP3_PIN D6
#define PUMP4_PIN D7

int sensorPins[] = {16, 4, 0, 2}; // GPIO numbers for D0, D2, D3, D4 And these sensors are for Soils
int sensorValues[4];

// Firebase credentials
#define FIREBASE_EMAIL "your-email@example.com"
#define FIREBASE_PASSWORD "your-password"
#define FIREBASE_API_KEY "Replace with your Firebase API key" --------------------------------------------------

// Firebase objects
FirebaseData firebaseData;

// FirebaseConfig and FirebaseAuth objects
FirebaseConfig config;
FirebaseAuth auth;
WiFiClient client;


// Struct for indexed priority scores
struct IndexedValue {
  int value;
  int index;
};

// Function to sort IndexedValue array in descending order
void sortDescending(IndexedValue arr[], int size) {
  for (int i = 0; i < size - 1; i++) 
  {
    for (int j = i + 1; j < size; j++) 
    {
      if (arr[j].value > arr[i].value) 
      {
        IndexedValue temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
    }
  }
}

// Function to sort IndexedValue array in ascending order
void sortAscending(IndexedValue arr[], int size) {
  for (int i = 0; i < size - 1; i++) {
    for (int j = i + 1; j < size; j++) {
      if (arr[j].value < arr[i].value) {
        IndexedValue temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
      }
    }
  }
}

time_t makeTimeStruct(int y, int m, int d) {
  tmElements_t tm;
  tm.Year = y - 1970;
  tm.Month = m;
  tm.Day = d;
  tm.Hour = 0;
  tm.Minute = 0;
  tm.Second = 0;
  return makeTime(tm);
}

// Call this function in loop()
void checkSchedulerOncePerDay() {
  static String lastCheckedDate = "";  // Keeps track of last check

  // Get current date
  timeClient.update();
  time_t rawTime = timeClient.getEpochTime();
  struct tm *timeInfo = localtime(&rawTime);

  char currentDate[11];
  sprintf(currentDate, "%04d-%02d-%02d", timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday);
  String today = String(currentDate);
  today.replace("\"", "");  // Remove quotes
  today.replace("\\", "");  // Remove backslashes
  today.trim(); // Remove any extra spaces

  if (today == lastCheckedDate) return;  // already checked today

  lastCheckedDate = today;  // update last checked

  // Get IntervalDays from Firebase
  if (Firebase.getInt(firebaseData, "/Smart_Irrigation_System/Scheduler/IntervalDays")) {
    int intervalDays = firebaseData.intData();

    // Get LastIrrigationDate from Firebase
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Scheduler/LastIrrigationDate")) {
      String lastDate = firebaseData.stringData();
      lastDate.replace("\"", "");  // Remove quotes
      lastDate.replace("\\", "");  // Remove backslashes
      lastDate.trim(); // Remove any extra spaces

      // Convert both dates to time_t
      int fYear = lastDate.substring(0, 4).toInt();
      int fMonth = lastDate.substring(5, 7).toInt();
      int fDay = lastDate.substring(8, 10).toInt();

      int tYear = today.substring(0, 4).toInt();
      int tMonth = today.substring(5, 7).toInt();
      int tDay = today.substring(8, 10).toInt();

      time_t from = makeTimeStruct(fYear, fMonth, fDay);
      time_t to = makeTimeStruct(tYear, tMonth, tDay);
      int daysPassed = (to - from) / 86400;

      // If interval exceeded, start automatic irrigation
      if (daysPassed >= intervalDays) {
        Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Status", "Started");
        Firebase.setString(firebaseData, "/Smart_Irrigation_System/Scheduler/LastIrrigationDate", today);
        Serial.println("Automatic irrigation triggered.");
      }
    }
  }
}


void setup() {
  Serial.begin(115200);
  
  // Set up WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  timeClient.begin();  // Initialize the NTPClient

  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Initialize Firebase configuration
  config.api_key = FIREBASE_API_KEY;
  config.database_url = "https://smart-irrigation-system-d2662-default-rtdb.firebaseio.com/";

  // Set up the authentication with email and password
  auth.user.email = FIREBASE_EMAIL;
  auth.user.password = FIREBASE_PASSWORD;

  // Initialize Firebase
  Firebase.begin(&config, &auth);



  if (Firebase.ready()) {
    Serial.println("Firebase initialized successfully");
  } else {
    Serial.println("Failed to initialize Firebase");
    return;  // Exit setup if Firebase initialization fails
  }

  // Initialize pump pins
  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);
  pinMode(PUMP3_PIN, OUTPUT);
  pinMode(PUMP4_PIN, OUTPUT);

  // Ensure all pumps are off initially
  digitalWrite(PUMP1_PIN, LOW);
  digitalWrite(PUMP2_PIN, LOW);
  digitalWrite(PUMP3_PIN, LOW);
  digitalWrite(PUMP4_PIN, LOW);

  // Set soil sensor pins as inputs
  for (int i = 0; i < 4; i++) {
    pinMode(sensorPins[i], INPUT);
  }
}

void loop() {

  //Smart Schedular
  checkSchedulerOncePerDay();
  
  sensorValue = analogRead(sensorPin);
  waterLevelPercentage = (sensorValue / 1023.0) * 100.0;

  //Water Level to Firebase
  sensorValue = analogRead(sensorPin);
  waterLevelPercentage = (sensorValue / 1023.0) * 100.0;
  Serial.print("Water Level ");
  Serial.print(": ");
  Serial.println(waterLevelPercentage);
  Firebase.setString(firebaseData, "/Smart_Irrigation_System/Water_Level", waterLevelPercentage);


  //----------------------------------------------------------------------------------FOR MANUALLY---------------------------------------------------------------------------------
  
  Firebase.getString(firebaseData, "/Smart_Irrigation_System/Manual_Feature/Status");
  String Manual_Status = firebaseData.stringData();
  Manual_Status.replace("\"", "");  // Remove quotes
  Manual_Status.replace("\\", "");  // Remove backslashes
  Manual_Status.trim(); // Remove any extra spaces
  String Manual_Status1;
  if(Manual_Status == "Started"){
  
    //PUMP 1
  String pumpPath1 = "/Smart_Irrigation_System/Line1/Status/PumpStatus";
  Serial.print("Checking Firebase path: ");
  Serial.println(pumpPath1);
  if(Firebase.getString(firebaseData, pumpPath1)) 
  {
    Manual_Status1 = firebaseData.stringData();
    Manual_Status1.replace("\"", "");  // Remove quotes
    Manual_Status1.replace("\\", "");  // Remove backslashes
    Manual_Status1.trim(); // Remove any extra spaces
    
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Mode", "Manual Mode");

    String pump1Status = firebaseData.stringData();
    pump1Status.replace("\"", "");  // Remove quotes
    pump1Status.replace("\\", "");  // Remove backslashes
    pump1Status.trim(); // Remove any extra spaces

    Serial.print("PumpStatus Line1: ");
    Serial.println(pump1Status);

    if (pump1Status == "ON") {
      digitalWrite(PUMP1_PIN, LOW);
      Serial.println("Pump 1 ON");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "1");

    } else if (pump1Status == "OFF") {
      digitalWrite(PUMP1_PIN, HIGH);
      Serial.println("Pump 1 OFF");
    }
  } 
  else {
    Serial.print("Failed to read PumpStatus Line1: ");
    Serial.println(firebaseData.errorReason());
  }

  //Soil Sensor to Firebase
  int sensorValue1 = digitalRead(sensorPins[0]);
  Serial.print("Soil Sensor 1");
  Serial.print(": ");
  if (sensorValue1 == LOW) {
    Serial.println("Soil is wet");
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Line1/Status/Soil_Moisture", "Wet");

  } else {
    Serial.println("Soil is dry");
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Line1/Status/Soil_Moisture", "Dry");
  }

  //PUMP 2
  String pumpPath2 = "/Smart_Irrigation_System/Line2/Status/PumpStatus";
  Serial.print("Checking Firebase path: ");
  Serial.println(pumpPath2);
  if (Firebase.getString(firebaseData, pumpPath2)) {
    String pump2Status = firebaseData.stringData();
    pump2Status.replace("\"", "");  // Remove quotes
    pump2Status.replace("\\", "");  // Remove backslashes
    pump2Status.trim(); // Remove any extra spaces

    Serial.print("PumpStatus Line2: ");
    Serial.println(pump2Status);

    if (pump2Status == "ON") {
      digitalWrite(PUMP2_PIN, LOW);
      Serial.println("Pump 2 ON");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "2");
    } else if (pump2Status == "OFF") {
      digitalWrite(PUMP2_PIN, HIGH);
      Serial.println("Pump 2 OFF");
    }
  } else {
    Serial.print("Failed to read PumpStatus Line2: ");
    Serial.println(firebaseData.errorReason());
  }

  //Soil Sensor to Firebase
  int sensorValue2 = digitalRead(sensorPins[1]);
  Serial.print("Soil Sensor 2");
  Serial.print(": ");
  if (sensorValue2 == LOW) {
    Serial.println("Soil is wet");
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Line2/Status/Soil_Moisture", "Wet");
  } else {
    Serial.println("Soil is dry");
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Line2/Status/Soil_Moisture", "Dry");
  }


  //PUMP 3
  String pumpPath3 = "/Smart_Irrigation_System/Line3/Status/PumpStatus";
  Serial.print("Checking Firebase path: ");
  Serial.println(pumpPath3);
  if (Firebase.getString(firebaseData, pumpPath3)) {
    String pump3Status = firebaseData.stringData();
    pump3Status.replace("\"", "");  // Remove quotes
    pump3Status.replace("\\", "");  // Remove backslashes
    pump3Status.trim(); // Remove any extra spaces

    Serial.print("PumpStatus Line3: ");
    Serial.println(pump3Status);

    if (pump3Status == "ON") {
      digitalWrite(PUMP3_PIN, LOW);
      Serial.println("Pump 3 ON");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "3");
    } else if (pump3Status == "OFF") {
      digitalWrite(PUMP3_PIN, HIGH);
      Serial.println("Pump 3 OFF");
    }
  } else {
    Serial.print("Failed to read PumpStatus Line3: ");
    Serial.println(firebaseData.errorReason());
  }

  //Soil Sensor to Firebase
  int sensorValue3 = digitalRead(sensorPins[2]);
  Serial.print("Soil Sensor 3");
  Serial.print(": ");
  if (sensorValue3 == LOW) {
    Serial.println("Soil is wet");
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Line3/Status/Soil_Moisture", "Wet");
  } else {
    Serial.println("Soil is dry");
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Line3/Status/Soil_Moisture", "Dry");
  }


  //PUMP 4
  String pumpPath4 = "/Smart_Irrigation_System/Line4/Status/PumpStatus";
  Serial.print("Checking Firebase path: ");
  Serial.println(pumpPath4);
  if (Firebase.getString(firebaseData, pumpPath4)) {
    String pump4Status = firebaseData.stringData();
    pump4Status.replace("\"", "");  // Remove quotes
    pump4Status.replace("\\", "");  // Remove backslashes
    pump4Status.trim(); // Remove any extra spaces

    Serial.print("PumpStatus Line4: ");
    Serial.println(pump4Status);

    if (pump4Status == "ON") {
      digitalWrite(PUMP4_PIN, LOW);
      Serial.println("Pump 4 ON");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "4");
    } else if (pump4Status == "OFF") {
      digitalWrite(PUMP4_PIN, HIGH);
      Serial.println("Pump 4 OFF");
    }
    }
   else {
     Serial.print("Failed to read PumpStatus Line4: ");
     Serial.println(firebaseData.errorReason());
    }

    //Soil Sensor to Firebase
    int sensorValue4 = digitalRead(sensorPins[3]);
    Serial.print("Soil Sensor 4");
    Serial.print(": ");
    if (sensorValue4 == LOW) {
     Serial.println("Soil is wet");
     Firebase.setString(firebaseData, "/Smart_Irrigation_System/Line4/Status/Soil_Moisture", "Wet");
    } 
    else {
      Serial.println("Soil is dry");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Line4/Status/Soil_Moisture", "Dry");
    }
  }
  else if(Manual_Status == "Stopped")
  {
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "Stopped");
    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Mode", "None");
  }
  
  //-----------------------------------------------------------------------------------For Automation-------------------------------------------------------------------------------

  sensorValue = analogRead(sensorPin);
  waterLevelPercentage = (sensorValue / 1023.0) * 100.0;

  //Water Level to Firebase
  sensorValue = analogRead(sensorPin);
  waterLevelPercentage = (sensorValue / 1023.0) * 100.0;
  Serial.print("Water Level ");
  Serial.print(": ");
  Serial.println(waterLevelPercentage);
  Firebase.setString(firebaseData, "/Smart_Irrigation_System/Water_Level", waterLevelPercentage);

  Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Status");
  String Automation_Status = firebaseData.stringData();
  Automation_Status.replace("\"", "");  // Remove quotes
  Automation_Status.replace("\\", "");  // Remove backslashes
  Automation_Status.trim(); // Remove any extra spaces
  if(Automation_Status == "Started"){

        Firebase.setInt(firebaseData, "/Smart_Irrigation_System/Display/MessageValue", 1);


    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Mode", "Automatic Mode");
    
    String Automation_array[4]={};

    IndexedValue scores[4];
    int allScoresFetched=0;
    

    //Line 1 P score
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line1/Score")) {
      String P_Score1 = firebaseData.stringData();
      P_Score1.replace("\"", "");  // Remove quotes
      P_Score1.replace("\\", "");  // Remove backslashes
      P_Score1.trim(); // Remove any extra spaces

      Automation_array[0]=P_Score1;
      allScoresFetched=allScoresFetched+1;

      scores[0].value = firebaseData.stringData().toInt(); // Convert to integer
      scores[0].index = 0;

    } else {
      Serial.print("Failed to read Priority Score: ");
      Serial.println(firebaseData.errorReason());
    }

    //Line 2 P Score 
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line2/Score")) {
      String P_Score2 = firebaseData.stringData();
      P_Score2.replace("\"", "");  // Remove quotes
      P_Score2.replace("\\", "");  // Remove backslashes
      P_Score2.trim(); // Remove any extra spaces

      Automation_array[1]=P_Score2;
      allScoresFetched=allScoresFetched+1;

      scores[1].value = firebaseData.stringData().toInt(); // Convert to integer
      scores[1].index = 1;

    } else {
      Serial.print("Failed to read Priority Score: ");
      Serial.println(firebaseData.errorReason());
    }

    //Line 3 P Score 
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line3/Score")) {
      String P_Score3 = firebaseData.stringData();
      P_Score3.replace("\"", "");  // Remove quotes
      P_Score3.replace("\\", "");  // Remove backslashes
      P_Score3.trim(); // Remove any extra spaces

      Automation_array[2]=P_Score3;
      allScoresFetched=allScoresFetched+1;

      scores[2].value = firebaseData.stringData().toInt(); // Convert to integer
      scores[2].index = 2;

    } else {
      Serial.print("Failed to read Priority Score: ");
      Serial.println(firebaseData.errorReason());
    }

    //Line 4 P Score 
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line4/Score")) {
      String P_Score4 = firebaseData.stringData();
      P_Score4.replace("\"", "");  // Remove quotes
      P_Score4.replace("\\", "");  // Remove backslashes
      P_Score4.trim(); // Remove any extra spaces

      Automation_array[3]=P_Score4;
      allScoresFetched=allScoresFetched+1;

      scores[3].value = firebaseData.stringData().toInt(); // Convert to integer
      scores[3].index = 3;

    } else {
      Serial.print("Failed to read Priority Score: ");
      Serial.println(firebaseData.errorReason());
    }

    //---------------------------------------------------------------------------

    if (allScoresFetched == 4) 
    {
      // Sort scores in descending order
      sortDescending(scores, 4);

      // Debug: Print sorted scores
      Serial.println("Sorted Priority Scores:");
      for (int i = 0; i < 4; i++) {
        Serial.print("Line ");
        Serial.print(scores[i].index + 1);
        Serial.print(": ");
        Serial.println(scores[i].value);

        Serial.println("Testing 1");
      }
        Serial.println("Testing 2");

      for (int i = 0; i < 4; i++) {
        Serial.println("Testing 3");
        Serial.println(scores[i].index);
      }

      Serial.println("Testing 4");


      int soil1,soil2,soil3,soil4;
      
      for (int i = 0; i < 4; i++) 
      {
                Serial.println("Testing 5");

        switch ((scores[i].index+1)) 
        {
                  Serial.println("Testing 6");

          Serial.println(scores[i].index);
          case 1: 
          soil1=digitalRead(sensorPins[0]);
          while(!(soil1 == LOW))
          {
                    Serial.println("Testing 7");

            digitalWrite(PUMP1_PIN, LOW);
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line1/PumpStatus", "ON");
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "1");
            soil1=digitalRead(sensorPins[0]);
          }
          digitalWrite(PUMP1_PIN, HIGH);
          Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line1/PumpStatus", "OFF");
          break;

          case 2: 
                  Serial.println("Testing 8");

          soil2=digitalRead(sensorPins[1]);
          while(!(soil2 == LOW))
          {
            digitalWrite(PUMP2_PIN, LOW);
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line2/PumpStatus", "ON");
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "2");
            soil2=digitalRead(sensorPins[1]);
          }
          digitalWrite(PUMP2_PIN, HIGH);
          Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line2/PumpStatus", "OFF");
          break;

          case 3: 
          soil3=digitalRead(sensorPins[2]);
          while(!(soil3 == LOW))
          {
            digitalWrite(PUMP3_PIN, LOW);
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line3/PumpStatus", "ON");
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "3");
            soil3=digitalRead(sensorPins[2]);
          }
          digitalWrite(PUMP3_PIN, HIGH);
          Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line3/PumpStatus", "OFF");
          break;

          case 4: 
          soil4=digitalRead(sensorPins[3]);
          while(!(soil4 == LOW))
          {
            digitalWrite(PUMP4_PIN, LOW);
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line4/PumpStatus", "ON");
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "4");
            soil4=digitalRead(sensorPins[3]);
          }
          digitalWrite(PUMP4_PIN, HIGH);
          Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line4/PumpStatus", "OFF");
          break;
        }
      }
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Status", "Stopped");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "Stopped");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Mode", "None");

          Firebase.setInt(firebaseData, "/Smart_Irrigation_System/Display/MessageValue", 2);


    }
  }

  //-----------------------------------------------------------------------------------For Optimally-------------------------------------------------------------------------------

  sensorValue = analogRead(sensorPin);
  waterLevelPercentage = (sensorValue / 1023.0) * 100.0;

  //Water Level to Firebase
  sensorValue = analogRead(sensorPin);
  waterLevelPercentage = (sensorValue / 1023.0) * 100.0;
  Serial.print("Water Level ");
  Serial.print(": ");
  Serial.println(waterLevelPercentage);
  Firebase.setString(firebaseData, "/Smart_Irrigation_System/Water_Level", waterLevelPercentage);

  Firebase.getString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Status");
  String Optimal_Status = firebaseData.stringData();
  Optimal_Status.replace("\"", "");  // Remove quotes
  Optimal_Status.replace("\\", "");  // Remove backslashes
  Optimal_Status.trim(); // Remove any extra spaces
  if(Optimal_Status == "Started"){

        Firebase.setInt(firebaseData, "/Smart_Irrigation_System/Display/MessageValue", 3);


    Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Mode", "Manual Mode");
    
    String Optimal_array[4]={};

    IndexedValue scores[4];
    int allScoresFetched=0;
    

    //Line 1 P score
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line1/Score")) {
      String P_Score1 = firebaseData.stringData();
      P_Score1.replace("\"", "");  // Remove quotes
      P_Score1.replace("\\", "");  // Remove backslashes
      P_Score1.trim(); // Remove any extra spaces

      Optimal_array[0]=P_Score1;
      allScoresFetched=allScoresFetched+1;

      scores[0].value = firebaseData.stringData().toInt(); // Convert to integer
      scores[0].index = 0;

      //Giving CROP CONDITION
      if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Line1_crop_condition")) {
        String Condition1 = firebaseData.stringData();
        Condition1.replace("\"", "");  // Remove quotes
        Condition1.replace("\\", "");  // Remove backslashes
        Condition1.trim(); // Remove any extra spaces

        if (Condition1 == "Green") {
        } 
        else if (Condition1 == "Moderate") {
          scores[0].value=scores[0].value*100;
        }
        else if(Condition1 == "Dry")
        {
            Serial.println("Line 1 is completely dry so no need of irrigating it");
            scores[0].value=scores[0].value*0;
        }
      } 
      else {
      Serial.print("Failed to read Crop condition: ");
      Serial.println(firebaseData.errorReason());
      }

    } else {
      Serial.print("Failed to read Priority Score: ");
      Serial.println(firebaseData.errorReason());
    }

    //Line 2 P Score 
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line2/Score")) {
      String P_Score2 = firebaseData.stringData();
      P_Score2.replace("\"", "");  // Remove quotes
      P_Score2.replace("\\", "");  // Remove backslashes
      P_Score2.trim(); // Remove any extra spaces

      Optimal_array[1]=P_Score2;
      allScoresFetched=allScoresFetched+1;

      scores[1].value = firebaseData.stringData().toInt(); // Convert to integer
      scores[1].index = 1;

      //Giving CROP CONDITION
      if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Line2_crop_condition")) {
        String Condition2 = firebaseData.stringData();
        Condition2.replace("\"", "");  // Remove quotes
        Condition2.replace("\\", "");  // Remove backslashes
        Condition2.trim(); // Remove any extra spaces

        if (Condition2 == "Green") {
        } 
        else if (Condition2 == "Moderate") {
          scores[1].value=scores[1].value*100;
        }
        else if(Condition2 == "Dry")
        {
            Serial.println("Line 2 is completely dry so no need of irrigating it");
            scores[1].value=scores[1].value*0;
        }
      } 
      else {
      Serial.print("Failed to read Crop condition: ");
      Serial.println(firebaseData.errorReason());
      }

    } else {
      Serial.print("Failed to read Priority Score: ");
      Serial.println(firebaseData.errorReason());
    }

    //Line 3 P Score 
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line3/Score")) {
      String P_Score3 = firebaseData.stringData();
      P_Score3.replace("\"", "");  // Remove quotes
      P_Score3.replace("\\", "");  // Remove backslashes
      P_Score3.trim(); // Remove any extra spaces

      Optimal_array[2]=P_Score3;
      allScoresFetched=allScoresFetched+1;

      scores[2].value = firebaseData.stringData().toInt(); // Convert to integer
      scores[2].index = 2;

      //Giving CROP CONDITION
      if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Line3_crop_condition")) {
        String Condition3 = firebaseData.stringData();
        Condition3.replace("\"", "");  // Remove quotes
        Condition3.replace("\\", "");  // Remove backslashes
        Condition3.trim(); // Remove any extra spaces

        if (Condition3 == "Green") {
        } 
        else if (Condition3 == "Moderate") {
          scores[2].value=scores[2].value*100;
        }
        else if(Condition3 == "Dry")
        {
            Serial.println("Line 3 is completely dry so no need of irrigating it");
            scores[2].value=scores[2].value*0;
        }
      } 
      else {
      Serial.print("Failed to read Crop condition: ");
      Serial.println(firebaseData.errorReason());
      }

    } else {
      Serial.print("Failed to read Priority Score: ");
      Serial.println(firebaseData.errorReason());
    }

    //Line 4 P Score 
    if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Automatic_Feature/Priority_Score_of_line4/Score")) {
      String P_Score4 = firebaseData.stringData();
      P_Score4.replace("\"", "");  // Remove quotes
      P_Score4.replace("\\", "");  // Remove backslashes
      P_Score4.trim(); // Remove any extra spaces

      Optimal_array[3]=P_Score4;
      allScoresFetched=allScoresFetched+1;

      scores[3].value = firebaseData.stringData().toInt(); // Convert to integer
      scores[3].index = 3;

      //Giving CROP CONDITION
      if (Firebase.getString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Line4_crop_condition")) {
        String Condition4 = firebaseData.stringData();
        Condition4.replace("\"", "");  // Remove quotes
        Condition4.replace("\\", "");  // Remove backslashes
        Condition4.trim(); // Remove any extra spaces

        if (Condition4 == "Green") {
        } 
        else if (Condition4 == "Moderate") {
          scores[3].value=scores[3].value*100;
        }
        else if(Condition4 == "Dry")
        {
            Serial.println("Line 4 is completely dry so no need of irrigating it");
            scores[3].value=scores[3].value*0;
        }
      } 
      else {
      Serial.print("Failed to read Crop condition: ");
      Serial.println(firebaseData.errorReason());
      }

    } else {
      Serial.print("Failed to read Priority Score: ");
      Serial.println(firebaseData.errorReason());
    }

    //---------------------------------------------------------------------------

    if (allScoresFetched == 4) 
    {
      // Sort scores in descending order
      sortDescending(scores, 4);

      // Debug: Print sorted scores
      Serial.println("Sorted Priority Scores:");
      for (int i = 0; i < 4; i++) {
        Serial.print("Line ");
        Serial.print(scores[i].index + 1);
        Serial.print(": ");
        Serial.println(scores[i].value);
      }
      
      int soil1,soil2,soil3,soil4;
      
      for (int i = 0; i < 4; i++) 
      {
        switch (scores[i].index+1) 
        {
          case 1: 
          soil1=digitalRead(sensorPins[0]);
          while(!(soil1 == LOW))
          {
            digitalWrite(PUMP1_PIN, LOW);
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Priority_Score_of_line1/PumpStatus", "ON");
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "1");
            soil1=digitalRead(sensorPins[0]);
          }
          digitalWrite(PUMP1_PIN, HIGH);
          Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Priority_Score_of_line1/PumpStatus", "OFF");
          break;

          case 2: 
          soil2=digitalRead(sensorPins[1]);
          while(!(soil2 == LOW))
          {
            digitalWrite(PUMP2_PIN, LOW);
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Priority_Score_of_line2/PumpStatus", "ON");
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "2");
            soil2=digitalRead(sensorPins[1]);
          }
          digitalWrite(PUMP2_PIN, HIGH);
          Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Priority_Score_of_line2/PumpStatus", "OFF");
          break;

          case 3: 
          soil3=digitalRead(sensorPins[2]);
          while(!(soil3 == LOW))
          {
            digitalWrite(PUMP3_PIN, LOW);
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Priority_Score_of_line3/PumpStatus", "ON");
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "3");
            soil3=digitalRead(sensorPins[2]);
          }
          digitalWrite(PUMP3_PIN, HIGH);
          Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Priority_Score_of_line3/PumpStatus", "OFF");
          break;

          case 4: 
          soil4=digitalRead(sensorPins[3]);
          while(!(soil4 == LOW))
          {
            digitalWrite(PUMP4_PIN, LOW);
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Priority_Score_of_line4/PumpStatus", "ON");
            Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "4");
            soil4=digitalRead(sensorPins[3]);
          }
          digitalWrite(PUMP4_PIN, HIGH);
          Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Priority_Score_of_line4/PumpStatus", "OFF");
          break;
        }
      }
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Optimal_Feature/Status", "Stopped");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Running", "Stopped");
      Firebase.setString(firebaseData, "/Smart_Irrigation_System/Display/Mode", "None");

          Firebase.setInt(firebaseData, "/Smart_Irrigation_System/Display/MessageValue", 4);

    }
  }
  sensorValue = analogRead(sensorPin);
  waterLevelPercentage = (sensorValue / 1023.0) * 100.0;

  //Water Level to Firebase
  sensorValue = analogRead(sensorPin);
  waterLevelPercentage = (sensorValue / 1023.0) * 100.0;
  Serial.print("Water Level ");
  Serial.print(": ");
  Serial.print(waterLevelPercentage);
  Firebase.setString(firebaseData, "/Smart_Irrigation_System/Water_Level", waterLevelPercentage);
}
