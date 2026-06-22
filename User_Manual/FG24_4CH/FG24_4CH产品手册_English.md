# FG24_4CH GMSL Camera Platform Product Manual

**Zhuying Intelligent Robot (Shenzhen) Co., Ltd.**

---

## 1. Product Introduction

FG24_4CH GMSL Camera Platform is an expansion board that allows up to 4 cameras to connect to Jetson Orin/Xavier NX modules. It is fully compatible with NVIDIA Jetson Orin/Xavier NX Devkit.

### Product Introduction

●The  FG24_4CH  GMSL  Board  is  a  4-channel  GMSL  capture  board  developed  by  Zhuying  Intelligent  /  Fangzhu
Technology based on the ADI Deserializer chip MAX96724, hereinafter referred to as the Capture Board.
●This capture board supports up to 4 GMSL cameras simultaneously and outputs  CSI  signals to the NVIDIA® Jetson Orin™ NX Development Kit (also compatible with the Xavier NX Development Kit).
●Through software configuration, the MAX96724 can adaptively decode GMSL1 and GMSL2 cameras at different rates.
●The 22Pin connector on the NVIDIA Jetson® Orin/Xavier™  NX Devkit  cannot  supply  sufficient  power  for  cameras.Therefore, the capture board uses a pluggable 12V–24V DC external power  connector (can  share  the  same  poweradapter as the kit).

facilitating installation and use in applications such as Autonomous Mobile Robots (AMR), Unmanned Aerial Vehicles (UAV) and automotive systems.

### 1.1 Product Images

![Product Image 1](../images/FG24_4CH/pic1.png)

![Product Image 2](../images/FG24_4CH/pic2.png)

![Product Image 3](../images/FG24_4CH/pic3.png)

### 1.2 Product Features

| Feature | Description |
|---------|-------------|
| Protocol Compatibility | GMSL2 and GMSL1 protocol compatible |
| Power Supply | Power over Coax |
| Voltage Output | Camera power supply supports wide voltage output |
| Protection | Built-in overvoltage and short-circuit protection |
| Power Control | Camera power can be controlled by software |
| Size | Compact size for Jetson Orin/Xavier NX Devkit |

### 1.3 Product Specifications

| Item | Specification |
|------|--------------|
| Dimensions | 80mm × 53mm |
| Weight | 40g |
| Jetson Devkit Interface | Molex 22Pin ZIF connector (4Lane CSI) |
| Camera Inputs | 4-channel GMSL2 or GMSL1 cameras |
| Deserializer | ADI MAX96724 |
| MIPI Output | 2/4-Lane CSI-2 v1.3 |
| Camera Connector | Amphenol single-end horizontal FAKRA |
| PoC Power Supply | 4-channel PoC 12V |
| Power Input | Supports 12V~24V @ 3A MAX |
| Operating Temperature | -20°C ~ +65°C |
| Warranty | One year warranty and technical support |



---

## 3. Safety Instructions

Before using the product, you must read this document carefully to gain a preliminary understanding of the product. Always comply with the safety instructions in this manual to ensure personal safety and avoid equipment damage. The manufacturer shall not be liable for any damage to equipment or personal life and property caused by incorrect operation.

### 3.1 Personal Safety

- Before operating the equipment, wear anti-static work clothes, anti-static gloves or wristbands, and remove jewelry, watches, and other conductive objects to avoid electric shock or burns.
- In case of fire, evacuate the building or equipment area and press the fire alarm bell, or call the fire department. Under no circumstances should you re-enter a burning building.

### 3.2 Power Supply Voltage

- The FG24_4CH carrier board supports input voltage range: 12V~24V, current: 2A or above

### 3.3 Environmental Requirements

- Operating temperature: -20°C ~ 65°C
- Ventilation: Good ventilation must be provided around the installed computing platform.
- Grounding: The power adapter must be properly grounded. In special scenarios, grounding screws may be required for earth connection.

### 3.4 ESD Protection

Electronic components and circuits are sensitive to electrostatic discharge. Although our company designs anti-static protection for main interfaces on the board, it is difficult to protect all components and circuits. Therefore, when handling any circuit board components, please follow these ESD protection measures:

1. During transportation and storage, keep the board in an anti-static bag until installation.
2. Before touching the board, discharge static electricity from your body by wearing a grounding wristband.
3. Only operate the board in an ESD-safe area.
4. Avoid moving the board in carpeted areas.

---

## 4. Block Diagram

### 4.1 Diagram Description

![Block Diagram](../images/FG24_4CH/block_diagram.png)

**Notes:**

1. I2C bus numbers correspond to hardware positions (matching connector J12, J13 pins). Bus numbers may not correspond to those listed in software.
2. Coaxial power is shared, but each GMSL line has its own PoC filter.

---

## 5. Component Placement Diagram

### 5.1 Placement Layout

![Component Placement](../images/FG24_4CH/component_placement.png)

---

## 6. External Interfaces

### 7.1 Interface Overview

| No. | Name | ID | Remarks |
|-----|------|-----|---------|
| 1 | Power Input | J1 | 12V~24V DC |
| 2 | CAMA | J2 | FAKRA connector |
| 3 | CAMB | J3 | FAKRA connector |
| 4 | CAMC | J4 | FAKRA connector |
| 5 | CAMD | J5 | FAKRA connector |
| 6 | External Trigger | J11 | 2.54-2*3P |
| 7 | CSI0 | J12 | Molex 22Pin ZIF |
| 8 | CSI1 | J13 | Molex 22Pin ZIF |

---

## 7. Interface Functions

### 7.1 J1 - Power Input Interface

**Function:** POWER input

**ID:** J1

**Type/Model:** JPD1030-N521-4F

| Pin | Signal |
|-----|--------|
| 1 | 12V~24V |
| 2 | GND |
| 3 | GND |

### 7.2 J11 - External Trigger Connector

**Function:** External trigger signal input

**ID:** J11

**Type/Model:** 2.54-2*3P

| Pin | Signal | Pin | Signal |
|-----|--------|-----|--------|
| 1 | MAX1_CSI0_SYNC | 4 | MAX1_CSI3_SYNC |
| 2 | MAX1_CSI2_SYNC | 5 | MAX1_ALL_SYNC |
| 3 | MAX1_CSI1_SYNC | 6 | GND |

### 7.2 J2/J3/J4/J5 - FAKRA Connectors

**Function:** GMSL camera interface

**ID:** J2, J3, J4, J5

**Type/Model:** Amphenol FK1252ZW-031-TLCP5G-50

#### 8.3.1 FAKRA Pin Definition

| Pin | Signal |
|-----|--------|
| 1 | MAX1_SIOx_P |
| 2 | GND |
| 3 | GND |
| 4 | GND |
| 5 | GND |

#### 8.3.2 Physical Interface to Device Node Mapping

| Physical Interface | ID | Device Node |
|-------------------|-----|------------|
| GMSL0~3 | J2~J5 | /dev/video0 ~ /dev/video3 |

### 7.2 J12/J13 - Molex 22Pin CSI Output

**Function:** CSI output interface

**ID:** J12, J13

**Type/Model:** AFC01-S22FCC-00

#### 7.3.1 J12 Pin Definition

| Pin | Signal | Pin | Signal |
|-----|--------|-----|--------|
| 1 | GND | 13 | GND |
| 2 | MAX1_CSI0_DATA0_N | 14 | MAX1_CSI1_DATA1_N |
| 3 | MAX1_CSI0_DATA0_P | 15 | MAX1_CSI1_DATA1_P |
| 4 | GND | 16 | GND |
| 5 | MAX1_CSI0_DATA1_N | 17 | MAX1_ALL_SYNC_C |
| 6 | MAX1_CSI0_DATA1_P | 18 | NC |
| 7 | GND | 19 | GND |
| 8 | MAX1_CSI0_CKA_N | 20 | MAX1_SCL_CON |
| 9 | MAX1_CSI0_CKA_P | 21 | MAX1_SDA_CON |
| 10 | GND | 22 | VCC_3V3 |
| 11 | MAX1_CSI1_DATA0_N | 23 | GND |
| 12 | MAX1_CSI1_DATA0_P | 24 | GND |

#### 7.3.2 J13 Pin Definition

| Pin | Signal | Pin | Signal |
|-----|--------|-----|--------|
| 1 | GND | 13 | GND |
| 2 | MAX1_CSI2_DATA0_N | 14 | MAX1_CSI3_DATA1_N |
| 3 | MAX1_CSI2_DATA0_P | 15 | MAX1_CSI3_DATA1_P |
| 4 | GND | 16 | GND |
| 5 | MAX1_CSI2_DATA1_N | 17 | MAX1_ALL_SYNC_C |
| 6 | MAX1_CSI2_DATA1_P | 18 | NC |
| 7 | GND | 19 | GND |
| 8 | MAX1_CSI2_CKA_N | 20 | MAX1_SCL_CON |
| 9 | MAX1_CSI2_CKA_P | 21 | MAX1_SDA_CON |
| 10 | GND | 22 | VCC_3V3 |
| 11 | MAX1_CSI3_DATA0_N | 23 | GND |
| 12 | MAX1_CSI3_DATA0_P | 24 | GND |

---

## 8. Camera Testing

### 8.1 Camera Configuration

Run `sudo fzcam_ui` to configure CAM5~8, then select camera brands and models.

**Operation Steps:**

1. Select GMSL position for camera node
2. Select Manufacturer
3. Select Model
4. Save configuration
5. Run configuration

### 8.2 Camera Configuration Interface

![Camera Configuration GUI](../images/FG24_4CH/camera_config.png)

### 8.3 RGB Camera Quick Verification and Reference Code

The device supports video output using Gstreamer. Image capture and display methods are as follows:

#### 8.3.1 8MP Camera (3840×2160)

```bash
gst-launch-1.0 v4l2src device=/dev/video0 ! "video/x-raw, format=(string)UYVY, width=(int)3840, height=(int)2160" ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
```

#### 8.3.2 5MP Camera (2880×1860)

```bash
gst-launch-1.0 v4l2src device=/dev/video0 ! "video/x-raw, format=(string)UYVY, width=(int)2880, height=(int)1860" ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
```

#### 8.3.3 2MP Camera (1920×1080)

```bash
gst-launch-1.0 -ev v4l2src device=/dev/video0 ! "video/x-raw, format=(string)UYVY, width=(int)1920, height=(int)1080" ! fpsdisplaysink text-overlay=0 video-sink=fakesink sync=0
```

#### 8.3.4 1MP Camera (1280×720)

```bash
gst-launch-1.0 -ev v4l2src device=/dev/video0 ! "video/x-raw, format=(string)UYVY, width=(int)1280, height=(int)720" ! fpsdisplaysink text-overlay=0 video-sink=fakesink sync=0
```

**Note:** For cameras with other resolutions, configure according to specific camera parameters. Please contact our sales team or visit our website for the camera support list.

---

## 9. Warranty Terms

### 9.1 Important Notice

Zhuying Intelligent Robot (Shenzhen) Co., Ltd. warrants that all products provided are free from defects in materials and workmanship to the best of its knowledge, and fully conform to the specifications of the original factory shipment.

Zhuying Intelligent Robot (Shenzhen) Co., Ltd.'s warranty covers all original products. For failures of accessories configured by distributors, please contact the distributor for resolution. All products provided by the company are covered by a one-year warranty (lifetime repair service is provided beyond the warranty period). The warranty period starts from the date of manufacture. For products repaired within the warranty period, the repaired parts enjoy an extended 12-month warranty. Unless otherwise notified by Zhuying Intelligent Robot (Shenzhen) Co., Ltd., the date on your original delivery note is the date of manufacture.

### 9.2 How to Obtain Warranty Service

If your product fails to operate properly within the warranty period, please contact Zhuying Intelligent Robot (Shenzhen) Co., Ltd. for warranty service. When claiming warranty, please present your purchase invoice (this is your proof of entitlement to warranty service).

### 9.3 Warranty Resolution Procedures

When requesting warranty service, you need to follow the problem determination and resolution procedures specified by Zhuying Intelligent Robot (Shenzhen) Co., Ltd.. You need to accept the initial diagnosis by technical personnel via phone, WeChat, or email. You will be required to complete all questions on the repair form we provide to ensure we accurately determine the cause of the failure and the location of damage (for out-of-warranty products, we will also provide a fee estimate for your confirmation).

Zhuying Intelligent Robot (Shenzhen) Co., Ltd. reserves the right to "repair" or "replace" the claimed product. If the product is "replaced" or "repaired", the replaced "faulty" product or the "faulty" parts after repair will be returned to Zhuying Intelligent Robot (Shenzhen) Co., Ltd..

Since some repaired products need to be sent to the original factory, to avoid accidental losses, Zhuying Intelligent Robot (Shenzhen) Co., Ltd. recommends that you purchase transportation insurance. If you waive insurance, Zhuying Intelligent Robot (Shenzhen) Co., Ltd. shall not be liable for damage or loss of the shipped items during transportation.

For products within the warranty period:
- The user bears the shipping cost for returning the product to the factory
- Zhuying Intelligent Robot (Shenzhen) Co., Ltd. bears the shipping cost for returning the repaired product to the user

### 9.4 The Following Cases Are Not Covered by Warranty

1. Improper installation, misuse, or abuse (such as exceeding working load, etc.)
2. Faults or damage caused by improper maintenance (such as fire, explosion, etc.) or natural disasters (such as lightning, earthquake, typhoon, etc.)
3. Alterations to the product (such as circuit characteristics, mechanical characteristics, software characteristics, anti-corrosion treatment, etc.)
4. Other faults obviously caused by improper use (such as overvoltage, undervoltage, reverse polarity, bent or broken pins, wrong bus connection, component falling off, electrostatic breakdown, external force extrusion, drop damage, excessive temperature, excessive humidity, poor transportation, etc.)
5. The marks and part numbers on the product have been deleted or removed
6. The product is beyond the warranty period

### 9.5 Special Note

If multiple products experience the same fault or the same fault occurs repeatedly on the same equipment, to find the cause and confirm responsibility, our company reserves the right to request the user to provide physical or technical information of peripheral equipment, such as monitors, external devices, cables, power supplies, connection diagrams, system structure diagrams, etc. Otherwise, we reserve the right to refuse warranty service. Repairs will be charged at market prices, and a repair deposit will be required.

---

**Document Version:** V1.0

**Company Name:** Zhuying Intelligent Robot (Shenzhen) Co., Ltd.
