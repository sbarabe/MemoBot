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

The MemoBot PCB is programmed through its on-board 6-pin AVR ISP connector.

An AVR-compatible programmer is required, such as:

- USBasp
- Atmel-ICE
- Arduino configured as ISP

ISP programming is used to:

- upload the firmware
- configure the ATmega328PB fuse settings
- update the firmware without requiring a bootloader

Do not power MemoBot from both the battery pack and the ISP programmer at the same time.

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
