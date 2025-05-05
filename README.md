# Automatic-Plant-Watering-System

With the rapid growth of urbanization and increasingly busy lifestyles, maintaining a consistent
plant care routine has become a challenge for many individuals. At the same time, the global
shift toward smart homes and IoT-based solutions has opened new opportunities to automate
everyday tasks for convenience, efficiency, and sustainability. This project aims to address the
need for efficient plant care by developing an Automatic Plant Watering System using the ESP-
IDF framework and ESP RainMaker platform.

Additionally, this project takes switch example from ESP-IDF Rainmaker repository as fundamental file to edit and develop the plant watering system.

# Required Components
ESP32C6-WROOM-1 x1 <br>
Soil Moisture Sensor x1 <br>
5V Relay Module 1 Channel x1 <br>
Water Pump and pipe (3-5V) x1 <br>
1.5v Battery x3 <br>
Battery Holder x1 <br>
Jumper Wires A few <br>
• Male to Female <br>
• Male to Male <br>
Breadboard x1 

# Technical Drawing
Automatic Plant Watering System Circuit Diagram <br>
The circuit diagram below illustrates an automated plant watering system using the ESP32C6 as the main controller. Since the water pump cannot be driven directly by the ESP32C6, a relay module is used for switching, powered by an external battery pack (3 x 1.5V). The ESP32C6 itself is powered via the UART port. 
In the setup, the relay module is connected to GPIO4 of the ESP32C6, and the soil moisture sensor is connected to GPIO2. These two GPIO pins will be referenced in the control code.

<img width="450" alt="image" src="https://github.com/user-attachments/assets/71c1a43a-6dd1-4fc1-b8c0-6148c7ad5c85" />


Automatic Plant Watering System Case Technical Drawing <br>
<img width="452" alt="image" src="https://github.com/user-attachments/assets/f9a28041-1b0d-4532-9e04-932d3442856d" />



### Reset to Factory

Press and hold the BOOT button for more than 3 seconds to reset the board to factory defaults. You will have to provision the board again to use it.# Automatic-Plant-Watering-System

