# FG92_2CH GMSL Camera Platform Product Manual

**Zhuying Intelligent Robot (Shenzhen) Co., Ltd.**

---

## 1. Product Introduction

FG92-2CH GMSL Camera Platform is an expansion board that allows up to 2 cameras to connect to Jetson AGX Orin/Xavier modules.

### Product Introduction

● FG92-2CH GMSL Board is a 2-channel GMSL capture board developed by Zhuying Intelligent / Fangzhu Technolo gy based
on the ADI Deserializer chip MAX96718A, hereinafter referred to as the Capture Board.
● This capture board supports connecting up to 2 GMSL cameras simultaneously and outputs CSI signals to the NVIDIA® Jetson AGX Orin™ NX development kit (also compatible with the Xavier NX development kit).
● The MAX96712 can adaptively decode both GMSL1 and GMSL2 cameras at different rates through software configuration.
●The 22Pin connector on the NVIDIA Jetson® AGX Orin™ / Xavier™ NX development kit cannot provide sufficient power for cameras. Therefore, the capture board uses an external pluggable 12V–24V DC power connector (can share the same power adapter as the kit).

facilitating installation and use in applications such as Autonomous Mobile Robots (AMR), Unmanned Aerial Vehicles (UAV) and automotive systems.

### 1.1 Product Images

![Product Image 1](../images/FG92_2CH/pic1.png)

![Product Image 2](../images/FG92_2CH/pic2.png)

![Product Image 3](../images/FG92_2CH/pic3.png)

### 1.2 Product Features

| Feature | Description |
|---------|-------------|
| Protocol Compatibility | GMSL2 and GMSL1 protocol compatible |
| Power Supply | Power over Coax |
| Voltage Output | Camera power supply supports wide voltage output |
| Protection | Built-in overvoltage and short-circuit protection |
| Power Control | Camera power can be controlled by software |
| Size | Compact size for AGX Orin/Xavier Devkit |

### 1.3 Product Specifications

| Item | Specification |
|------|--------------|
| Dimensions | 80mm × 53mm |
| Weight | 30g |
| Jetson Devkit Interface | Molex 22Pin ZIF connector (4Lane CSI) |
| Camera Inputs | 2-channel GMSL2/GMSL1 cameras |
| Deserializer | ADI MAX96718A |
| MIPI Output | 4-Lane CSI |
| Camera Connector | Single-end vertical FAKRA |
| PoC Power Supply | 2-channel PoC 12V |
| Power Input | 12V~24V @ 4A MAX |
| Operating Temperature | -20°C ~ +65°C |
| Warranty | One year warranty and technical support |



---

## 3. Safety Instructions

Before using the product, you must read this document carefully to gain a preliminary understanding of the product. Always comply with the safety instructions in this manual to ensure personal safety and avoid equipment damage. The manufacturer shall not be liable for any damage to equipment or personal life and property caused by incorrect operation.

### 3.1 Personal Safety

- Before operating the equipment, wear anti-static work clothes, anti-static gloves or wristbands, and remove jewelry, watches, and other conductive objects to avoid electric shock or burns.
- In case of fire, evacuate the building or equipment area and press the fire alarm bell, or call the fire department. Under no circumstances should you re-enter a burning building.

### 3.2 Power Supply Voltage

- The FG92_2CH carrier board supports input voltage range: 12V~24V, current: 2A or above

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

![Block Diagram](../images/FG92_2CH/block_diagram.png)

**Notes:**

1. I2C bus numbers correspond to hardware positions (matching connector J410, J411 pins). Bus numbers may not correspond to those listed in software.
2. Coaxial power is shared, but each GMSL line has its own PoC filter.

---

## 5. Component Placement Diagram

### 5.1 Placement Layout

![Component Placement](../images/FG92_2CH/component_placement.png)

---

## 6. External Interfaces

### 7.1 Interface Overview

| No. | Name | ID | Remarks |
|-----|------|-----|---------|
| 1 | Power Input | J101 | 12~24V DC |
| 2 | CSI0 | J411 | Molex 22Pin ZIF |
| 3 | CSI1 | J410 | Molex 22Pin ZIF |
| 4 | CAM0 | J708 | FAKRA connector |
| 5 | CAM1 | J709 | FAKRA connector |
| 6 | External Trigger | J102 | 1*3P |

---

## 7. Interface Functions

### 7.1 J101 - Power Input Interface

**Function:** POWER input

**ID:** J101

**Type/Model:** JPD1030-N521-4F

| Pin | Signal |
|-----|--------|
| 1 | 12V~24V |
| 2 | GND |
| 3 | GND |

### 7.2 J102 - External Trigger Connector

**Function:** External hardware trigger

**ID:** J102

**Type/Model:** Z-211-0311-0021-001

| Pin | Signal |
|-----|--------|
| 1 | MFP0 |
| 2 | MFP6 |
| 3 | GND |

### 7.2 J410/J411 - Molex 22Pin CSI Output

**Function:** CSI output interface

**ID:** J410, J411

**Type/Model:** AFC01-S22FCC-00

| Pin | Signal | Pin | Signal |
|-----|--------|-----|--------|
| 1 | GND | 13 | GND |
| 2 | DB0N | 14 | DB3N |
| 3 | DB0P | 15 | DB3P |
| 4 | GND | 16 | GND |
| 5 | DB1N | 17 | CAM_PWDN_1 |
| 6 | DB1P | 18 | NC |
| 7 | GND | 19 | GND |
| 8 | CKBN | 20 | CAM_I2C_SCL |
| 9 | CKBP | 21 | CAM_I2C_SDA |
| 10 | GND | 22 | VCC_3V3 |
| 11 | DB2N | 23 | GND |
| 12 | DB2P | 24 | GND |

### 7.2 J708/J709 - FAKRA Connectors

**Function:** GMSL camera interface

**ID:** J708, J709

**Type/Model:** Amphenol single-end vertical FAKRA

#### 7.3.1 FAKRA Pin Definition

| Pin | Signal |
|-----|--------|
| 1 | MAX1_SIOA_P |
| 2 | MAX1_SIOB_P |
| 3 | GND |
| 4 | GND |
| 5 | GND |

#### 7.3.2 Physical Interface to Device Node Mapping

| Physical Interface | ID | Device Node |
|-------------------|-----|------------|
| GMSL0 (CAM0) | J708 | /dev/video0 |
| GMSL1 (CAM1) | J709 | /dev/video1 |

---

## 8. Camera Testing

### 8.1 Camera Configuration

Run `sudo fzcam_ui` to configure CAM0~1, then select camera brands and models.

**Operation Steps:**

1. Select GMSL position for camera node
2. Select Manufacturer
3. Select Model
4. Save configuration
5. Run configuration

### 8.2 Camera Configuration Interface

![Camera Configuration GUI](../images/FG92_2CH/camera_config_en.png)

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
