# ESP32-P4 企业级 AI 桌宠展示版

这是 ESP32-P4 Function EV Board 的触摸屏 AI 桌宠项目。展示主体仍然是“桌面智能陪伴宠物”，但架构按企业级项目处理：电脑端本地代理保护 API Key，ESP32-P4 只访问局域网接口，DeepSeek 负责生成更自然的陪伴建议。

运行链路：

```text
触摸屏 -> ESP32-P4 -> WiFi -> 电脑本地代理 -> 天气/日程/DeepSeek -> LCD 显示
```

## 当前功能

- `天气助手`：显示西安天气、温度、降雨概率、出行提醒，并作为桌宠环境感知依据。
- `日程提醒`：显示月份、日期、时间、农历、节假日，并作为桌宠陪伴节奏依据。
- `AI陪伴`：请求电脑端 `/insight`，未配置 DeepSeek 时使用本地兜底建议，配置 API Key 后由 DeepSeek 生成陪伴建议。

屏幕 UI 使用 LVGL，英文数字使用 Montserrat，中文使用项目内自定义字库：

```text
main/workbuddy_cn_20.c
main/workbuddy_cn_28.c
```

如果后面新增固定中文显示内容，需要先把新增汉字加入：

```text
tools/generate_workbuddy_fonts.js
```

然后重新生成字库并构建。

## DeepSeek 接入原则

DeepSeek API Key 只放在电脑端环境变量里，不写入 ESP32 固件，也不提交到仓库。ESP32-P4 只访问同一 WiFi 下的本地代理，例如：

```text
http://电脑IP:8787/insight
```

代理会调用 DeepSeek `/chat/completions` 并要求模型返回结构化 JSON，再整理为屏幕端稳定显示的桌宠陪伴字段：

```json
{
  "insight": "FOCUS",
  "risk": "LOW",
  "basis": "WEATHER_STABLE WORK_HOUR"
}
```

ESP32-P4 端显示为桌宠话术，例如：

```text
适合专注陪伴
天气变化要提醒
桌宠状态稳定
```

推荐双击下面这个脚本配置 DeepSeek API Key，脚本会生成本地 `deepseek_config.ps1`，这个文件已被 `.gitignore` 忽略，不会上传：

```text
set_deepseek_key.bat
```

也可以临时用环境变量启动：

```powershell
$env:DEEPSEEK_API_KEY="你的 DeepSeek Key"
$env:DEEPSEEK_MODEL="deepseek-v4-flash"
.\start_demo.bat
```

如需更强模型，可以把 `DEEPSEEK_MODEL` 改为 `deepseek-v4-pro`。

## 启动电脑代理

电脑和开发板需要连接同一个 WiFi。展示前双击：

```text
start_demo.bat
```

这个脚本会启动本地代理，并检查 `/health`、`/weather`、`/time`。

也可以只检查代理：

```text
check_proxy.bat
```

代理默认监听：

```text
http://0.0.0.0:8787
```

可用接口：

```text
/health
/weather
/time
/insight
```

## 新电脑从零拉取

新电脑只需要安装好 Git、Node.js 18+ 和 ESP-IDF 5.x，然后执行：

```powershell
git clone https://gitee.com/h616444/revised-edition.git
cd revised-edition
```

拉取后先双击：

```text
new_pc_check.bat
```

这个脚本会检查 Git、Node.js、ESP-IDF 命令行和项目关键文件是否齐全。ESP-IDF 项目的 `sdkconfig`、`build/`、`managed_components/` 不提交到仓库，新电脑构建时会自动根据 `sdkconfig.defaults` 和 `main/idf_component.yml` 生成/下载。

启动本地代理时双击：

```text
start_demo.bat
```

如果失败，窗口会提示常见原因，同时把详细日志写到：

```text
proxy.err.log
proxy.out.log
```

构建固件可以在 ESP-IDF 终端里双击或运行：

```text
build_firmware.bat
```

也可以手动执行：

```powershell
idf.py set-target esp32p4
idf.py build
idf.py -p COM3 flash
```

如果想让 Windows 登录后自动启动代理，可以运行：

```text
install_proxy_startup.bat
```

## 接线提醒

保持 MIPI 屏幕排线连接。显示屏供电/控制排针：

- 显示屏 `5V` -> 主板 `5V`
- 显示屏 `GND` -> 主板 `GND`
- 显示屏 `RST_LCD` -> 主板 `GPIO5`
- 显示屏从主板供电时，不要再接显示屏独立的 `5V INPUT` Type-C

GPIO 使用：

- `GPIO5`：LCD reset
- `GPIO7`：GT911 touch I2C SDA
- `GPIO8`：GT911 touch I2C SCL
- `GPIO22`：LCD backlight

## 构建和烧录

推荐使用 VSCode ESP-IDF 插件构建、烧录。命令行也可以：

```powershell
idf.py build
idf.py -p COM3 flash
```

如果 Windows 分配了其他串口，把 `COM3` 改成实际端口。

## 配置

默认配置在 `sdkconfig.defaults`：

```text
CONFIG_WORKBUDDY_WIFI_SSID="abc"
CONFIG_WORKBUDDY_WIFI_PASSWORD="abc123456"
CONFIG_WORKBUDDY_PROXY_BASE_URL="auto"
CONFIG_WORKBUDDY_PROXY_PREFERRED_HOST="172.31.169.142"
CONFIG_WORKBUDDY_PROXY_PORT=8787
```

如需修改，运行：

```powershell
idf.py menuconfig
```

进入 `WorkBuddy Configuration` 修改。

## 项目文件说明

- `main/workbuddy_main.c`：主入口，初始化 WiFi、LCD、触摸、业务逻辑。
- `main/workbuddy_display_test.c`：屏幕 UI 和触摸交互。
- `main/workbuddy_actions.c`：天气/日程/AI 洞察请求配置。
- `main/workbuddy_launcher.c`：桌面图标入口。
- `tools/workbuddy_proxy.js`：电脑端本地代理，提供天气、日程和 DeepSeek 桌宠陪伴建议。
- `tools/generate_workbuddy_fonts.js`：生成中文 LVGL 字库。
- `start_demo.bat`：一键启动并检查代理。

## 后续优化方向

1. 接入更多桌宠状态字段，例如用户专注度、休息状态、设备状态。
2. 给 `/insight` 增加更多上下文字段，让 DeepSeek 生成更自然的陪伴建议。
3. 增加后台配置页，方便现场切换 DeepSeek Key、模型和展示城市。
