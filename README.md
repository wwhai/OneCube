# OneCube

OneCube is an Arduino-based project that demonstrates control of LEDs, a buzzer, and an I2C LCD display. The project provides a modular structure for managing hardware components and displaying system information.

## Features

- **LED Control:** Manage four LEDs (Red, Green, Yellow, Blue) with simple color setting and turn-off functions.
- **Buzzer Control:** Generate beeps at different frequencies and durations.
- **LCD Display:** Show messages, CPU/memory usage, temperature/humidity, IP address, and a real-time clock (since a fixed start date).

## Hardware Requirements

- Arduino board (Uno, Mega, etc.)
- 1602 I2C LCD display (default address: `0x27`)
- 4 LEDs (Red, Green, Yellow, Blue)
- 1 Buzzer
- Jumper wires and breadboard

## Pin Configuration

| Component | Pin  |
|-----------|------|
| Red LED   | 2    |
| Green LED | 3    |
| Yellow LED| 4    |
| Blue LED  | 5    |
| Buzzer    | 6    |

## File Structure

```
src/
  ├── main.cpp           # Main Arduino sketch
  ├── LEDControl.cpp     # LED control implementation
  ├── BeepControl.cpp    # Buzzer control implementation
  ├── LCDControl.cpp     # LCD display implementation
include/
  ├── LEDControl.h
  ├── BeepControl.h
  ├── LCDControl.h
```

## Usage

1. **Wiring:** Connect the LEDs and buzzer to the specified pins. Connect the LCD via I2C.
2. **Build and Upload:** Open the project in the Arduino IDE or PlatformIO, select your board, and upload.
3. **Operation:** On startup, the system will:
    - Display a welcome message on the LCD.
    - Cycle through LED colors.
    - Play beeps at different frequencies.
    - Continuously display the date and time since July 1, 2025.

## Customization

- Modify `main.cpp` to change the displayed information or add new features.
- Use the provided methods in `LCDControl` to display custom messages or sensor data.

## License

This project is for educational and