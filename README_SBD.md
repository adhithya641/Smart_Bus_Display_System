# Smart Bus Tracking & Passenger Information Display System

An IoT-based Smart Bus Tracking System designed to provide real-time bus location tracking, ETA prediction, and passenger information through a web dashboard and ESP32 TFT display installed at bus stops.

## Features

* Real-time GPS bus tracking
* Admin and passenger dashboards
* ESP32 TFT bus stop display
* MongoDB Atlas cloud database
* Live ETA updates
* Route and schedule management
* Driver location tracking
* Socket.io real-time communication
* SD card support for TFT display
* Physical button navigation system

## Building 
### Block Diagram

  

 ┌──────────────────┐
 │   DRIVER MOBILE  │
 │  GPS Tracking    │
 └────────┬─────────┘
          │ Live Location
          ▼
 ┌──────────────────┐
 │ NODE.JS BACKEND  │
 │ Express + APIs   │
 │ Socket.io + JWT  │
 └────────┬─────────┘
          │
          ▼
 ┌──────────────────┐
 │ MONGODB ATLAS    │
 │ Cloud Database   │
 └───────┬──────────┘
         │
 ┌───────┼───────────────────────┐
 ▼       ▼                       ▼

┌──────────────┐      ┌────────────────┐
│ ADMIN PANEL  │      │ PASSENGER WEB  │
│ Manage Data  │      │ Live Tracking  │
└──────────────┘      └────────────────┘

                ▼
      ┌─────────────────────┐
      │ ESP32 TFT DISPLAY   │
      │ Bus Stop Display    │
      │ Live ETA + Map      │
      └─────────┬───────────┘
                │
      ┌─────────┼─────────┐
      ▼         ▼         ▼
 ┌────────┐ ┌────────┐ ┌──────────┐
 │ TFT UI │ │SD Card │ │ Buttons  │
 └────────┘ └────────┘ └──────────┘

### Connections

ESP32 ↔ 3.5" TFT Display Connections


> TFT Pin	ESP32 Pin
LCD_D0	GPIO 12
LCD_D1	GPIO 13
LCD_D2	GPIO 26
LCD_D3	GPIO 25
LCD_D4	GPIO 18
LCD_D5	GPIO 19
LCD_D6	GPIO 27
LCD_D7	GPIO 14
LCD_RST	GPIO 32
LCD_CS	GPIO 33
LCD_RS	GPIO 15
LCD_WR	GPIO 4
LCD_RD	GPIO 2
5V	VIN
GND	GND

> SD Card Connections
SD Pin	ESP32 Pin
SD_SCK	GPIO 18
SD_DO	GPIO 19
SD_DI	GPIO 23
SD_CS	GPIO 5

>4 Physical Switch Connections


Switch	Function	ESP32 Pin
Switch 1	UP	GPIO 34
Switch 2	DOWN	GPIO 35
Switch 3	SELECT	GPIO 5
Switch 4	BACK	GPIO 12

>Connection for GPIO5 & GPIO12

These support internal pullup.

Simple connection:

GPIO5 ---- Switch ---- GND
GPIO12 --- Switch ---- GND

## Technologies Used

### Frontend

* React.js
* HTML/CSS
* JavaScript

### Backend

* Node.js
* Express.js
* Socket.io
* JWT Authentication

### Database

* MongoDB Atlas

### Hardware

* ESP32
* ILI9488 / ILI9486 TFT Display
* GPS-enabled mobile tracking
* SD Card Module
* Push Buttons

## System Workflow

1. Driver enables live GPS tracking
2. Backend receives location data
3. MongoDB stores bus information
4. Passenger website updates in real-time
5. ESP32 TFT display fetches live bus data
6. Bus stop display shows ETA, routes, and bus details


## TFT Display Features

* Bus arrival list
* ETA timing display
* Route map view
* Bus details page
* SD card support
* Physical button controls

## Future Enhancements

* AI-based ETA prediction
* Voice announcements
* QR ticket booking
* Multi-language support
* Offline caching system

## Author

Adhithya
Electronics and Communication Engineering Student
Embedded Systems & IoT Enthusiast
