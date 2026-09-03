# MeshRoom HomeChat Protocol Extensions

`meshroom` extends the base `HomeChat` protocol to provide infrared (IR) remote control for TVs and Air Conditioners (AC), acoustic buzzer controls, Morse code signaling, and onboard temperature telemetry on Raspberry Pi Pico (RP2040/RP2350) platforms.

---

## 1. Television Control (`tv`)

Controls the TV via infrared transmitter:

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `tv on` | Power on the TV. | `tv: pwr=on` |
| `tv off` | Power off the TV. | `tv: pwr=off` |
| `tv toggle` | Toggle TV power state. | `tv: pwr=on` |
| `tv vol [up\|down\|<0-100>]` | Step or set TV volume level (0 to 100). | `tv: vol=15` |
| `tv chan [up\|down\|<1-999>]`| Step or set TV channel number. | `tv: chan=4` |
| `tv mute [on\|off]` | Mute, unmute, or toggle TV audio mute. | `tv: mute=on` |
| `tv input` / `tv source` | Cycle through TV video input sources. | `tv: input=switched` |
| `tv key <digit>` / `tv <0-9>` | Send numeric digit key (0–9) to TV. | `tv: key=5` |
| `tv` | Query current TV state and IR protocol. | `tv: pwr=on vol=15 chan=4 mute=off ir=Sony` |

---

## 2. Air Conditioner Control (`ac`)

Controls AC settings and transmits state via IR blast:

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `ac on` | Turn AC power on. | `ac: pwr=on` |
| `ac off` | Turn AC power off. | `ac: pwr=off` |
| `ac temp [up\|down\|<16-30>]` | Step or set target AC temperature (°C). | `ac: temp=24` |
| `ac mode [cool\|heat\|dry\|fan\|auto]` | Set operating mode. | `ac: mode=cool` |
| `ac fan [auto\|quiet\|low\|med\|high\|max\|<1-5>]` | Set fan speed. | `ac: fan=auto` |
| `ac vane [1-5\|auto\|swing]` | Set swing/vane direction. | `ac: vane=auto` |
| `ac tx` / `ac blast` | Force transmit the current AC state via IR. | `ac: ir=tx` |
| `ac` | Query current AC settings state. | `ac: pwr=on mode=cool temp=24 fan=auto vane=auto` |

---

## 3. Buzzer & Audio Signaling

| Command Syntax | Description | Example Reply |
| :--- | :--- | :--- |
| `buzz <freq> <duration_ms>` | Play tone at frequency (Hz) for duration (ms). | `buzz: freq=1000 dur=500` |
| `morse <text>` | Sound the given text in Morse code on buzzer. | `morse: text='SOS'` |

---

## 4. System & Diagnostics Extensions

- **`reset`**:
  - Displays reset count and seconds since last boot:
    ```text
    reset: count=1 secs_ago=3600
    ```
- **`env`**:
  - In addition to standard BME280/SHT3x telemetry, appends RP2040 onboard temperature:
    ```text
    env: temp=23.5 hum=55.0 press=1013.2 temp_board=26.4
    ```
- **`status`**:
  - Returns `operational`.
