# Folo AI Passport XiaoZhi

面向 **Folo AI Passport ESP32-C3 工牌** 的小智语音助手固件。本仓库只保留这一款硬件的板级实现，不能直接用于其他 ESP32 开发板。

本项目基于开源项目 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 的 2.4.2 版本适配，保留原项目 MIT 许可证。具体来源与修改范围见 [NOTICE](NOTICE)。

## 已支持功能

- ESP32-C3，8 MB Flash，无 PSRAM
- 240 × 320 ST7789P3 SPI 屏幕及 PWM 背光
- ES8311 编解码器、麦克风输入和扬声器输出
- GPIO0 ADC 电阻梯三键
- Wi-Fi 热点配网与小智服务连接
- 简体中文资源和“你好小智”本地唤醒词
- WebSocket、MQTT/UDP 与设备端 MCP 能力

## 硬件连接

| 功能 | GPIO / 参数 |
| --- | --- |
| LCD MOSI / SCLK / CS / DC | GPIO9 / GPIO8 / GPIO1 / GPIO20 |
| LCD RST / 背光 | 未连接 / GPIO21 |
| ES8311 I²C SDA / SCL | GPIO10 / GPIO7 |
| I²S MCLK / BCLK / WS | GPIO6 / GPIO5 / GPIO3 |
| I²S DOUT / DIN | GPIO2 / GPIO4 |
| 三键 ADC | GPIO0 / ADC1_CH0 |
| USB Serial/JTAG | GPIO18 / GPIO19 |

按键默认行为：上键音量 `+10`，下键音量 `-10`，确认键开始或停止对话；启动阶段按确认键可进入配网。

## 构建

推荐使用 ESP-IDF 6.0.2；本板也已在 ESP-IDF 5.5.3 下完成构建和实机烧录验证。

```sh
python scripts/build.py folo/ai-passport-c3 \
  --name folo-ai-passport-c3 \
  --language zh-CN \
  --wake-word nihaoxiaozhi
```

构建完成后，完整 8 MB 合并镜像位于 `build/merged-binary.bin`。

## 烧录

把 `PORT` 替换为实际串口：

```sh
python -m esptool --chip esp32c3 --port PORT --baud 460800 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 build/merged-binary.bin
```

烧录会覆盖 Flash 中原有固件及运行数据。首次启动后连接屏幕显示的小智热点；若配网页面没有自动弹出，访问 `http://192.168.4.1`。

## 目录

- `main/boards/folo/ai-passport-c3/`：工牌引脚、屏幕、音频和按键初始化
- `main/boards/common/`：小智公共板卡抽象和 Wi-Fi 支持
- `main/audio/`：录音、播放、编解码和唤醒词
- `main/display/`：LCD/LVGL 界面
- `main/protocols/`：WebSocket 与 MQTT/UDP 协议
- `scripts/build.py`：唯一推荐的构建入口

## 当前限制

- CW2017 电量计尚未接入小智的电量接口。
- 已验证编译、Flash 内容以及基础启动/配网；麦克风、扬声器、按键和长期稳定性仍建议在不同批次硬件上继续测试。

## 许可证

本项目采用 [MIT License](LICENSE)。发布衍生版本时请保留 `LICENSE` 和 `NOTICE`。

