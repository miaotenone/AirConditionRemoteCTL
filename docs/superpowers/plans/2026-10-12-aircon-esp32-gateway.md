# 空调远程控制网关（ESP32）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建 ESP32-WROOM-32E 固件：通过 MQTT 接收指令、经 UART 以高配版红外模块的多参数协议控制美的空调、OLED 显示状态、网页配网。

**Architecture:** 纯函数编码器（`ac_encoder`）与状态模型（`ac_state`）脱离硬件、可原生单元测试；硬件/网络层（`ir_module` / `net_mqtt` / `display` / `config_store` / `web_config`）薄封装；`main.cpp` 仅做编排。

**Tech Stack:** PlatformIO + Arduino-ESP32；U8g2、PubSubClient、ArduinoJson、WiFiManager；原生测试用 g++。

**Spec:** `docs/superpowers/specs/2026-10-12-aircon-esp32-gateway-design.md`

## Global Constraints

- 高配版红外模块：UART **115200 8N1**。
- 帧：`68 | len(2B LE) | 模块地址 | AFN | 数据 | 校验和 | 16`。
- 多参数：AFN=`0x30`，len=`0x0C`，校验固定 `0x55`。
- 品牌 美的1=`0x10`；模式 自动0/制冷1/除湿2/送风3/制热4；温度 16~30 直接作为字节 `0x10~0x1E`；风量 自动0/低1/中2/高3；开关 开1/关0。
- 引脚：I2C SDA=GPIO21、SCL=GPIO22；红外模块 RX=GPIO16、TX=GPIO17。
- MQTT topics：`ac/cmd`（入）、`ac/state`（retained）、`ac/status`（LWT）。
- 所有可调项进 NVS，配网页可改，换网络/服务器无需重编译。

## Review Focus

- 温度/模式/风量越界输入：应被夹取到合法范围而不是产出非法帧。
- `ac/cmd` 里的未知字段或非法 JSON：应忽略/报错而不崩溃、不改状态。
- 校验和/字节序错误：帧必须与 IrStudy 生成的一致（尤其 `0x55` 固定校验）。
- WiFi/MQTT 掉线：应自动重连并保留空调状态，`ac/status` 反映离线。
- 连续快速指令：最终 `ac/state` 必须等于最后一次生效的指令。

---

### Task 1: 工程脚手架 + `ac_encoder`（原生 TDD）

**Files:**
- Create: `platformio.ini`
- Create: `src/pins.h`, `src/config.h`
- Create: `src/ac_encoder.h`, `src/ac_encoder.cpp`
- Create: `tests/native/ac_encoder_test.cpp`
- Create: `.gitignore`（追加）

**Interfaces:**
- Produces:
  - `uint8_t frame_checksum(const uint8_t* data, size_t len);`
  - `constexpr uint8_t AFN_MULTI_PARAM = 0x30; uint8_t AFN_SINGLE_PARAM = 0x31;`
  - `size_t build_multi_param_frame(uint8_t* out, uint8_t brand, uint8_t power, uint8_t mode, uint8_t temp, uint8_t fan);` 返回 12
  - `size_t build_single_param_frame(uint8_t* out, uint8_t param_index, uint8_t value);` 返回 9
  - `size_t build_learn_frame(uint8_t* out, uint8_t slot);` 返回 8
  - `size_t build_send_learned_frame(uint8_t* out, uint8_t slot);` 返回 8
  - `constexpr uint8_t BRAND_MIDEA1 = 0x10;`

- [ ] **Step 1: 写失败测试**

`tests/native/ac_encoder_test.cpp`：断言 `build_multi_param_frame` 对（美的1, 开, 制冷, 24, 自动风）产出 `68 0C 00 FF 30 10 01 01 18 00 55 16`；断言学习帧 `slot 0` → `68 08 00 FF 10 00 0F 16`、发送帧 → `68 08 00 FF 12 00 11 16`；断言 `frame_checksum` 对 `{0xFF,0x10,0x00}` = `0x0F`。

- [ ] **Step 2: 运行测试确认失败**（`g++` 编译报未定义）

- [ ] **Step 3: 实现 `ac_encoder`**（纯 C++，仅 `<stdint.h>/<stddef.h>`）

- [ ] **Step 4: 运行测试通过**

- [ ] **Step 5: `pio run` 编译通过**

- [ ] **Step 6: 提交**

### Task 2: `ac_state`（状态模型 + 夹取）

**Files:** Create `src/ac_state.h`, `src/ac_state.cpp`；Test `tests/native/ac_state_test.cpp`

**Interfaces:**
- Produces `struct AcState { uint8_t power, mode, temp, fan; };`
  - `void ac_state_clamp(AcState&);`
  - `bool ac_state_apply_field(AcState&, const char* field, int value);`（未知字段返回 false）
- Consumes: `ac_encoder` 的边界常量。

- [ ] Step 1: 写失败测试（越界 temp=99 → 30；未知字段 → false；mode=7 → 4）
- [ ] Step 2: 运行失败
- [ ] Step 3: 实现
- [ ] Step 4: 通过
- [ ] Step 5: 提交

### Task 3: `ir_module`（UART 封装）

**Files:** Create `src/ir_module.h`, `src/ir_module.cpp`

**Interfaces:** `void ir_module_begin(); void ir_module_send(const uint8_t* frame, size_t len);`
- Consumes: `pins.h` (IR_RX_PIN/IR_TX_PIN)、`config.h` (IR_BAUD)

- [ ] Step 1: 实现 begin（`Serial2.begin(IR_BAUD, SERIAL_8N1, IR_RX_PIN, IR_TX_PIN)`）
- [ ] Step 2: 实现 send（写字节 + 刷新）
- [ ] Step 3: `pio run` 通过并提交

### Task 4: `config_store` + `web_config`

**Files:** Create `src/config_store.h/.cpp`, `src/web_config.h/.cpp`

**Interfaces:**
- `struct AppConfig { String wifi_ssid, wifi_pass, mqtt_host; uint16_t mqtt_port; String mqtt_user, mqtt_pass, topic_prefix, device_name; uint8_t brand; AcState initial; };`
- `bool config_store_load(AppConfig&); void config_store_save(const AppConfig&);`
- `void web_config_run(AppConfig&);`（WiFiManager 门户 + 自定义参数）

- [ ] Step 1: 实现 NVS 读写（Preferences + JSON 字符串）
- [ ] Step 2: 实现 WiFiManager 门户与自定义参数页
- [ ] Step 3: `pio run` 通过并提交

### Task 5: `display`（U8g2 OLED）

**Files:** Create `src/display.h/.cpp`

**Interfaces:**
- `void display_begin();`
- `void display_show_status(const char* line1, const char* line2, const char* line3, const char* line4);`
- `void display_show_ac(const AcState&, const char* mqtt_state);`
- I2C 地址自动探测 0x3C/0x3D，SSD1306/SH1106 自适配。

- [ ] Step 1: 实现初始化（U8g2 构造 + 探测）
- [ ] Step 2: 实现状态渲染
- [ ] Step 3: `pio run` 通过并提交

### Task 6: `net_mqtt`（WiFi + MQTT）

**Files:** Create `src/net_mqtt.h/.cpp`

**Interfaces:**
- `void net_begin(const AppConfig&);`
- `void net_loop();`
- `void net_publish_state(const AppConfig&, const AcState&, bool online);`
- 回调：`void net_set_command_handler(void (*)(const char* payload, size_t len));`
- 内部：WiFi 重连、PubSubClient 重连、订阅 `{prefix}/cmd`、LWT `{prefix}/status`、retained 发布 `{prefix}/state`。

- [ ] Step 1: WiFi + MQTT 连接与重连
- [ ] Step 2: 订阅与回调解析（ArduinoJson）
- [ ] Step 3: `pio run` 通过并提交

### Task 7: `main.cpp` 编排 + 编译验证

**Files:** Create `src/main.cpp`

- [ ] Step 1: `setup()`：config 加载 → 配网 → OLED → UART → MQTT → 初始状态发布
- [ ] Step 2: `loop()`：`net_loop()`；处理指令 → 合并状态 → 发帧 → 发布状态 → 刷新 OLED
- [ ] Step 3: `pio run` 编译通过（粘贴输出）
- [ ] Step 4: 提交

### Task 8: 文档

**Files:** Create `README.md`、`docs/protocol.md`、`docs/wiring.md`

- [ ] Step 1: 写协议编解码表（含 0x30/0x31/学习帧与校验算法）
- [ ] Step 2: 写接线表与默认引脚
- [ ] Step 3: 写 README（构建/烧录/配网/联调步骤）
- [ ] Step 4: 提交

## Self-Review

- Spec 覆盖：MQTT 契约（Task 6）、多参数帧与品牌/模式/温度/风量（Task 1/2）、OLED（Task 5）、网页配网/免重编译（Task 4）、UART（Task 3）均有对应任务。
- Review Focus 各项均由 Task 1/2/6/7 的测试或夹取逻辑覆盖。
