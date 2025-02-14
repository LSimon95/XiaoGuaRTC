<p align="center" style="font-size: 2.5em">
    <strong>🍉 XiaoGua RTC Firmware 🤖<strong>
</p>

<h3 align="center">  
Real time, low cost and esay chat with AI robot.
</h3>

---

XiaoGua RTC is a firmware for ESP32 chip, which is designed by [ZideAI](https://www.zideai.com) for the XiaoGua RTC project. This project use RTC protocol and websocket to communicate with the AI server p2p. User can use own wake word, set up a connection in short time, and chat with the AI with audio in real time. Also, AI speaking can be interrupted by the user speaking without any extra components. Furthermore, voice clone is supported by [ZideAI](https://www.zideai.com) to make the AI voice more natural.

[![Licence](https://img.shields.io/badge/License-MIT-blue)](https://github.com/LSimon95/XiaoGuaRTC/blob/main/LICENSE)
![Static Badge](https://img.shields.io/badge/Chip-ESP32S3-green)

｜ ENGLISH ｜[简体中文](README.md) ｜

---

## Quick Start

### 1. Prepare hardware
Compatible with [XiaoZhi](https://github.com/78/xiaozhi-esp32) hardware, but only the microphone(INMP441) and speaker(MAX98357A) are needed in this version. Only the ESP32S3-N16R8 chip is supported now.
### 2. Download and flash firmware
Download the latest firmware from the [release page](https://github.com/LSimon95/XiaoGuaRTC/releases). If you want to compile the firmware yourself, please refer to the [compile guide](#compile-guide). Use the [esptool](https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/flash_download_tool.html) to flash the firmware to the ESP32S3 chip. 
### 3. Connect to internet and register
After the firmware is flashed, the device will create a hotspot named `XiaoGuaRTC-XXXX`. Connect to the hotspot and open the browser to visit `http://192.168.4.1`. Follow the instructions to connect to the internet.
Register an account on the [ZideAI](https://www.zideai.com) website and create a new device. Input the 6-digit device code(Audio code) to the website and bind the device.
### 4. Enjoy
After the device is connected to the internet, the device will automatically connect to the AI server. Say the default wake word `你好小瓜` to wake up the AI and start chatting.
### *5. Customize wake word and voice clone
You can customize the wake word and voice clone on the [ZideAI](https://www.zideai.com) website. The wake word can be set to any Chinese word, and the voice clone can be set to any voice style.

## Features
- Wi-Fi connection
- Real-time audio communication
- Custom LLM API
- Custom wake word
- Voice clone

## Compile Guide
1. Clone the repository.
2. Clone [libpeer](https://github.com/sepfy/libpeer) and put it in the `managed_components` folder.
3. Open the project with the ESP-IDF framework.
4. Compile the project and flash the firmware.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.