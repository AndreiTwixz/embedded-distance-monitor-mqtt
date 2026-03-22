ESP8266 Real-Time Distance Monitoring System
Project Introduction

I developed this project to create a practical IoT solution for monitoring distances in real-time. The system uses an ESP8266 microcontroller and an ultrasonic sensor to gather data, which is then sent over Wi-Fi to a central server using the MQTT protocol. My main focus was to create a stable firmware using FreeRTOS, ensuring that the hardware protection and data transmission work seamlessly together.

1. Hardware Configuration and Safety

For the hardware part of this project, I used the ESP8266 development board and the HC-SR04 ultrasonic sensor. One of the most important aspects of the build was ensuring the safety of the microcontroller.

The sensor operates at 5V, but the ESP8266 pins are only rated for 3.3V. To solve this, I built a voltage divider between the sensor's Echo pin and GPIO 5 of the ESP8266. I used a 1kOhm resistor and a 2kOhm resistor for this purpose.

2. Software Architecture and Logic

The code is written in C using the ESP-IDF framework and follows a professional multitasking approach. Instead of a simple loop, I used FreeRTOS to manage the workload through two separate tasks:

Measurement Task: This task is dedicated to the sensor. It triggers the ultrasonic pulse, measures the timing of the echo, and converts it into centimeters. It runs at a high priority to ensure timing accuracy.

Communication Task: This task handles the MQTT protocol. It waits for new data to be available and then publishes it to the andrei/distanta topic on the local Mosquitto broker.

Data Flow: To connect these two tasks safely, I used a FreeRTOS Queue. The measurement task sends the distance value to the queue, and the MQTT task reads it. This prevents any data loss or system crashes if the network connection is slow.

3. Personal Observations and Conclusion

This project was a great way to practice both hardware protection and real-time software design. Dealing with the voltage levels taught me the importance of checking component datasheets before connecting them. Also, using FreeRTOS made the system much more reliable than a standard Arduino-style sketch. The final result is a fast, responsive IoT node that can be used for various monitoring applications.
