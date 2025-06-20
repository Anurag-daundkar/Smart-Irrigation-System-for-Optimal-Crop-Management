## 🌾 Smart Irrigation System for Optimal Crop Management

An IoT-powered system to automate and optimize crop irrigation using soil moisture sensors, water level monitoring, smart scheduling, real-time notifications, and a mobile app. Built with NodeMCU, Firebase, and GSM, the system aims to reduce water wastage, save labor, and improve agricultural efficiency.

---

## 🚀 Features

- Real-time soil moisture monitoring (4 lines)
- Water tank level sensing
- Three irrigation modes: Manual, Automatic, Optimal
- Firebase Realtime Database for remote control
- SMS alerts using GSM module (SIM800L)
- LCD display for local status feedback
- Scheduling system based on last irrigation date
- Fertilizer recommendations and climate notifications
- Data logging and visualization via ThingSpeak.
---

## ⚙️ How to Use

  # Flash Arduino Code:
    - Upload NodeMCU_1_MainController.ino to the main NodeMCU.
    - Upload NodeMCU_2_GSM_LCD_Controller.ino to the second NodeMCU.
    - Also not where ever you see long ---------- their you need to make changes
    
  
  # Setup Firebase:
    - Create a Firebase project and structure it based on firebase_structure.json/png in /Firebase_Config/.
  
  # Use the Mobile App:
    - Import Smart_Irrigation_System (2).aia into MIT App Inventor.
    - Replace Firebase API key with your api key
  
  # Power up:
    -Connect sensors, relays, and modules as per the circuit.
    -Ensure Wi-Fi credentials and Firebase API keys are correctly set in the Arduino code.
  
  # Hardware Connections
    - Connections are give in "Circuit Diagram and Pin Info" folder
