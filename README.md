# InfiniSense

A capacitor-based energy-harvesting sensor that does not need a battery.

An InfiniSense node wakes from deep sleep when its input changes, reports the
new state over LoRa, and immediately powers back down. Dedicated router nodes
form a mesh so events can travel further than a single radio hop.

## Repository layout

| Path | Contents |
|------|----------|
| [`Firmware/`](Firmware/) | PlatformIO firmware for the nRF52840 + SX1262. Start at [`Firmware/README.md`](Firmware/README.md). |
| [`Hardware/`](Hardware/) | KiCad schematic and PCB. |

## Getting started

- **Firmware:** see [`Firmware/README.md`](Firmware/README.md) for the node
  roles, pin map and build/flash instructions, and
  [`Firmware/CONTRIBUTING.md`](Firmware/CONTRIBUTING.md) if you want to change
  the code.
- **Hardware:** open `Hardware/KicadFiles/InfiniSense.kicad_pro` in
  [KiCad](https://www.kicad.org/).

## License

MIT — see [`LICENSE`](LICENSE).
