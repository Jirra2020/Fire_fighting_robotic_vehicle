# RF-Controlled Fire Fighting Robotic Vehicle

> **BSc Final Year Project — Jimma Institute of Technology, School of ECE (2016)**  
> PIC16F877A · MikroC · 433 MHz RF · L293D H-bridge · Servo · GSM SIM900D · LM35 · MQ5 · IR sensors

---

## Overview

A wirelessly controlled fire-fighting robot that can navigate toward a fire, extend its arm to reach elevated positions, and pump water to extinguish the flames. An onboard GSM module provides automatic feedback to the operator via SMS whenever smoke is detected, reporting temperature and obstacle directions in real time.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                        TRANSMITTER SIDE                          │
│   [ 9 Push Buttons ] → [ PIC16F877A ] → [ TWS-433 TX Module ]   │
└──────────────────────────────────────────────────────────────────┘
                                │  RF 433 MHz (range 300–500 m)
                                ▼
┌──────────────────────────────────────────────────────────────────┐
│                         RECEIVER SIDE                            │
│  [ TWR-433 RX ] → [ PIC16F877A ] → [ L293D #1 ] → [ Motor 1 ]  │
│                                  → [ L293D #2 ] → [ Motor 2 ]  │
│                                  → [ PWM2 CCP2 ] → [ Servo  ]  │
│                                  → [ Soft PWM  ] → [ Pump   ]  │
└──────────────────────────────────────────────────────────────────┘
                                │
┌──────────────────────────────────────────────────────────────────┐
│                       GSM FEEDBACK SIDE                          │
│  [ MQ5 Gas Sensor ] → [ PIC16F877A ] ← [ LM35 + 4× IR Sensors ]│
│                              │                                   │
│                       [ MAX232 level shifter ]                   │
│                              │                                   │
│                       [ SIM900D GSM ] → SMS → [ Operator Phone ] │
└──────────────────────────────────────────────────────────────────┘
```

---

## Firmware Modules

| File | Target PIC | Purpose |
|---|---|---|
| `src/transmitter.c` | PIC16F877A | Button panel → RF command transmitter |
| `src/receiver.c` | PIC16F877A | RF commands → motors + servo + pump |
| `src/gsm_feedback.c` | PIC16F877A | Smoke detection → sensor reading → SMS |

Each file is fully independent and flashed to its own PIC.

---

## Module 1 — Transmitter

Reads 9 active-LOW push buttons and sends a single framed character over UART to the 433 MHz RF transmitter module at 2400 baud.

**Frame format:** `,,,,,,,,,,,( X ),,,,,,,,,,,`  
The comma preamble/postamble allows the RF receiver to lock onto the carrier signal before the data arrives.

### Button Map

| Button | Command | Action |
|---|---|---|
| PORTB.F0 | `(A)` | Base — Forward-left |
| PORTB.F1 | `(B)` | Base — Forward-right |
| PORTB.F2 | `(C)` | Base — Forward |
| PORTB.F3 | `(D)` | Base — Reverse |
| PORTB.F4 | `(E)` | Arm — Up |
| PORTB.F5 | `(F)` | Arm — Down |
| PORTB.F6 | `(G)` | Pump — Speed Up |
| PORTB.F7 | `(H)` | Pump — Speed Down |
| PORTA.F0 | `(M)` | Stop Base |

---

## Module 2 — Receiver

Decodes incoming RF commands and drives three actuator systems simultaneously.

### Base Motors — L293D H-bridge on PORTB

| Command | PORTB (hex) | PORTB (binary) | Action |
|---|---|---|---|
| `A` | `0x05` | `0000 0101` | Forward-left |
| `B` | `0x0A` | `0000 1010` | Forward-right |
| `C` | `0x02` | `0000 0010` | Forward |
| `D` | `0x04` | `0000 0100` | Reverse |
| `M` | `0x00` | `0000 0000` | Stop |

> PORTB bit patterns verified against Proteus simulation in the original thesis.

Speed is controlled via **Hardware PWM1** (CCP1 / PORTC.F2) connected to the L293D enable pin.

### Arm Servo — Hardware PWM2 (CCP2 / PORTC.F1) at 50 Hz

| Duty Count | Pulse Width | Angle |
|---|---|---|
| 52 | ≈ 1.0 ms | 0° — fully down |
| 154 | ≈ 1.5 ms | 90° — centre (home) |
| 256 | ≈ 2.0 ms | 180° — fully up |

Commands `E` (arm up) and `F` (arm down) step by 10 counts (~18°) per press.

### Water Pump — Software PWM (PORTC.F0)

The PIC16F877A has only 2 hardware PWM channels (CCP1, CCP2) both already in use. A software PWM is bit-banged on PORTC.F0 to drive the pump motor via a MOSFET or relay.

- `G` — increases pump duty by 25 counts (10 steps from off to full)
- `H` — decreases pump duty by 25 counts
- Pump is OFF at power-up

---

## Module 3 — GSM Feedback

Continuously monitors the **MQ5 gas/smoke sensor** on PORTB.F0. When smoke is detected (pin goes HIGH), the PIC reads all sensors and sends an SMS via the SIM900D module. The message is repeated every **10 seconds** while smoke remains present.

### Sensors

| ADC Channel | Sensor | Description |
|---|---|---|
| ch0 (PORTA.F0) | LM35 | Temperature (°C) |
| ch1 (PORTA.F1) | IR — Left | Sharp GP2Y0A700KOF, range 100–550 cm |
| ch2 (PORTA.F2) | IR — Right | Sharp GP2Y0A700KOF |
| ch3 (PORTA.F3) | IR — Front | Sharp GP2Y0A700KOF |
| ch4 (PORTA.F4) | IR — Back | Sharp GP2Y0A700KOF |
| PORTB.F0 | MQ5 Gas Sensor | Smoke/gas trigger (digital) |

### Example SMS

```
temperature=34 obstacle from=left front
```

### AT Command Sequence Used

| Step | Command | Purpose |
|---|---|---|
| 1 | `AT` | Check modem is alive |
| 2 | `ATE0` | Disable echo |
| 3 | `AT+CMGF=1` | Select text SMS mode |
| 4 | `AT+CMGS="<number>"` | Set recipient and open SMS body |
| 5 | `<body>` + Ctrl-Z | Send the message |

> **To change the recipient number**, update `RECIPIENT_NUMBER` at the top of `gsm_feedback.c`.

> **Note on MAX232:** The SIM900D operates at RS-232 voltage levels. A MAX232 IC is required between the PIC UART pins (PORTC.F6/F7) and the GSM module.

---

## Hardware Summary

| Component | Model | Qty |
|---|---|---|
| Microcontroller | PIC16F877A | 3 |
| RF Transmitter | TWS-433 MHz | 1 |
| RF Receiver | TWR-433 MHz | 1 |
| Motor Driver | L293D | 2 |
| DC Motors (base) | 12V | 2 |
| Servo Motor (arm) | Standard 5V servo | 1 |
| Pump Motor | 12V DC | 1 |
| GSM Module | SIM900D | 1 |
| Level Shifter | MAX232 | 1 |
| Temperature Sensor | LM35 | 1 |
| Gas/Smoke Sensor | MQ5 | 1 |
| IR Distance Sensor | Sharp GP2Y0A700KOF | 4 |
| Push Buttons | 5V, 4-pin | 9 |
| Pull-up Resistors | 10 kΩ | 9 |

---

## Build Instructions

This firmware is written for **MikroC Pro for PIC**.

1. Open MikroC Pro for PIC IDE.
2. Create a new project targeting **PIC16F877A** at **8 MHz**.
3. Add the desired `.c` file from `src/` — one file per project.
4. Set configuration bits: XT oscillator, WDT off, PWRT on, BOD on.
5. Compile and flash using a PICkit 2/3 or ICD programmer.

> **Important:** Each `.c` file is a standalone project. Do not add all three to the same MikroC project — they each contain their own `main()`.

---

## Project Structure

```
rf-firefighting-robot/
├── src/
│   ├── transmitter.c       # Button panel → RF command sender
│   ├── receiver.c          # RF commands → motors, servo, pump
│   └── gsm_feedback.c      # Smoke trigger → sensor SMS via GSM
├── docs/
│   └── at_commands.md      # AT command reference
├── .gitignore
└── README.md
```

---

## Push to GitHub

```bash
git init
git add .
git commit -m "Initial commit: RF fire fighting robot firmware (BSc project 2016)"
gh repo create rf-firefighting-robot --public --source=. --push
```

---

## License

Shared for educational purposes. Free to use, adapt, and build upon.
