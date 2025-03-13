
<h1 align="center">
    🍉 小瓜 RTC 固件 🤖
</h1>

<h4 align="center">  
    实时、低成本且易于与 AI 机器人聊天。
</h4>

---

小瓜 RTC 是为 ESP32 芯片设计的固件，由 [ZideAI](https://www.zideai.com) 为小瓜 RTC 项目设计。该项目使用 RTC 协议和 WebSocket 与 AI 服务器进行点对点通信。用户可以使用自己的唤醒词，在短时间内建立连接，并通过音频与 AI 实时聊天。此外，用户在说话时可以中断 AI 的语音，无需任何额外组件。同时，支持通过 [ZideAI](https://www.zideai.com) 进行语音克隆，使 AI 语音更加自然。

[![Licence](https://img.shields.io/badge/License-MIT-blue)](https://github.com/LSimon95/XiaoGuaRTC/blob/main/LICENSE)
![Static Badge](https://img.shields.io/badge/Chip-ESP32S3-green)

｜ 简体中文 ｜[ENGLISH](README-EN.md) ｜

📺[方案简介](https://www.bilibili.com/video/BV1QEPceWEBJ/?vd_source=67eca34096f270ba35b7b86448bcdaf7)
📺[两年时间打造的AI女友有点话唠](https://www.bilibili.com/video/BV1P6KKeaEwo/)

QQ 交流群：1034377943

---

## 快速启动

### 1. 准备硬件
兼容 [小智](https://github.com/78/xiaozhi-esp32) 硬件，但此版本仅需要麦克风(INMP441)和扬声器(MAX98357A)。目前仅支持 ESP32S3-N16R8 芯片。
### 2. 下载并烧录固件
从 [发布页面](https://github.com/LSimon95/XiaoGuaRTC/releases)下载最新固件。如果你想自己编译固件，请参考[编译指南](#编译指南)。使用[esptool](https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/flash_download_tool.html)将固件烧录到 ESP32S3 芯片。
### 3. 连接互联网并注册硬件
烧录固件后，设备将创建一个名为 `XiaoGuaRTC-XXXX`的热点。连接到该热点并打开浏览器访问 `http://192.168.4.1`。按照说明连接到互联网。在
[ZideAI](https://www.zideai.com) 网站上注册一个帐户并创建一个新设备。将 6 位设备代码（音频代码）输入到网站并绑定设备。
### 4. 唤醒 AI并对话
设备连接到互联网后，设备将自动连接到 AI 服务器。说出默认唤醒词 `你好小瓜`以唤醒 AI 并开始聊天。
### *5. 自定义唤醒词和语音克隆
你可以在[ZideAI](https://www.zideai.com)网站上自定义唤醒词和语音克隆。唤醒词可以设置为任意中文词，语音克隆可以设置为任意语音风格。
## 功能
- Wi-Fi 连接
- 实时音频通信
- 自定义 LLM API
- 自定义唤醒词
- 语音克隆

## 编译指南
0. 编译环境准备，IDF V5.5，需要从[乐鑫官方库](https://github.com/espressif/esp-idf.git)中克隆。
1. 克隆代码仓库。
2. 克隆 [libpeer](https://github.com/LSimon95/libpeer) ，注意选择aiortc_turn_opus分支，并将其放在 `XiaoGuaRTC/managed_components` 文件夹中。
3. 使用 ESP-IDF 框架打开项目。
4. 编译项目并烧录固件。

## License
该项目采用 MIT 许可证 - 详情请参见 [LICENSE](LICENSE)文件。
