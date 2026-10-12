# 树莓派 + 飞书服务 · 接口规格（定稿 v1）

> 本文件描述**另一个仓库**（树莓派侧服务）的设计与接口契约。
> 它和 ESP32 固件仓库的共同边界只有一件事：**MQTT 契约**。
> 建议本文件原样复制为该仓库的 `docs/interface-spec.md`，两个仓库各自引用同一份契约。

日期：2026-10-12
状态：**定稿 v1**（决策记录见 §12）
上游仓库：`AirConditionRemoteCTL`（ESP32 固件，已完成）

---

## 1. 目的与范围

在树莓派上常驻一个服务，把**飞书机器人的交互卡片**与**局域网内 ESP32 网关的 MQTT 状态**连起来，让用户像用遥控器一样在飞书里控制空调。

**做：**
- 订阅 ESP32 的状态，维护"当前空调状态 + 在线状态"缓存。
- 提供一张可点击的**遥控器卡片**（显示屏上状态 + 按钮控制）。
- 把用户点击翻译成 MQTT 指令发给 ESP32。
- 断线自动重连；重启后能从 retained 状态恢复。

**不做：**
- 不做云端中转（只在局域网 + 飞书开放平台之间）。
- 不直接驱动红外（那是 ESP32 的事）。
- 不做群聊 / 多用户（v1 **私聊单用户**）。
- 不做多台空调（v1 单台；topic 前缀留出扩展位）。
- 不做温度曲线 / 定时场景（列为后续）。

---

## 2. 架构与数据流

```
┌────────┐  卡片回调(长连接)  ┌──────────────────────┐
│ 飞书 App │ ←──────────────→ │  树莓派服务            │
│ (手机端) │  消息/卡片 API     │  ac-bot               │
└────────┘                   │  ├─ feishu_client     │
                             │  ├─ state (内存+SQLite)│
                             │  └─ mqtt_client       │
                             └──────────┬───────────┘
                                        │ MQTT (局域网)
                                   ┌────▼────┐
                                   │ Mosquitto│  ← broker 也在树莓派
                                   └────┬────┘
                                        │ MQTT
                                   ┌────▼────────────┐
                                   │ ESP32 网关       │
                                   │ → 红外 → 美的空调 │
                                   └─────────────────┘
```

**两条独立通道：**
1. **飞书通道**：进 = 卡片按钮回调（长连接）；出 = 发送/更新卡片。
2. **MQTT 通道**：进 = `ac/state`、`ac/status`（retained）；出 = `ac/cmd`。

服务是**无状态中转 + 本地状态缓存**，真值（source of truth）在 ESP32 的 `ac/state`。

---

## 3. 技术选型

| 关注点 | 选择 | 理由 |
|---|---|---|
| 语言 | Python 3.11+ | 生态成熟、树莓派原生 |
| MQTT | `paho-mqtt`（或 `aiomqtt`） | 与 Mosquitto 配合，支持 LWT/retained |
| 飞书 | 官方 `lark-oapi` + **长连接**（`lark.ws.Client`） | 无需公网 IP / 内网穿透，树莓派直接收事件 |
| 配置 | 环境变量 + `.env`（`pydantic-settings`） | 密钥不落库、部署简单 |
| 持久化 | SQLite（仅存"卡片 message_id ↔ chat"映射、可选历史） | 重启后能更新旧卡片 |
| 常驻 | systemd unit | 开机自启、崩溃重启 |
| 日志 | 标准 logging，JSON 可选 | 便于排查 |

> **为什么用长连接而不是 Webhook**：Webhook 需要飞书能访问到你的公网地址；树莓派在局域网里只有内网 IP。长连接由客户端主动连飞书，无需暴露端口。

---

## 4. MQTT 契约（与固件仓库的唯一接口）

**Broker**：Mosquitto，跑在树莓派，默认 `1883`（只监听局域网）。

### 4.1 Topics

| Topic | 方向（相对本服务） | QoS | Retained | Payload |
|---|---|---|---|---|
| `{prefix}/cmd` | 发布 | 1 | 否 | 只含要改字段的 JSON |
| `{prefix}/state` | 订阅 | 1 | **是** | 完整状态 JSON |
| `{prefix}/status` | 订阅 | 1 | **是** | 在线/离线 JSON（LWT） |

`prefix` 默认 `ac`，由固件配网页设定，两边必须一致。

### 4.2 Payload 定义

**`{prefix}/cmd`（服务 → ESP32）** — 只带需要改变的字段：

```json
{ "power": 1 }
{ "temp": 24 }
{ "mode": 1, "fan": 2 }
```

| 字段 | 类型 | 取值 |
|---|---|---|
| `power` | int | `0` 关 / `1` 开 |
| `mode` | int | `0` 自动 / `1` 制冷 / `2` 除湿 / `3` 送风 / `4` 制热 |
| `temp` | int | `16`~`30`（℃） |
| `fan` | int | `0` 自动 / `1` 低 / `2` 中 / `3` 高 |

- 越界值由固件夹取，但**服务侧应先校验**，不要依赖下游兜底。
- 未知字段固件会忽略并打日志；服务侧不得发送未知字段。
- 用户侧不提供 `brand` 修改（属配置项，走固网页）。

**`{prefix}/state`（ESP32 → 服务）** — retained，完整状态：

```json
{ "online": true, "power": 1, "mode": 1, "temp": 24, "fan": 0, "brand": 16 }
```

**`{prefix}/status`（ESP32 → 服务）** — retained，遗嘱通道：
- ESP32 上线时发布 `{"online":true}`；
- 掉线时由 broker 代发遗嘱 `{"online":false}`。

> 固件每 30s 心跳重发 `ac/state`。服务可用"最后一条 `ac/state` 的到达时间"判断数据新鲜度，超时（如 >90s）但 `status.online=true` 时提示"状态可能过期"。

### 4.3 时序（一次点击）

```
用户点"制冷" 
  → 飞书长连接回调 action.value={"a":"mode","v":1}
  → 服务 3s 内先回一张"乐观更新"的卡片（避免超时）
  → 服务发布 ac/cmd {"mode":1} (QoS1)
  → ESP32 发红外帧 → 更新自身状态
  → ESP32 发布 ac/state (retained)
  → 服务收到，用真值刷新卡片（去掉"待确认"标记）
```

---

## 5. 飞书交互规格

### 5.1 交互形态

一句话：**一个会话里一张"遥控器"卡片，点按钮就地更新同一张卡片**（不是每条消息刷屏）。

**v1 范围：私聊单用户** —— 只服务 owner 自己，机器人**不拉群**、不做多人并发。因此"卡片归属"只有一条记录，实现大幅简化。

- 用户对机器人发送任意消息（如 `/ac`、`遥控器`）→ 回复一张卡片。
- 之后所有点击都**原地更新**这张卡片。
- 服务在 SQLite 记录 `owner_chat_id → message_id`（单条），重启后仍能更新旧卡片。
- 非 owner 的会话/事件一律忽略（`OWNER_OPEN_ID` 之外不响应）。

### 5.2 按钮动作 schema

卡片里每个可点击元素携带统一的 `value`：

```json
{ "a": "<action>", "v": <int> }
```

| `a` | `v` | 含义 |
|---|---|---|
| `power` | 0 / 1 | 关机 / 开机 |
| `mode` | 0~4 | 设定模式 |
| `temp` | 16~30 | 直接设定温度 |
| `temp_step` | -1 / +1 | 温度 ±1（遥控器手感） |
| `fan` | 0~3 | 设定风速 |
| `refresh` | 0 | 强制重发卡片（按真值重绘） |

> 服务只接受**上表列出的 `a`**；未知 `a` 直接忽略并记日志。
>
> **温度两种入口，动作统一**：`－/＋` 按钮发 `temp_step`；下拉选择发 `temp`（16~30）。两者最终都归一化到同一个 `AcState.temp`。
>
> **回调字段归一化**：按钮回调用 `action.value`，下拉（`select_static`）回调用 `action.option`。`feishu_client` 需把二者统一成 `{a, v}` 再交给 `state`，让 `state` 完全不关心飞书组件差异。

### 5.3 卡片布局

```
┌──────────────────────────────────────────────┐
│  ❄️ 空调遥控器                       🟢 在线   │
├──────────────────────────────────────────────┤
│  状态：开机 · 制冷 · 24℃ · 自动风               │
│  更新时间：12:03:41                            │
├──────────────────────────────────────────────┤
│  电源   [ 开机 ] [ 关机 ]                      │
│  模式   [自动][制冷][除湿][送风][制热]          │
│  温度   [ － ] [ 24℃ ] [ ＋ ]   [ 选择 ▾ ]     │
│  风速   [自动][低][中][高]                      │
│                                   [ 刷新 ]     │
└──────────────────────────────────────────────┘
```

- 当前生效的档位用高亮/主色按钮表示（如"制冷"用 primary）。
- 关机时把 模式/温度/风速 组置灰（`disabled`），只留 开机。
- `🟢 在线` / `🔴 离线` 由 `status.online` 驱动。
- 乐观更新期间状态行末尾加 `⏳`；确认后去掉；超时改 `⚠️ 未确认`。
- **温度两件套都放**：`－/＋` 用于微调（遥控器手感）；右侧 `选择 ▾` 是 `select_static` 下拉，一次列出 16~30 快捷跳转。二者指向同一状态字段。

### 5.4 卡片 JSON 骨架（飞书卡片 v2，示意）

```json
{
  "schema": "2.0",
  "config": { "update_multi": true },
  "header": {
    "title": { "tag": "plain_text", "content": "❄️ 空调遥控器" },
    "template": "blue"
  },
  "body": {
    "elements": [
      {
        "tag": "markdown",
        "content": "**状态**：开机 · 制冷 · 24℃ · 自动风\n**连接**：🟢 在线 · 更新于 12:03:41"
      },
      {
        "tag": "column_set",
        "columns": [
          { "tag": "column", "width": "auto",
            "elements": [ { "tag": "markdown", "content": "**电源**" } ] },
          { "tag": "column", "width": "weighted", "weight": 1,
            "elements": [
              { "tag": "button", "size": "medium", "type": "danger",
                "text": { "tag": "plain_text", "content": "关机" },
                "behaviors": [ { "type": "callback", "value": { "a": "power", "v": 0 } } ] }
            ] }
        ]
      },
      {
        "tag": "markdown",
        "content": "**模式**"
      },
      {
        "tag": "button", "size": "medium", "type": "primary",
        "text": { "tag": "plain_text", "content": "制冷" },
        "behaviors": [ { "type": "callback", "value": { "a": "mode", "v": 1 } } ]
      },
      {
        "tag": "button", "size": "medium",
        "text": { "tag": "plain_text", "content": "－" },
        "behaviors": [ { "type": "callback", "value": { "a": "temp_step", "v": -1 } } ]
      },
      {
        "tag": "button", "size": "medium",
        "text": { "tag": "plain_text", "content": "24℃" },
        "behaviors": [ { "type": "callback", "value": { "a": "temp", "v": 24 } } ]
      },
      {
        "tag": "button", "size": "medium",
        "text": { "tag": "plain_text", "content": "＋" },
        "behaviors": [ { "type": "callback", "value": { "a": "temp_step", "v": 1 } } ]
      },
      {
        "tag": "select_static",
        "placeholder": { "tag": "plain_text", "content": "选择温度" },
        "options": [
          { "text": { "tag": "plain_text", "content": "16℃" },
            "value": { "a": "temp", "v": 16 } },
          { "text": { "tag": "plain_text", "content": "24℃" },
            "value": { "a": "temp", "v": 24 } }
        ]
      }
    ]
  }
}
```

> 上面为**结构示意**，实际按钮分组请按 5.3 的 `column_set` 排布，并用 `disabled`/`type` 表达当前档位。
> `select_static` 的 `options` 实际应列出 **16~30 全部 15 项**，这里只留两项示意。
> 飞书卡片 schema 版本请以官方文档为准，实现时**锁定一个版本**并在 `cards.py` 里集中构造。

### 5.5 回调处理约定

- 飞书要求回调 **3 秒内**响应 → 服务先返回"已接受 + 乐观卡片"，**不要**等 MQTT。
- MQTT 往返 + 红外 ≈ 数十~数百毫秒，但网络抖动时可能更久，因此异步是必须的。
- 所有点击**串行化**（单用户单队列），避免乱序导致卡片抖动。
- 连续 `temp_step` 快速点击：**合并**为最终值再发一条 `ac/cmd`（去抖窗口建议 300~500ms）。

---

## 6. 服务内部状态

```python
@dataclass
class AcState:
    power: int = 0      # 0/1
    mode: int  = 1      # 0..4
    temp: int  = 24     # 16..30
    fan: int   = 0      # 0..3

@dataclass
class Presence:
    online: bool = False
    last_state_at: float = 0.0   # 最后一条 ac/state 的时间戳
```

- 启动后先等 retained `ac/state` 到达（给 2~3s 窗口），再渲染卡片，避免显示空状态。
- **乐观更新**：点击后本地预测新状态并立即渲染，标记 `⏳`；收到 `ac/state` 与预测一致则清除标记；不一致则以真值为准并提示。
- 幂等：重复收到相同 `ac/state` 不重复编辑卡片（比较内容哈希）。

---

## 7. 配置项

| 变量 | 说明 | 默认 |
|---|---|---|
| `FEISHU_APP_ID` | 飞书应用 App ID | （必填） |
| `FEISHU_APP_SECRET` | 飞书应用 Secret | （必填） |
| `MQTT_HOST` | broker 地址 | `127.0.0.1` |
| `MQTT_PORT` | broker 端口 | `1883` |
| `MQTT_USER` / `MQTT_PASS` | 账号（可空） | 空 |
| `MQTT_PREFIX` | topic 前缀，须与固件一致 | `ac` |
| `OWNER_OPEN_ID` | 允许控制的唯一用户 open_id（私聊单用户） | （必填） |
| `LOG_LEVEL` | 日志级别 | `INFO` |

---

## 8. 部署

```ini
# /etc/mosquitto/conf.d/ac.conf
listener 1883 0.0.0.0
allow_anonymous true          # 内网可接受；要求更高则改用密码/ACL
persistence true
persistence_location /var/lib/mosquitto/
```

```ini
# /etc/systemd/system/ac-bot.service
[Unit]
Description=AC Feishu Bot
After=network-online.target mosquitto.service
Wants=mosquitto.service

[Service]
User=pi
WorkingDirectory=/opt/ac-bot
EnvironmentFile=/opt/ac-bot/.env
ExecStart=/opt/ac-bot/.venv/bin/python -m app.main
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

---

## 9. 建议的仓库结构

```
ac-bot/
  app/
    main.py            # 装配：配置 → MQTT → 飞书 → 事件循环
    config.py          # pydantic-settings
    mqtt_client.py     # 连接/重连/订阅/发布 + 回调分发
    state.py           # AcState / Presence / 乐观更新
    cards.py           # 卡片 JSON 构造（纯函数，可单测）
    feishu_client.py   # 长连接、发卡/更新卡、回调路由
    store.py           # SQLite: owner_chat_id ↔ message_id（单条）
  tests/
    test_cards.py      # 卡片构造纯函数
    test_state.py      # 乐观更新/合并/夹取
  deploy/
    mosquitto-ac.conf
    ac-bot.service
  .env.example
  pyproject.toml
  README.md
```

> 沿用固件仓库的做法：把 **`cards.py`（卡片构造）** 和 **`state.py`（状态合并/夹取）** 做成**纯函数**，脱离飞书与 MQTT 做单元测试。

---

## 10. 错误处理与可观测性

| 场景 | 行为 |
|---|---|
| MQTT 断开 | 指数退避重连；卡片显示 `🔴 离线` |
| 发布 `ac/cmd` 后 5s 无 `ac/state` 变化 | 卡片标 `⚠️ 未确认`，保留用户输入供重试 |
| 飞书 API 失败 | 重试 1 次；仍失败则记日志、不改本地状态 |
| 状态超过 90s 未更新但 `online=true` | 卡片提示"状态可能过期" |
| 非 owner 的消息/点击 | 忽略 + 记日志（不回复，避免探测） |

日志至少包含：每次 `ac/cmd`（payload）、每次 `ac/state` 变化、每次卡片更新、每次重连。

---

## 11. 验收标准

1. 树莓派重启后，服务自动连上 MQTT 与飞书，卡片自动恢复为**真值**（retained 状态）。
2. 点"制冷"→ 5 秒内空调切换，卡片状态与 `ac/state` 一致。
3. 点"＋"三下 → 温度升 3℃，服务只发一条合并后的 `ac/cmd`。
4. 拔掉 ESP32 电源 → 卡片在一个心跳周期内变 `🔴 离线`。
5. ESP32 断电期间点击按钮 → 卡片提示"未确认/离线"，不崩、不刷屏。
6. 发送非法字段/越界温度 → 服务侧被拒或夹取，绝不发未知字段给固件。
7. `cards.py` / `state.py` 单元测试全绿，无需飞书与 MQTT 环境。

---

## 12. 决策记录

### 已定（v1）

1. **控制入口**：**私聊单用户**。只服务 owner 一个人的私聊，机器人**不拉群**；`OWNER_OPEN_ID` 之外一律忽略。
2. **温度交互**：**`－/＋` 步进 + 下拉选择，两者都放**。步进用于微调，下拉用于 16~30 快捷跳转。
3. **飞书接入**：**自建应用 + 机器人能力 + 长连接**（不暴露公网，无需内网穿透）。
4. **定时 / 场景**：v1 **不做**。
5. **多空调**：v1 **不做**，topic 前缀固定 `ac`。

### 待确认（不阻塞 v1 开工）

1. **MQTT 鉴权**：内网 `allow_anonymous` 是否可接受，还是要上账号 / ACL？
2. **飞书自建应用权限**：确认你能创建自建应用并拿到 `App ID/Secret`，并开通长连接所需的事件与权限（接收消息、卡片回调、发送 / 更新消息）。

> 未来扩展路径：群聊 → 把 `OWNER_OPEN_ID` 换成白名单、每 chat 各一张卡；多空调 → topic 前缀改 `ac/<room>`，并在固件配网页同步改 `topic_prefix`。
