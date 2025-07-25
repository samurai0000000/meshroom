This project is used for a device which is made from two components:
  - A Raspberry Pi Pico (rp2040) as the MCU
  - A meshtastic device (E.g. Heltec V3)

The system communicates on a Meshtastic network and accepts commands as
direct messages from a requesting user to control and monitor the room:
  - Environment metrics
  - Air-conditioner infrared transmitter
  - Push button (for broadcast paging)
