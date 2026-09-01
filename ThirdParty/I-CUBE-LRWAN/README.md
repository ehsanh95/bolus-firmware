# I-CUBE-LRWAN vendor staging

This directory is reserved for the STM32 I-CUBE-LRWAN package used by Bolus.

For now these files are staged as third-party/reference content only. Do not add them to the CubeIDE build until the LoRaWAN dependency set is reviewed.

Target layout:

- `Middlewares/Third_Party/LoRaWAN/`
- `Utilities/`
- `Reference/NUCLEO-L476RG/LoRaWAN_End_Node/`

The existing Bolus RFM95W/SX1276 board adaptation remains authoritative until LoRaWAN integration is completed and validated on hardware.
