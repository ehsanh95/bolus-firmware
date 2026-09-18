# App

This directory contains the Bolus product-specific firmware.

| Directory | Purpose |
|---|---|
| `Application` | Shared application data contracts and types |
| `BSP` | Board-level GPIO, power, and hardware abstractions |
| `Config` | Hardware and runtime configuration |
| `Drivers` | Low-level device access |
| `Services` | Product-level orchestration and policy |

Architecture rule:

```text
Application models
       ↑
Services
       ↑
Drivers / BSP
       ↑
STM32 HAL
```

Drivers should remain as policy-free as possible. Scheduling, aggregation, retry handling, and fault policy belong in the service layer.
