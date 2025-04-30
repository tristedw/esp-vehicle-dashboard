# ESP8266 WebSocket Dashboard

An efficient, self-hosted real-time vehicle dashboard powered by an ESP8266. Designed with smooth needle animation using [CanvasGauges](https://canvas-gauges.com/), fast WebSocket communication, and optimizations like GZIP compression.

![dashgif](https://github.com/user-attachments/assets/c9d0ad6e-0117-4890-9a22-09e2b994efaf)

## Features

- Real-time RPM updates via WebSocket
- Smooth SVG-based needle animation (CSS + JS)
- Auto-reconnecting WebSocket client
- GZIP compression for faster page loads
- Heartbeat-based timeout system for connection health
- Fully self-hosted on the ESP8266 – no external server needed
- Ideal for mounting a tablet in your car to monitor live engine data

## Hardware Requirements

- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- USB cable for flashing
- Optional: Sensor or ECU signals for inputs

## Web UI

- Clean, responsive layout
- CSS-based smooth RPM needle
- Loads directly from LittleFS
- WebSocket client auto-reconnects if connection is lost
