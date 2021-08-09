# Tilt-pan ESP32-Cam firmware

Base code source for video stream server:
https://RandomNerdTutorials.com/esp32-cam-projects-ebook/

Also expressif demo code examples for ESP32-CAM 

## Stuff added

1. Converted to PlatformIO project (I am using PlatformIO plugin for VS Code
2. Added zeroconf/ mDNS via ESPmDNS library.
3. Control of 9G servos off ESP32CAM pins, using ESP32Servo library.
4. (web api) EEPROM on tilt and pan servo values
5. (web-api) User-control of amount of rotation as well as direction
6. (web api) Change framesize and jpeg-quality
7. Changed up web UI so you get tilt and pan updates.

## Config

`<name>`, the mDNS device name, is set in `PanTiliWebCam.cpp`
Eneter wifi details in `demo_network_credentials.h` and rename to `network_credentials.h`

## Api


### Web ui

```http://<name>.local```

### Video stream 

```http://<name>.local:81/stream```

### Commands
#### Move camera
Move default increment (5 degrees)

```http://<name>.local/action?go=<left|right|up|down>```

Move `<x>` degrees

```http://<name>.local/action?go=<left|right|up|down>&degrees=<x>```

200 response is a JSON with current tilt and pan angles

#### Change framesize/ resolution

`<framesize>` is an integer corresponding to `framesize_t` type in `esp32_camera` library `sensor.h`

```http://<name>.local/action?framesize=<framesize>```


#### Change quality

`<quality>` is an integer (0-63)

```http://<name>.local/action?quality=<quality>```

### EEPROM

Reset camera to saved settings

```http://<name>.local/action?eeprom=0```

Save current cmaera settings to EEPROM

```http://<name>.local/action?eeprom=1```
