# ElderGuard — ESP32-S3 + BNO086 Elderly Safety Wearable

## Communication Architecture

```
  ┌────────────────────┐
  │   ESP32-S3 Device  │
  │  (deep sleep cycle)│
  └───┬───────────┬────┘
      │ WiFi      │ 4G (A7670E)
      │           │
      ▼           ▼
  ┌──────────────────────────────────────┐
  │  Mosquitto Broker @ iot.gtpc.com.br  │
  │  Port 1883 (MQTT) + 9001 (WebSocket)│
  └──────┬─────────────────────┬─────────┘
         │                     │ wss:// via nginx
         │                     ▼
         │              ┌─────────────┐
         │              │  Dashboard  │
         │              │ (browser)   │
         │              └─────────────┘
         │
         ▼ (fallback)
  ┌─────────────┐
  │  SMS Alert  │
  └─────────────┘
```

### MQTT Topics

| Topic | Dir | QoS | Retained | Content |
|---|---|---|---|---|
| `elderguard/001/alert` | device → broker | 1 | yes | Panic / fall events with GPS |
| `elderguard/001/location` | device → broker | 0 | no | GPS coordinates |
| `elderguard/001/telemetry` | device → broker | 0 | no | Battery, signal, boot count |
| `elderguard/001/status` | LWT | 1 | yes | `{"online":true/false}` |
| `elderguard/001/command` | dashboard → device | 1 | no | `locate`, `ping`, `interval:N` |

### Transport Priority

1. **MQTT over WiFi** — when a known AP is in range (fastest, ~2s connect)
2. **MQTT over 4G** — via A7670E AT+CMQTT commands (works anywhere)
3. **SMS** — always sent for panic/fall alerts as a guaranteed fallback

## Server Setup (iot.gtpc.com.br)

### Quick Deploy

```bash
# On your VPS:
cd server/
chmod +x setup_server.sh
sudo ./setup_server.sh
```

This will configure Mosquitto with authentication, deploy the dashboard to nginx, set up the WebSocket proxy, and open firewall ports.

### Manual Steps

**1. Mosquitto passwords** — the setup script prompts you, or do it manually:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd elderguard
sudo mosquitto_passwd    /etc/mosquitto/passwd dashboard
```

**2. Match passwords everywhere:**
- `config.h` → `MQTT_PASSWORD` (device user)
- `server/dashboard/index.html` → `password` in `mqtt.connect()` (dashboard user)

**3. TLS (recommended for production):**

```bash
sudo certbot --nginx -d iot.gtpc.com.br
```

For MQTT-over-TLS on port 8883, add to `elderguard.conf`:

```
listener 8883
certfile /etc/letsencrypt/live/iot.gtpc.com.br/cert.pem
cafile   /etc/letsencrypt/live/iot.gtpc.com.br/chain.pem
keyfile  /etc/letsencrypt/live/iot.gtpc.com.br/privkey.pem
```

Then set `MQTT_PORT 8883` and `MQTT_USE_TLS true` in `config.h`.

**4. Test the broker:**

```bash
# Terminal 1: subscribe to all device topics
mosquitto_sub -h localhost -u dashboard -P <pass> -t 'elderguard/#' -v

# Terminal 2: simulate a device heartbeat
mosquitto_pub -h localhost -u elderguard -P <pass> \
  -t 'elderguard/001/telemetry' \
  -m '{"batt_pct":85,"batt_v":3.92,"signal":18,"boot":1,"transport":"wifi"}'
```

## Hardware Wiring

### BNO086 IMU → ESP32-S3-A7670E

```
   BNO086 Breakout                 ESP32-S3-A7670E
   ┌─────────────┐                ┌─────────────────┐
   │         VCC  │───────────────│ 3V3              │
   │         GND  │───────────────│ GND              │
   │         SDA  │──────┬────────│ GPIO 2           │
   │         SCL  │──┬───│────────│ GPIO 1           │
   │         INT  │──│───│────────│ GPIO 3 (RTC)     │  ← deep-sleep wake
   │         RST  │──│───│────────│ GPIO 4           │
   │         PS0  │──│───│──┐     │                  │
   │         PS1  │──│───│──┤     │                  │
   └─────────────┘  │   │  │     └─────────────────┘
                  4.7kΩ 4.7kΩ GND
                     │   │
                    3V3 3V3
```

### Panic Button → GPIO 0 (internal pull-up, press = LOW → ext1 wake)

### Battery ADC → GPIO 6 via 100kΩ/100kΩ voltage divider

## Firmware Setup

### Arduino IDE / PlatformIO

**Board**: ESP32S3 Dev Module

**Libraries** (install via Library Manager):
- `SparkFun BNO08x Cortex Based IMU` by SparkFun
- `PubSubClient` by Nick O'Leary (for MQTT over WiFi)
- `ESP32 BLE Arduino` (included with ESP32 core)

### Configuration

Edit `config.h`:
- `ALERT_PHONE_NUMBER` — caregiver's phone for SMS fallback
- `MQTT_BROKER` — already set to `iot.gtpc.com.br`
- `MQTT_USER` / `MQTT_PASSWORD` — must match Mosquitto credentials
- `WIFI_SSID` / `WIFI_PASSWORD` — home WiFi for preferred transport
- Fall detection thresholds — tune after testing

## Dashboard

The web dashboard at `https://iot.gtpc.com.br` shows:
- **Live map** with the device's last known position (Leaflet + CARTO dark tiles)
- **Alert banner** with audio notification on panic/fall events
- **Telemetry cards** — battery, signal strength, transport type, boot count
- **Command buttons** — "Locate now" and "Ping" send messages to the device
- **Event log** — scrollable feed of all MQTT messages

The dashboard connects to Mosquitto via WebSocket through the nginx `/mqtt` proxy endpoint. No backend server needed — it's a single HTML file subscribing directly to MQTT topics.

## Power Budget (3000 mAh)

| State | Current | Duration | Notes |
|---|---|---|---|
| Deep sleep | ~10 µA | ~295 s | ESP32-S3 + BNO086 idle |
| WiFi MQTT publish | ~160 mA | ~3 s | Faster than 4G |
| 4G MQTT publish | ~250 mA | ~8 s | Fallback transport |
| GPS fix (A7670E) | ~120 mA | ~30 s | Cold start worst case |
| SMS (fallback) | ~300 mA | ~5 s | Only for panic/fall |

**Estimated battery life: 3–5 days** with 5-minute heartbeats over WiFi MQTT.

## Project Structure

```
elderly_wearable/
├── elderly_wearable.ino    # Main sketch — wake/assess/act/sleep
├── config.h                # All settings: pins, MQTT, thresholds
├── imu.h                   # BNO086 fall detection + wake-on-motion
├── cellular.h              # A7670E AT commands: GPS, SMS, HTTP
├── mqtt_manager.h          # MQTT: WiFi (PubSubClient) + 4G (AT+CMQTT)
├── ble_manager.h           # BLE GATT server for companion app
├── power.h                 # Deep sleep, wake sources, battery ADC
├── platformio.ini          # PlatformIO build config
└── server/
    ├── setup_server.sh     # One-command VPS deployment
    ├── mosquitto/
    │   ├── elderguard.conf # Broker config
    │   └── acl             # Topic access control
    ├── nginx/
    │   └── elderguard.conf # HTTPS + WebSocket proxy
    └── dashboard/
        └── index.html      # Real-time MQTT dashboard
```
