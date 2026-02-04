# Bill of Materials (BOM)
## SAHANC - Spatially Aware Hybrid ANC System

**Last Updated**: February 2026  
**Status**: Draft - Needs team review

---

## 🟢 Already Have
| Component | Qty | Notes |
|-----------|-----|-------|
| IMU (BNO055 or MPU-6050) | 1 | Dae has one |

---

## 🔴 Need to Purchase

### 1. Microcontrollers & Modules

| Component | Qty | Est. Price | Source | Notes |
|-----------|-----|------------|--------|-------|
| **ESP32-S3 DevKit** | 2 | $10 ea | Amazon/DigiKey | One for FDM, one for headset |
| **STM32F446RE Nucleo** | 1 | $15 | DigiKey/Mouser | DSP processing |
| **DWM3000 UWB Module** | 2 | $25 ea | DigiKey/Mouser | Distance ranging (anchor + tag) |

**Subtotal: ~$85**

### 2. Audio Components

| Component | Qty | Est. Price | Source | Notes |
|-----------|-----|------------|--------|-------|
| **I2S MEMS Mic (SPH0645)** | 2 | $8 ea | Adafruit/DigiKey | FDM + Calibration mic |
| **I2S DAC (PCM5102)** | 1 | $5 | Amazon | Audio output |
| **Small Speaker/Driver** | 1 | $10 | Amazon | For testing |
| **3.5mm Headphone Jack** | 2 | $1 ea | - | - |

**Subtotal: ~$33**

### 3. Analog Phase Control Circuit

| Component | Qty | Est. Price | Source | Notes |
|-----------|-----|------------|--------|-------|
| **Op-Amp (TL074 or OPA4134)** | 4 | $2 ea | DigiKey | All-pass filter |
| **Digital Potentiometer (MCP41010)** | 4 | $2 ea | DigiKey | Phase adjustment |
| **Precision Resistors Kit** | 1 | $15 | Amazon | 1% tolerance |
| **Capacitor Kit (film)** | 1 | $12 | Amazon | For filters |
| **Voltage Regulator (±5V)** | 2 | $3 ea | DigiKey | Analog power |

**Subtotal: ~$50**

### 4. Power & Misc

| Component | Qty | Est. Price | Source | Notes |
|-----------|-----|------------|--------|-------|
| **LiPo Battery (2000mAh)** | 2 | $12 ea | Amazon | FDM + Headset |
| **Battery Charger Module** | 2 | $3 ea | Amazon | TP4056 |
| **3.3V Regulator (AMS1117)** | 5 | $1 ea | Amazon | - |
| **PCB Prototype (JLCPCB)** | 5 | $10 total | JLCPCB | 5 boards |
| **Breadboards** | 3 | $5 ea | Amazon | Prototyping |
| **Jumper Wires** | 1 set | $8 | Amazon | - |
| **Headers & Connectors** | 1 set | $10 | Amazon | - |

**Subtotal: ~$73**

### 5. Enclosure & Mechanical

| Component | Qty | Est. Price | Source | Notes |
|-----------|-----|------------|--------|-------|
| **3D Printed Enclosure** | - | $0-20 | Bechtel/Lab | If available |
| **Headband/Mount** | 1 | $15 | Amazon | Modify existing headphones? |
| **Velcro/Mounting Hardware** | 1 | $5 | Amazon | - |

**Subtotal: ~$40**

---

## 💰 Total Estimate

| Category | Cost |
|----------|------|
| MCUs & Modules | $85 |
| Audio | $33 |
| Analog Circuit | $50 |
| Power & Misc | $73 |
| Enclosure | $40 |
| **TOTAL** | **~$281** |

**Per person (4 members): ~$70**

---

## 📦 Recommended Purchase Order

### Phase 1 - Core Testing (Week 1-2)
- [ ] ESP32-S3 DevKit x2
- [ ] STM32 Nucleo
- [ ] I2S MEMS Mics x2
- [ ] Breadboards, jumper wires

### Phase 2 - UWB Integration (Week 3-4)
- [ ] DWM3000 modules x2
- [ ] (IMU already have)

### Phase 3 - Analog Circuit (Week 5-6)
- [ ] Op-amps, digital pots
- [ ] Resistor/capacitor kits
- [ ] PCB order

### Phase 4 - Integration (Week 7+)
- [ ] Batteries, regulators
- [ ] Enclosure materials

---

## 🔗 Quick Links

- [DigiKey DWM3000](https://www.digikey.com/en/products/detail/qorvo/DWM3000/14310381)
- [Adafruit SPH0645 Mic](https://www.adafruit.com/product/3421)
- [Amazon ESP32-S3](https://www.amazon.com/s?k=esp32-s3+devkit)
- [JLCPCB](https://jlcpcb.com/) - $2 for 5 PCBs

---

## ⚠️ Notes

1. **Check Purdue ECE Stock** - Some components might be available in lab
2. **Student Discounts** - DigiKey/Mouser have student programs
3. **Lead Time** - DWM3000 may have 2-3 week lead time
4. **Alternative UWB** - DW1000 is cheaper but older

---

*Update this file as purchases are made. Mark [x] when ordered.*
