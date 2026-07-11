# MemoBot

**MemoBot** is an open-source handheld memory game designed to teach embedded programming and electronics through a complete, real-world project.

Inspired by the classic electronic memory game, MemoBot combines custom hardware and firmware into an educational platform suitable for hobbyists, students, and makers interested in learning how embedded systems work.

> **Project status:** Functional prototype. Firmware is stable and open source. Documentation and educational resources are currently under development.

---

## Features

- 🎮 Memory games (with additional game modes planned)
- 💡 Four illuminated push buttons
- 🔊 Piezo buzzer with sound effects and melodies
- 🔋 Battery-powered (3 × AAA)
- ⚡ Ultra-low-power design with automatic power latching
- 📈 Battery voltage monitoring and level indication
- 🔌 On-board ISP programming connector for firmware upload and fuse configuration
- ⚙️ Built around the ATmega328PB microcontroller
- 🔧 Compatible with Arduino Uno, Nano and Pro Mini (ATmega328-based)
- 📚 Designed for learning embedded programming and electronics

---

## Repository Contents

This repository currently includes:

- Complete MemoBot firmware
- Arduino-compatible source code
- Educational Fritzing schematic
- Bill of Materials (BOM)
- Project documentation (work in progress)

This repository intentionally does **not** include:

- Production PCB design files
- Gerber files
- Manufacturing files
- Commercial enclosure files

---

## Educational Version

In addition to the production hardware, MemoBot is being developed with an educational breadboard version built from through-hole components.

The goal is to allow students to assemble, understand, modify, and experiment with the same firmware before moving to the finished hardware.

Compile-time options allow certain hardware-specific features (such as the power latch) to be disabled so the firmware can run on simplified educational circuits.

---

## Programming

The MemoBot firmware is compatible with ATmega328-based Arduino boards such as the Arduino Uno, Nano, and Pro Mini.

- **Arduino Uno/Nano:** Upload sketches using the onboard USB interface, or use ISP programming if preferred.
- **Arduino Pro Mini:** Upload using a USB-to-Serial adapter or an AVR ISP programmer.
- **MemoBot PCB:** Program through the on-board 6-pin AVR ISP header.

Compatible AVR ISP programmers include:

- USBasp
- Atmel-ICE
- Arduino configured as an ISP programmer

ISP programming allows you to:

- Upload the firmware
- Configure the ATmega328PB fuse settings
- Program the microcontroller directly, eliminating the need for a bootloader and reducing startup time.

> **Warning:** Do not power the MemoBot PCB from both the battery pack and the ISP programmer at the same time.

---

## Roadmap

Planned additions include:

- Assembly guide
- Hardware documentation
- Wiring diagrams
- Classroom exercises
- Breadboard tutorials
- Hardware test firmware
- Additional game modes
- 3D printable enclosure (to be determined)

---

## Contributing

Suggestions, bug reports, and improvements are welcome.

If you find a bug or have an idea for making MemoBot more educational or easier to build, feel free to open an Issue or submit a Pull Request.

---

## License

The MemoBot firmware is licensed under the **MIT License**.

Documentation, photographs, diagrams, Fritzing schematics, and other educational materials are licensed under the **Creative Commons Attribution 4.0 International License (CC BY 4.0)**, unless otherwise noted.

Production PCB design files, Gerber files, and other proprietary hardware assets are intentionally not included in this repository.
