# ESP8266 WebSocket Dashboard

An efficient and responsive real-time RPM dashboard powered by an ESP8266. Built with smooth needle animation, WebSocket communication, and smart optimizations like GZIP compression and heartbeat timeouts.

![{EEE25229-242E-4B6B-8B08-2189DF0232EC}](https://github.com/user-attachments/assets/68c6961a-06cf-4003-b665-8af1d1eaf239)


## Features

- Real-time RPM updates via WebSocket
- Smooth SVG-based needle animation (CSS + JS)
- Auto-reconnecting WebSocket client
- GZIP compression for faster page loads
- Heartbeat-based timeout system for connection health
- Designed for automotive, sim rigs, or embedded displays

## Hardware Requirements

- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- USB cable for flashing
- Optional: Sensor or ECU signals for inputs

## Web UI

- Clean, responsive layout
- CSS-based smooth RPM needle
- Loads directly from LittleFS
- WebSocket client auto-reconnects if connection is lost
