# LibbyCam firmware

Libby is what I am calling my bird-scarer robot.

This repo is the firmware for the camera component used to track robot on lawn.

WebCam codes based on expressif sample **.ino* code (installed in Arduino IDE following this instructions).

LibbyCam 'hardware' consists two ESP32Cam units. mounted on servos. Feeds from these two cameras are  stitiched together to created panarama.


## Additional stuff added

1. Converted to PlatformIO project (I am using PlatformIO plugin for VS Code
2. Added zeroconf/ mDNS via ESPnDNS library.
3. Control of upto 9G 2 servos via ESP32CAM pins.
