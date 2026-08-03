# MemoBot 


**MemoBot** is an open-source handheld memory game designed to teach embedded programming and electronics through a complete, real-world project.

Unlike typical Arduino examples, MemoBot demonstrates how a complete embedded product is designed, from hardware architecture and power management to firmware organization and user interaction.

> **Project status:** Functional prototype. Firmware is stable and open source. Documentation and educational resources are currently under development.
> 
<img width="711" height="655" alt="image" src="https://github.com/user-attachments/assets/b49ece35-a58d-46fe-a9cb-ba67de3e6953" />

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

### ✔ Included

- Complete MemoBot firmware
- Arduino-compatible source code
- Educational Fritzing schematics  *(**work in progress**)*
- Educational bill of materials (BOM)         *(**work in progress**)*
- Project documentation           *(**work in progress**)*

### ✘ Not Included

- Production PCB design files
- Gerber files
- Manufacturing files
- Commercial enclosure files

---

## Educational Version

In addition to the production hardware, MemoBot is being developed with an educational breadboard version built from through-hole components.

The goal is to let students assemble, understand, modify, and experiment with the firmware before moving to the dedicated hardware.

Compile-time options allow certain hardware-specific features (such as the power latch) to be disabled so the firmware can run on simplified educational circuits.

---

## Running on an Arduino Board

The firmware supports standard Arduino-compatible boards through compile-time hardware configuration options.

Depending on the selected hardware configuration:

- Battery monitoring can be disabled.
- Internal VCC measurement can be used when the microcontroller is powered directly from the battery.
- An external resistor divider can be used to measure battery voltage independently of the regulated MCU supply.
- The buzzer can be configured for either single-ended or differential drive.
- Power latching can be disabled when no external power-latch circuit is present.

When an Arduino board is powered from USB or another regulated supply, internal VCC measurement reflects the regulated supply voltage rather than the battery voltage. In that case, use the external-divider option for meaningful battery monitoring, or disable battery monitoring entirely.

### Using the MemoBot PCB

The dedicated MemoBot PCB powers the ATmega328PB directly from three AAA batteries and includes the optional hardware supported by the firmware.

This allows the firmware to:

- Monitor the battery voltage using the internal VCC measurement method.
- Display the remaining battery level using LED indicators.
- Drive the passive piezo buzzer in differential mode for increased sound output.
- Automatically shut down when the batteries become depleted.
- Control the hardware power latch for ultra-low standby current.
  
---

## Programming

The MemoBot firmware can be compiled for ATmega328-based Arduino boards such as the Uno, Nano, and Pro Mini, as well as the dedicated MemoBot PCB based on the ATmega328PB.

- **Arduino Uno/Nano:** Upload sketches using the onboard USB interface, or use ISP programming if preferred.
- **Arduino Pro Mini:** Upload using a USB-to-Serial adapter or an AVR ISP programmer.
- **MemoBot PCB:** Program through the on-board 6-pin AVR ISP header.

The dedicated **MemoBot PCB** uses an **ATmega328PB** and is intended to be programmed with **MiniCore** using an AVR ISP programmer.

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

## What You'll Learn

MemoBot demonstrates many of the techniques used in real embedded products, including:

- Finite state machines
- Non-blocking firmware design
- Button debouncing and event handling
- Timer-driven audio generation
- Battery monitoring
- Low-power techniques
- Hardware power latching
- Compile-time hardware configuration
- Modular firmware architecture

---

## Roadmap

Planned additions include:

- Assembly guide
- Hardware documentation
- Wiring diagrams
- Hardware test firmware
- Breadboard tutorials
- Classroom exercises
- Additional game modes
- 3D printable enclosure (to be determined)

---

## Getting Started

Whether you're interested in building MemoBot, learning from the project, or experimenting with the hardware, you're welcome to get in touch.

While the firmware is fully open source, I can also provide small-batch hardware on demand, including:

* Fully assembled MemoBot PCBs
* Educational kits
* Individual electronic modules
* Bare PCBs (subject to availability)

If you're interested or have any questions, feel free to contact me:

📧 **[SmartBuildsKits@gmail.com](mailto:SmartBuildsKits@gmail.com)**

Since these are produced in small quantities, availability may vary.

---

## Contributing

Suggestions, bug reports, and improvements are welcome.

If you find a bug or have an idea for making MemoBot more educational or easier to build, feel free to open an Issue or submit a Pull Request.

---

## Support the Project

If you find MemoBot useful for learning, teaching, or your own projects, consider supporting its continued development.

Your support helps fund prototype hardware, documentation, educational content, and future open-source projects.

❤️ **PayPal Donations**

https://paypal.me/sbarab?country.x=CA&locale.x=fr_CA

Thank you for helping make open-source educational hardware available to everyone.

---

## License

The MemoBot firmware is licensed under the **MIT License**.

Documentation, photographs, diagrams, Fritzing schematics, and other educational materials are licensed under the **Creative Commons Attribution 4.0 International License (CC BY 4.0)**, unless otherwise noted.

Production PCB design files, Gerber files, and other hardware design assets are are not part of this open-source release.
