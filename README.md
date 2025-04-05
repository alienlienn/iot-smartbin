# IOT Smart Trash Bin System

A smart trash bin system utilising LoRa devices for bin fill level and location monitoring and real-time tracking on a Django dashboard.

## Table of Contents

- [About](#about)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Usage](#usage)
- [Bins Set-up](#bins-set-up)
- [Beacons Set-up](#beacons-set-up)

## About

The system uses multiple LoRa modules to form a mesh network that enables seamless communication between smart bins. Ultrasonic sensors monitor bin fill levels in real-time, with notifications displayed on a Django dashboard. The system also includes a triangulation-based location feature using three fixed LoRa beacons to determine the relative distance between bins.

## Features

- Real-time bin fill level monitoring via ultrasonic sensors
- Mesh network communication between bins using LoRa
- Triangulation-based bin location tracking with 3 LoRa beacons
- Django dashboard with live status monitoring
- WebSocket integration for live data updates

## Prerequisites

- Python 3.9+
- 8x LoRa hardware (e.g. Cytron Maker Uno with RFM LoRa Shield)
- 3x Utrasonic Sensor

## Installation

1. Clone the repository:
```
git clone https://github.com/alienlienn/iot-smartbin.git
```
2. Download dependency
```
pip install -r requirements.txt
```

## Usage

- Start the Django server to view the dashboard at `http://localhost:8000`
- Run `central.py` to listen for incoming LoRa data
- When a bin's fill level exceeds a threshold, it will be flagged on the dashboard
- Location of bins is updated based on triangulated data from fixed beacons

## Bins Set-up
1. Connect the ultrasonic trigger to pin 3 on the LoRa RFM shield.
2. Connect the ultrasonic echo to pin 4 on the shield.
3. Connect VCC and GND on the shield
4. Utilize the Bin Node Code to set up the system. Update the node number within the code as required.
5. Run the updated code on your computer to activate the smart bin functionality.

## Beacons Set-up



