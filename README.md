# mist
A mist pot based on ESP32 MCU

# Communication

Communication payload format uses Google Protobuf version 2. Encoding and decoding is done by nanopb.

In Mist system, we have:

* **Master**. A powerful ESP32 MCU which manage a set of **sensors**. It is connected to the internet. `ESP-NOW` is used for **master** and **sensor** communication.
* **Sensor**. A MCU has access to variety of sensors. It is manged by **master**, and has Wifi capability but only connects to **master** using `ESP-NOW` for communication.
* **MQTT broker**. It handles the MQTT messages between **master** and **consumer**. **Master** can delegate messages to the **sensor** using specific sensor MAC address in the protobuf message.
* **Consumer**. It is usually a customer facing application: analytics platform, MQTT client, management dashboard and database, whoever consume the MQTT messages.

## MQTT broker

## Master (MQTT client)

## Sensor

## Consumer (MQTT client)

# MQTT

Topics 

/<user_id>/master/<master_mac_address>/sensors
/<user_id>/master/<master_mac_address>/commands
/<user_id>/master/<master_mac_address>/commands/status
