# Agricultural Environmental Monitoring System — Milvus Consulting

A hardware and software rework of an agricultural gas monitoring platform, developed in collaboration with **Milvus Consulting** for the *IoT Systems* course at KU Leuven.

[System Architecture](images/architecture.png) 

---

## Background & Goal

The original prototype was a standalone ammonia ($NH_3$) meter built on a perfboard with a compact microcontroller, streaming measurements locally to a PC over a serial RF link. To make the system field-ready for livestock barns and support additional sensors, we redesigned it from the ground up into a custom PCB.

---

## Key Features

* **Core Controller:** `ESP32-S3` on a custom 24V DC PCB (with 12V/5V/3.3V rails, hardware watchdog, and LiPo backup for power outage).
* **Gas Sensing:** combines an NH3 sensor and a CO2 sensor via RS232-to-UART transceivers.
* **Active Flow & Thermal Control:**
  * PID-controlled air pump maintaining a constant **1.5 SLPM** flow rate using an $I^2C$ flow sensor.
  * PID-controlled heating blanket keeping the $NH_3$/$CO_2$ optical cell at a stable **40 °C** to prevent condensation.
* **Ambient Monitoring:** External **Adafruit SHT30** (sintered metal mesh) for barn temperature and relative humidity.
* **Cloud Connectivity:** **Arduino MKR NB 1500** transmitting compact TCP payloads over **NB-IoT** to a web dashboard (wide European coverage, no local gateway needed).
* **Offline Resilience:** Continuous raw data logging to a local **microSD card** with a **RTC** to prevent data loss during network or power outages.

