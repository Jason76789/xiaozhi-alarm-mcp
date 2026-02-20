# 小智 AI 闹钟系统 (Xiaozhi Alarm with MCP Server)

（中文 | [English](README_en.md) | [日本語](README_ja.md)）

## 项目简介

这是一个基于 ESP32-S3 的 AI 闹钟系统，采用 MCP (Model Context Protocol) 服务器架构，提供智能语音交互和闹钟管理功能。项目支持多种硬件平台，具有强大的扩展性和灵活性。

## 核心功能

### 1. 智能语音交互
- 离线语音唤醒（支持 ESP-SR 框架）
- 流式语音识别 (ASR)
- 自然语言处理 (LLM) 集成
- 语音合成 (TTS) 输出
- 支持多种通信协议（WebSocket 或 MQTT+UDP）

### 2. 闹钟管理系统
- **多种闹钟类型**：支持一次性闹钟、循环闹钟、倒计时闹钟
- **持久化存储**：使用 NVS (Non-Volatile Storage) 存储闹钟配置
- **灵活配置**：可设置闹钟名称、触发时间、重复模式、间隔等
- **实时提醒**：闹钟触发时播放提示音并震动
- **MCP 工具集成**：通过 MCP 协议远程管理闹钟

### 3. 硬件支持
- **ESP32-S3** 芯片平台
- **多种显示屏**：OLED、LCD 支持表情显示
- **音频编解码**：K10AudioCodec 音频处理
- **摄像头**：GC2145 摄像头传感器（可选）
- **LED 控制**：地址able LED 条支持
- **GPIO 扩展**：PCA9554 IO 扩展器
- **电源管理**：电量显示与低功耗模式

### 4. MCP 协议扩展
- **设备控制工具**：通过 MCP 协议实现设备控制
- **硬件抽象层**：支持多种硬件平台和传感器
- **云端集成**：可与大模型服务器通信
- **动态工具注册**：根据硬件自动注册可用工具

## 架构设计

### 核心组件

1. **Application** (`main/application.cc`)
   - 设备状态机管理（12种状态）
   - 语音交互流程控制
   - MQTT 连接管理
   - 状态切换与事件处理

2. **Alarm Manager** (`main/alarm_manager.cc/h`)
   - 闹钟生命周期管理
   - 定时器调度与触发
   - 持久化存储管理
   - 闹钟数量限制（最多支持10个闹钟）

3. **MCP Server** (`main/mcp_server.cc/h`)
   - MCP 协议实现
   - 工具注册与管理
   - JSON-RPC 2.0 通信
   - 设备控制接口

4. **Audio Service** (`main/audio/audio_service.cc/h`)
   - 音频捕获与播放
   - 编解码处理
   - 语音唤醒检测
   - 音频效果处理

### 状态机设计

设备支持以下状态：
- `kDeviceStateUnknown`：未知状态
- `kDeviceStateStarting`：启动中
- `kDeviceStateConfiguring`：配置中
- `kDeviceStateIdle`：空闲状态（可休眠）
- `kDeviceStateConnecting`：连接服务器
- `kDeviceStateListening`：聆听状态
- `kDeviceStateSpeaking`：播放状态
- `kDeviceStateUpgrading`：升级中
- `kDeviceStateActivating`：激活中
- `kDeviceStateAudioTesting`：音频测试
- `kDeviceStateFatalError`：致命错误

## 关键修复

### 1. 回答完毕后不进入聆听状态

**问题**：当 AI 回答完毕时，系统可能停留在 Speaking 状态，导致无法继续对话。

**解决方案**：
- 优化 `tts` -> `stop` 消息处理逻辑
- 根据不同的聆听模式（ManualStop、AutoStop、Realtime）正确切换状态
- 增强状态切换的日志输出，便于调试

**修改文件**：`main/application.cc:435-451`

### 2. 防止闹钟触发前系统进入休眠

**问题**：系统在闹钟即将触发时可能进入低功耗模式，导致闹钟无法正常响铃。

**解决方案**：
- 在 `CanEnterSleepMode()` 函数中添加闹钟保护逻辑
- 检查 AlarmManager 中是否有“已激活且未触发”的闹钟
- 添加 `get_active_alarm_count()` 接口获取活跃闹钟数量
- 在闹钟存在时禁止进入低功耗模式

**修改文件**：`main/application.cc:758-783`、`main/alarm_manager.h:29`、`main/alarm_manager.cc:227-237`

### 3. 闹钟响铃无声音

**问题**：闹钟触发时可能没有声音提示。

**解决方案**：
- 在闹钟触发时先清理当前对话状态
- 播放本地提示音（使用 P3_EXCLAMATION 声音）
- 确保音频服务已初始化

**修改文件**：`main/alarm_manager.cc:280-300`

## 开发环境

### 构建工具
- **ESP-IDF 5.4** 或更高版本
- **CMake** 构建系统
- **Ninja** 构建工具

### 开发流程

```bash
# 编译项目
idf.py build

# 擦除 flash 并烧录
idf.py erase-flash
idf.py flash

# 监视串口输出
idf.py monitor
```

### 项目结构
```
main/
├── application.cc/h          # 主应用程序
├── alarm_manager.cc/h        # 闹钟管理
├── mcp_server.cc/h           # MCP 服务器
├── audio/                    # 音频处理
├── boards/                   # 硬件支持
├── display/                  # 显示驱动
├── led/                      # LED 控制
├── protocols/                # 通信协议
├── partitions/               # 分区表
└── scripts/                  # 工具脚本
```

## 使用说明

### 设置闹钟

通过语音指令或 MCP 协议设置闹钟：
```
"设置一个明天早上7点的闹钟"
"30分钟后提醒我"
"每天下午3点的闹钟"
```

### 管理闹钟

- 查询所有闹钟：`"查询所有闹钟"`
- 删除闹钟：`"删除明天的闹钟"`
- 删除关键词匹配的闹钟：`"删除名字包含会议的闹钟"`

### 硬件支持

项目支持多种开发板和硬件平台，详细列表请查看 [main/boards/README.md](main/boards/README.md)。

## 许可证

MIT License

## 联系方式

如有问题或建议，请通过以下方式联系：
- Issue：https://github.com/Jason76789/xiaozhi-alarm-mcp/issues
- 邮箱：1935724779@qq.com

## 致谢

本项目基于虾哥开源的 ESP32 项目开发，感谢原作者的贡献。
