# 乐鑫 LeXin ESP32-P4 边缘 AI 桌宠

乐鑫是一套运行在 ESP32-P4 Function EV Board 上的触摸屏边缘 AI 桌宠项目。它把天气、日历、科研/生活建议、触摸交互、专注计时和摄像头情绪识别合并到同一个固件中，重点展示“边缘 AI 本地推理”，而不是只依赖云端问答。

项目核心链路：

```text
触摸/天气/日历/专注计时 -> ESP-DL 本地量化建议模型 -> LCD 当前建议
SC2336 摄像头 -> ESP-WHO 本地人脸检测/跟踪 -> LCD 情绪研伴
电脑代理 -> 天气/日期/DeepSeek 可选增强 -> LCD 云端建议
```

DeepSeek 是联网增强项；比赛展示时应重点说明 ESP32-P4 上的 ESP-DL 建议模型和 ESP-WHO 人脸识别都在本地运行。

## 已验证环境

本项目当前验证通过的开发环境如下，建议新电脑尽量保持一致：

| 项目 | 版本 |
| --- | --- |
| ESP-IDF | v5.5.4 |
| ESP-IDF VS Code 扩展 | 使用 Espressif IDF 扩展并选择 ESP-IDF v5.5.4 |
| 芯片目标 | esp32p4 |
| CMake | 3.30.2 |
| Ninja | 1.12.1 |
| RISC-V 工具链 | riscv32-esp-elf gcc 14.2.0 |
| Python 环境 | ESP-IDF 自带 Python 3.11 环境 |
| Node.js | 18 或更高版本 |
| 操作系统 | Windows 10/11 |

ESP-IDF 组件管理器会自动拉取依赖。当前构建中使用到的主要组件包括：

- `espressif/esp-dl 3.3.6`
- `espressif/esp32_p4_function_ev_board 5.2.3`
- `espressif/esp_cam_sensor 2.0.1`
- `espressif/esp_hosted 2.12.9`
- `espressif/esp_wifi_remote 1.6.1`
- `espressif/esp_lcd_ek79007 2.0.2~1`
- `espressif/esp_lcd_touch_gt911 1.2.0~2`
- `espressif/esp_lvgl_port 2.8.0~1`
- `lvgl/lvgl 9.5.0`

## 硬件

- ESP32-P4 Function EV Board
- 配套 1024 x 600 触摸屏
- SC2336 MIPI-CSI 摄像头
- ESP32-C6 无线子板
- 两根数据线：一根用于主板供电/调试，一根用于无线子板连接

## 功能入口

主界面包含四个功能入口：

1. 天气提醒：显示温度、天气、降雨概率、体感、湿度、风力、气压和生活建议。
2. 日程提醒：显示北京时间、月历、农历、日期类型和今日安排。
3. 情绪研伴：调用本地摄像头画面、人脸框、置信度、推理延迟和研伴回应。
4. 研伴建议：左侧显示 ESP-DL 本地模型建议，右侧显示 DeepSeek 云端建议，两者使用同一上下文但独立输出。

## 新电脑快速启动

### 1. 安装工具

在新电脑上先安装：

1. Git
2. VS Code
3. VS Code 的 Espressif IDF 扩展
4. ESP-IDF v5.5.4
5. Node.js 18 或更高版本

安装 ESP-IDF 时建议通过 VS Code 扩展安装或选择 `D:\Espressif\frameworks\esp-idf-v5.5.4` 这一类标准路径。只要 VS Code 右下角显示 ESP-IDF v5.5.4，即可继续。

### 2. 下载项目

推荐用 Git 克隆，并把本地文件夹命名为 `LeXin`：

```powershell
git clone https://github.com/Caspian66671/-.git LeXin
cd LeXin
```

如果从 GitHub 网页下载 ZIP，解压后的文件夹名可能由仓库名决定。无论文件夹叫什么，打开时都必须打开包含根目录 `CMakeLists.txt` 的项目根目录，不能只打开 `main` 子目录，否则会出现 `CMakeLists.txt not found in project directory`。

### 3. 检查环境

双击：

```text
new_pc_check.bat
```

它会检查 Git、Node.js、ESP-IDF 和项目关键文件。若提示 ESP-IDF 环境不可用，请在 VS Code 中执行：

```text
ESP-IDF: Open ESP-IDF Terminal
```

再进行构建。

### 4. 网络配置

演示 Wi-Fi 已写在 `sdkconfig.defaults`：

```text
CONFIG_LEXIN_WIFI_SSID="abc"
CONFIG_LEXIN_WIFI_PASSWORD="abc123456"
```

新电脑和开发板需要连接同一个 Wi-Fi。若现场网络不同，只改 `sdkconfig.defaults` 里的 SSID 和密码，然后重新构建烧录。

### 5. 启动电脑代理

天气、日历和 DeepSeek 需要电脑端代理。烧录前或烧录后均可双击：

```text
start_demo.bat
```

窗口显示以下内容即可：

```text
Proxy OK.
Weather:
Time:
AI pet insight:
```

首次运行如果 Windows 防火墙弹窗，请允许访问当前网络。代理默认端口为 `8787`，开发板会自动使用当前电脑局域网 IP。

### 6. 构建固件

在 ESP-IDF 终端中运行，或双击：

```text
build_firmware.bat
```

等价命令是：

```powershell
idf.py set-target esp32p4
idf.py build
```

本项目的根 `CMakeLists.txt` 已固定目标为 `esp32p4`，正常情况下新电脑不需要手动改目标。

### 7. 烧录

连接开发板后双击：

```text
flash_firmware.bat
```

如果电脑上有多个串口，也可以手动指定：

```powershell
idf.py -p COM3 flash monitor
```

烧录完成后按一次开发板复位键。主界面应在几秒内显示，天气、日历、研伴建议和情绪研伴都可以从主界面快速进入。

## DeepSeek 配置

DeepSeek 是可选增强，不影响本地模型和情绪识别。首次配置双击：

```text
set_deepseek_key.bat
```

输入 API Key 后会生成本机忽略文件 `deepseek_config.ps1`，该文件不会提交到仓库。随后重新双击 `start_demo.bat`。窗口出现 `DeepSeek enabled` 表示云端建议可用。

## 比赛演示流程

建议按以下顺序展示：

1. 开机展示主界面，说明四个功能入口都在同一个 ESP32-P4 固件里。
2. 打开情绪研伴，正对摄像头并缓慢移动，展示人脸框、`FACE: YES`、置信度、延迟和情绪回应。
3. 返回主界面，打开天气提醒，展示真实天气数据和生活建议。
4. 打开日程提醒，展示月历、农历、日期类型和今日安排。
5. 打开研伴建议，说明左侧是 ESP-DL 本地量化模型，右侧是 DeepSeek 云端建议。
6. 断开电脑代理或不配置 DeepSeek，再进入情绪研伴和本地建议，证明边缘 AI 能独立运行。

## 目录说明

```text
main/lexin_main.c                 Wi-Fi、代理、任务队列和应用入口
main/lexin_display_test.c         LVGL 主屏和四个功能页面
main/lexin_edge_advisor.cpp       ESP-DL 本地建议模型推理
main/lexin_interaction.c          触摸交互和专注计时数据
main/models/lexin_advisor.espdl   本地 INT8 量化建议模型
components/lexin_vision/          摄像头、ESP-WHO、人脸跟踪和情绪状态
components/human_face_detect/     与当前 ESP-DL 运行时匹配的人脸模型组件
tools/lexin_proxy.js              天气、日期和 DeepSeek 电脑代理
tools/start_lexin_proxy.ps1       自动发现网络并启动代理
```

`build/`、`managed_components/`、`sdkconfig`、日志文件和本机 API Key 都是生成文件或本地文件，不需要提交。新电脑构建时会自动重新生成。

## 重新生成字体

如果修改了 UI 中的中文文案，运行：

```powershell
node tools\generate_lexin_fonts.js
idf.py build
```

字体脚本会从 `main/lexin_display_test.c` 提取中文字符并生成 LVGL 字体。若屏幕出现方框，通常就是某个新中文字符没有进入字体，需要重新生成并烧录。

## 常见问题

### 1. 黑屏

确认打开的是项目根目录，且完整执行过 `idf.py build`。不要把另一个项目的 `app_main`、BSP 初始化或旧 `sdkconfig` 直接覆盖进来，否则可能造成 LCD 初始化冲突。

### 2. 天气显示“检查网络后再试”

确认电脑和开发板在同一个 Wi-Fi，`start_demo.bat` 窗口显示 `Proxy OK.`，并且 Windows 防火墙允许端口 `8787`。可以双击 `check_proxy.bat` 验证 `/weather` 和 `/time`。

### 3. 相机有画面但没有人脸框

保持人脸距离摄像头约 30 到 80 厘米，避免强逆光。串口日志应出现真实视觉后端初始化信息。页面里的 `FACE: YES`、坐标和置信度都来自 ESP-WHO 本地推理，不是固定假框。

### 4. 中文显示方框

运行：

```powershell
node tools\generate_lexin_fonts.js
idf.py build
```

然后重新烧录。主页标题中的 `LeXin` 使用英文字体，避免项目名缺字造成左上角方框。

## 合并新功能注意事项

- 不要新增第二个 `app_main`。
- 不要重复初始化 LCD、触摸、摄像头和 BSP。
- 新的视觉能力放到 `components/lexin_vision/`。
- 新的 UI 页面放到 `main/lexin_display_test.c`。
- 新的本地建议特征放到 `main/lexin_edge_advisor.cpp`。
- 合并后至少验证四个入口：天气、日历、情绪研伴、研伴建议。
