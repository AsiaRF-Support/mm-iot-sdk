# ThingsBoard STM32 MQTT Demo - Quick Setup Guide

This demo connects an AsiaRF Wi-Fi HaLow IoT board AWH575-001 to ThingsBoard via MQTT to control 3 relays and 1 LED dimmer through dashboard widgets.

## Features
- **Single Device**: One AWH575-001 controlling multiple I/O components
- **Real-time Control**: 3 Switch widgets + 1 Knob widget
- **QoS 1**: Reliable MQTT communication with CoreMQTT

---

## Setup

### Step 1: Create ThingsBoard Device
```
• Login to ThingsBoard (demo.thingsboard.io)
• Entities → Devices → Add Device
• Name: AWH575-001 → Add
• Device credentials → Copy Access Token
```

### Step 2: Create Dashboard & Widgets

**Create Dashboard:**
```
• Dashboards → Create new dashboard
• Title: AWH575-001 Control Panel → Add
```

**Add 3 Switch Controls (Relay 1, 2, 3):**
```
• Widget Bundle: Control widgets → Switch control
• Data source: Device AWH575-001

Value settings:
  • Retrieve on/off value setting
  • Retrieve using: Subscribe for time series
  • Time series key: r1_value (r2_value, r3_value for others)
  • Parse function: return data ? true : false;

Update value settings:
  • RPC method: r1_setValue (r2_setValue, r3_setValue for others)
  • Convert function: return value;
  • RPC timeout: 500ms
```

**Add 1 Knob Control (LED Dimmer):**
```
• Widget Bundle: Control widgets → Knob control
• Data source: Device AWH575-001

Behavior → Initial value:
  • Action: Get time series
  • Time series key: led_value
  • Converter: Function
    if (data >= 0 && data <= 10) {
        return data;
    } else {
        return NaN;
    }

Behavior → On value change:
  • Action: Execute RPC
  • Method: led_setValue
  • Parameters: Value

Appearance:
  • Knob title: LED Level
  • Range: min=0, max=10
  • Value: Set 0(decimals)
  • Fallback initial value: 0
```

### Step 3: Configure Firmware

**Edit `thingsboard_config.h`:**
```c
// Server settings
#define TB_SERVER_ENDPOINT      "demo.thingsboard.io"  // or your server
#define TB_DEVICE_TOKEN_MAIN    "YOUR_ACCESS_TOKEN_HERE"  // from Step 1

// QoS settings
#define MQTT_QOS_LEVEL          1  // Reliable delivery
```

### Step 4: Build & Test
```
• Build and flash firmware to AWH575-001
• Monitor serial output for connection status
• Test switches and knob in ThingsBoard dashboard
• Verify telemetry updates in device Latest telemetry tab
```

---

## Widget Summary

| Widget | Type | Time Series Key | RPC Method | Range |
|--------|------|----------------|------------|-------|
| Relay 1 | Switch | `r1_value` | `r1_setValue` | true/false |
| Relay 2 | Switch | `r2_value` | `r2_setValue` | true/false |
| Relay 3 | Switch | `r3_value` | `r3_setValue` | true/false |
| LED | Knob | `led_value` | `led_setValue` | 0-10 |

## MQTT Communication

**Telemetry (Device → ThingsBoard):**
```json
Topic: v1/devices/me/telemetry
Payload: {"r1_value":1,"r2_value":0,"r3_value":1,"led_value":7}
```

**RPC Commands (ThingsBoard → Device):**
```json
Topic: v1/devices/me/rpc/request/<id>
Payload: {"method":"r1_setValue","params":true}
Payload: {"method":"led_setValue","params":5}
```

**RPC Response (Device → ThingsBoard):**
```json
Topic: v1/devices/me/rpc/response/<id>
Payload: {"result":1}
```

---

## Troubleshooting

**Connection Issues:**
- Check Wi-Fi HaLow and server address
- Verify access token is correct
- Monitor serial output for error codes

**Widget Issues:**
- Ensure time series keys match firmware telemetry keys
- Check RPC method names match firmware expectations
- Verify converter functions handle data correctly

**Hardware Issues:**
- Check GPIO/Timer configuration for relays and LED
- Verify physical wiring and power supply
- Test components manually in firmware

---

## License
Copyright 2025 AsiaRF - Apache-2.0
