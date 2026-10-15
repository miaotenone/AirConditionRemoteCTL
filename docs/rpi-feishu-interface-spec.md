# 树莓派 + 飞书服务 · 接口规格（定稿 v1）

> 📌 **本文件是镜像**：原文是服务端仓库 `AirConditionRemoteServer/docs/interface-spec.md`，
> 逐字节复制到这里（固件是契约的另一半，手上得有）。**改契约请先改服务端、再复制过来**，
> 不要在本仓库里单独改。镜像时间：**2026-10-15**（v1.2 的 `env` / `hb` / `event` 三个 topic 已在 §4.1）。

> 本文件是**服务端仓库**（`AirConditionRemoteServer`，树莓派侧服务）的设计与接口契约。
> 它和 ESP32 固件仓库 `AirConditionRemoteCTL` 的共同边界只有一件事：**MQTT 契约**。
> 契约的权威实现在固件仓库（`src/net_mqtt.cpp`）；本仓库引用同一份契约，两边必须一致。

日期：2026-10-12
状态：**定稿 v1**（决策记录见 §12）
上游仓库：`AirConditionRemoteCTL`（ESP32 固件，已完成）
本仓库：`AirConditionRemoteServer`（树莓派 + 飞书服务；**MQTT 链路已验证**，服务本体未开工）
当前状态（2026-10-15）：v1 **已上线并在真机使用**；v1.2（预设 / 天气 / DHT11 夜间闭环 / 失联保护）也已上线，
遗留与缺口见 [`../README.md`](../README.md) 与设计文档 §18.12。**本文以下仍是 v1 的规格原文**，
v1.1/v1.2 的增补就地标在对应位置。

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
| 语言 | Python 3.10+ | 生态成熟、树莓派原生（目标树莓派现装 3.10.12，足够）|
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
| `{prefix}/env` | 订阅 | 1 | **是** | DHT11 读数 `{temp,humidity,ok}` |
| `{prefix}/hb` | 发布 | 0 | 否 | 服务心跳 `{ts}`（证明"决策者活着"）|
| `{prefix}/event` | 订阅 | 1 | **是** | 设备事件 `{type,ts,prev}` |

`prefix` 默认 `ac`，由固件配网页设定，两边必须一致。

> ⚠️ **上表的 QoS 是设计意图，线上实际是 QoS0**（2026-10-15 核对）：ESP32 侧用的是 **PubSubClient**，
> 它的 `publish()` 第三个参数是 `retained`、**没有 QoS 参数**（只能 QoS0），`subscribe()` 同理。
> **`Retained` 那一列是真的**（固件都传了 `true`）——retained + 固件每 30s 重发 `state`，才是实际兜底的东西。
> 所以：**不要依赖 QoS1 的投递保证**（掉一条 `ac/state`，服务端到 `CONFIRM_TIMEOUT_S` 就会显示「⚠️ 未确认」）。
> 要真 QoS1 得换客户端库、或把契约改成写实情——都还没做。设计文档 §18.12.4。

> **为什么还要 `hb`**：Mosquitto 跑在树莓派上但**是独立的 systemd 单元**，
> 所以「MQTT 连上了」并不等于「发命令的那个服务活着」。`hb` 是设备唯一能
> 拿到的、关于**决策者**在不在的证据（设计 18.8）。
>
> **`env` / `event` 是 retained 的陷阱**：broker 会把 retained 消息重放给
> 新订阅者。服务必须只看**非 retained** 的新消息（重放的是历史，不是读数），
> 且 `event` 只认 `ts` 在 10 分钟以内的（设计 18.6）。

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

**`{prefix}/env`（ESP32 → 服务）** — retained，DHT11 读数（每 30s 一条）：

```json
{ "temp": 26, "humidity": 55, "ok": true }
```

| 字段 | 类型 | 取值 |
|---|---|---|
| `temp` | int | 室温 ℃（DHT11 分辨率 1℃）|
| `humidity` | int | 相对湿度 % |
| `ok` | bool | 读数是否有效；`false` 时前两项无意义 |

- `ok=false` 只表示"这一次没读出来"，**服务不得把它当数据**（不入历史、不参与闭环）。
- 服务只采信**非 retained** 的新消息；超过 10 分钟没新读数就不参与决策。

**`{prefix}/hb`（服务 → ESP32）** — 不 retained，QoS0，每 30s 一条：

```json
{ "ts": 1792078200 }
```

**`{prefix}/event`（ESP32 → 服务）** — retained，设备事件（带 `ts`）：

```json
{ "type": "failsafe_off", "ts": 1792078200, "prev": {"power":1,"mode":1,"temp":24,"fan":0} }
```

- 目前只有 `failsafe_off`：设备连续失联 5 分钟后自行关机（见固件仓库设计）。
- 服务收到**非 retained 且 `ts` 在 10 分钟内**的事件才动作；恢复策略要求
  `prev.power == 1`（本来就关机就不必再开）。

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

- 用户对机器人发送召唤词 `设置卡片` → 发一张新卡片到消息流最新位置并撤回旧卡（无文字回执）。
- 发其它消息（如 `开机`）→ 解析成动作 + 文字回执；会话里没有卡片时**补一张**，之后只**原地更新**这张卡片。
- 之后所有点击都**原地更新**当前这张卡片。
- 旧召唤词 `/ac`、`/start`、`ac`、`空调` 已退场：现在按 `unclear` 处理（回「没听懂这句。…」，照旧补卡）。
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

**实现落点（2026-10-13）**：已锁定 **Card JSON 2.0**，对照官方
[卡片 2.0 结构](https://open.feishu.cn/document/feishu-cards/card-json-v2-structure) 与
[按钮组件](https://open.feishu.cn/document/feishu-cards/card-json-v2-components/interactive-components/button) 实现，
全部集中在 `app/cards.py`。

两处与上面示意不同、以官方文档为准的实现决定：

1. **按钮回调**用 2.0 的写法：`"behaviors": [{"type": "callback", "value": {"a": ..., "v": ...}}]`
   （2.0 不再支持 `"tag": "action"` 交互模块；按钮直接放进 `elements`）。
2. **下拉选项的 `value` 是字符串**，不是对象 —— 飞书回来的是 `action.option`。
   所以归一化放在 `app/actions.py`：`action.option == "24"` → `("temp", 24)`；
   `action.value` 优先。`app/cards.py` 只管生成，`feishu_client` 只管收，
   `actions.py` 是两者中间的纯函数。

卡片的**视觉排布**（`column_set` 的 `width`/`weight`、按钮换行）需要首次联调时对着真机微调。

### 5.5 回调处理约定

- 飞书要求回调 **3 秒内**响应 → 服务先返回"已接受 + 乐观卡片"，**不要**等 MQTT。
- MQTT 往返 + 红外 ≈ 数十~数百毫秒，但网络抖动时可能更久，因此异步是必须的。
- 所有点击**串行化**（单用户单队列），避免乱序导致卡片抖动。
- 连续 `temp_step` 快速点击：**合并**为最终值再发一条 `ac/cmd`（去抖窗口建议 300~500ms）。

### 5.6 文本控制（v1.1）

私聊里直接发中文短句，与点卡片**同源**：解析出来的仍是 `(action, value)`，进同一个动作队列，
因此去抖、乐观更新、确认超时全部继承 §5.3 / §5.5，`state.py` / `cards.py` / `mqtt_client.py` 未做任何改动。

**分派顺序**（`app/intent.py`，顺序即优先级）：

| # | 类别 | 触发 | 结果 |
|---|---|---|---|
| 1 | 问句 | 含 `状态`/`多少度`/`几度`/`怎么样`，或**以 `吗`/`呢` 结尾** | `query` → 回一条当前状态，不动空调 |
| 2 | 帮助 | 含 `帮助`/`怎么用`/`能做什么`/`指令` | `help` → 回语法清单 |
| 3 | 动作 | 命中下表任一 | `actions` → 入队执行 |
| 4 | 纯否定 | 只有 `不要制冷` 这类 | 回「好，模式没动」，不发 `ac/cmd` |
| 5 | 感受词 | `太热`/`有点冷`/`有点闷` 等 | `suggest` → 只给建议，**永不执行**（设计 D12）|
| 6 | 兜底 | 其它 | `unclear` → 回「没听懂这句」+ 语法清单 |

> 问句必须排在动作前面：`「开着吗」` 里有个 `开`，若先走动作就会误开机。

**词表**（只覆盖空调真实具备的能力）：

| 字段 | 说法 |
|---|---|
| `power` | 开机 / 开空调 / 打开空调 / 开一下 / 启动 · 关机 / 关掉 / 关闭 / 停了 / 不用了 |
| `mode` | 制冷 / 冷风 =1 · 除湿 / 抽湿 =2 · 送风 / 吹风 =3 · 制热 / 暖风 / 热风 =4 · 自动 =0 |
| `temp` | `调到24度` / `设成26℃` / `温度24` / `24度`；`开到26` = 开机 + 26℃ |
| `temp_step` | `调低两度` / `凉快一点` / `凉一点` = 降温 · `升三度` / `高一点` = 升温（支持中文数字）|
| `fan` | `风速最大` / `大风` / `强风` / `风速3` =3 · `中风` / `2档` / `风速2` =2 · `低风` / `弱风` / `风速1` =1 · `自动风` =0 |

数字可写全角（`２４`）或中文（`两`/`三`）；模式名也认空调面板上的英文（`COOL` / `HEAT` / `DRY` / `FAN` / `AUTO`）。

解析规则：最长匹配优先（`自动风` 是风速而非模式、`冷风` 是模式而非风速）；命中片段被消费，
`打开空调` 不会被拆成 `打开` + `空调`；同一字段多次出现**以最后一次为准**（`调到24，算了26` → 26）；
裸数字不构成温度（`2` → 没听懂），除非句子里已出现过温度语境；`档`/`档位`/`风` 绑定风速，
**优先于温度动词**（`调到2档` 是风速 2，不是 2℃）；相对变化**必须带量词**（`低两度` 可以，
`身高180` 不行）。

**短词上锁**（防误触发的核心规则）：`开 / 关 / 停 / 打开 / 关闭 / 停了` 在普通话里到处都是，
所以它们只在**整句是对空调说的**时才算命令 —— 前面只能是称呼（`帮我`/`请`/`麻烦`/`空调`/`把空调`…），
后面只能是 `空调` 或语气词（`了`/`吧`/`着`…）；或者后面**紧跟一个真动作**（`开制冷24度`、`开2档`）。
因此 `「没关系」`、`「开心」`、`「开关」`、`「关灯」`、`「打开心扉」`、`「雨停了」` 一律**不动作**，
回「没听懂」。**代价是刻意的**：`「一直开着」`、`「把窗户关了」` 也不动作 —— 防误触发优先于覆盖率，
一次误触发的代价是物理世界里的空调真的动了。

**回执**（`app/replies.py`，走 `im.v1.message.reply`，回在原消息线程里）：

```
✅ 已开机 · 制冷 · 24℃ · 自动风                     ← 动作成功
✅ 已开机 · 制冷 · 30℃ · 自动风（温度范围 16~30）      ← 被夹取
✅ 已关机 · 制冷 · 24℃ · 自动风（当前关机，开机后生效）  ← 关机状态下改设定
✅ 已开机 · 制冷 · 24℃ · 自动风（设备离线，可能不生效）  ← 设备离线
好，模式没动                                        ← 整句只有否定
现在 开机 · 制冷 · 24℃ · 自动风 · 🟢 在线（更新于 12:03:41）  ← 问句
```

多条原因用 `；` 连接，顺序固定为 **夹取 → 否定 → 关机 → 离线**。

**去重**：以 `message_id` 为准（`store.mark_seen`）。飞书可能重投同一条事件，而 `temp_step`
这类相对命令**不幂等**，重放会把温度推两次。注意去重只管重投 —— 用户自己发三遍 `调低两度`
就是三次降温，不是 bug。

**开关**：`TEXT_CONTROL_ENABLED`（默认 `true`）。置 `false` 回到旧行为 —— 只有 `设置卡片`
召唤词有反应，其余文本只记日志。

**文本路径不走卡片的置灰**：卡片在关机时把模式/风速按钮置灰，但文本没有"可点元素"，
所以 `关机状态下发「制冷」` 会被接受并入队，靠回执里的「当前关机，开机后生效」说明后果。

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
| `FEISHU_LOG_LEVEL` | 仅长连接 socket 的日志级别；`DEBUG` 可看到原始 WS 收发包 | `INFO` |
| `DEBOUNCE_MS` | 去抖窗口：此窗口内的连续操作合并成一条 `ac/cmd` | `400` |
| `CONFIRM_TIMEOUT_S` | 超过这么久没收到 `ac/state` → 卡片显示「⚠️ 未确认」（**命令无需改变时会误报**，见 §10 注） | `5` |
| `STALE_AFTER_S` | 在线但静默超过这么久 → 卡片加「状态可能过期」软提示 | `90` |
| `TEXT_CONTROL_ENABLED` | 是否把普通中文短句当命令解析（§5.6）；`false` 则只认 `设置卡片` | `true` |
| `DB_PATH` | SQLite 路径（owner ↔ 卡片 message_id、已处理消息去重） | `ac-bot.sqlite3` |

---

## 8. 部署

本仓库的 [`deploy/mosquitto-ac.conf`](../deploy/mosquitto-ac.conf) 是**已在树莓派上验证通过**的版本：

```ini
# /etc/mosquitto/conf.d/ac.conf
listener 1883 0.0.0.0
allow_anonymous true          # 内网可接受；要求更高则改用密码/ACL
```

> ⚠️ **不要把 `persistence` / `persistence_location` 写进 drop-in**：Ubuntu 自带的
> `/etc/mosquitto/mosquitto.conf` 已经设置了两者，mosquitto 2.0 把重复项当**致命错误**，
> 会直接启动失败（`Error: Duplicate persistence_location value in configuration.`）。
> 这个坑已经踩过一次，详见 `deploy/mosquitto-ac.conf` 顶部的注释。

```ini
# /etc/systemd/system/ac-bot.service
[Unit]
Description=AC Feishu Bot
After=network-online.target mosquitto.service
Wants=mosquitto.service

[Service]
User=pi
WorkingDirectory=/home/dirj/project/AirConditionRemoteServer
EnvironmentFile=/home/dirj/project/AirConditionRemoteServer/.env
ExecStart=/home/dirj/project/AirConditionRemoteServer/.venv/bin/python -m app.main
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

> 上面是 2026-10-12 的设计草案。**真正下发到树莓派的是
> [`deploy/ac-bot.service`](../deploy/ac-bot.service)**：`User=dirj`（这台机器上没有 `pi` 用户）、
> 多了 `StateDirectory=ac-bot` / `Environment=DB_PATH=/var/lib/ac-bot/ac-bot.sqlite3`，
> 以及 `NoNewPrivileges` / `PrivateTmp` / `Nice` 三条加固。检出位置
> **`~/project/AirConditionRemoteServer`**（2026-10-15 从 `/opt/ac-bot` 迁入，
> 见 [`feishu-bringup.md`](feishu-bringup.md) §2）。

---

## 9. 建议的仓库结构

本仓库（`AirConditionRemoteServer`）的根目录结构：

```
AirConditionRemoteServer/
  app/
    main.py            # ✅ 装配 + 事件循环（乐观更新 / 去抖 / 超时）
    config.py          # ✅ pydantic-settings
    mqtt_client.py     # ✅ 连接/重连/订阅/发布 + 回调分发
    state.py           # ✅ AcState / Presence / 夹取 / 合并 / 指纹
    actions.py         # ✅ 回调归一化：按钮 action.value 与下拉 action.option → (a, v)
    cards.py           # ✅ 卡片 JSON 构造（纯函数）
    feishu_client.py   # ✅ 长连接、发卡/更新卡、回调路由
    store.py           # ✅ SQLite: owner ↔ message_id
  tests/
    test_state.py      # ✅ 夹取 / 合并 / 指纹 / presence
    test_cards.py      # ✅ 卡片结构与按钮回调值
    test_actions.py    # ✅ 回调归一化与拒绝
    test_config.py     # ✅ 环境变量契约
    test_store.py      # ✅ 卡片映射
  deploy/
    mosquitto-ac.conf  # ✅ 已在树莓派部署并验证
    ac-bot.service     # ✅ 注意 User=dirj（树莓派上没有 pi 用户）；检出在 ~/project/AirConditionRemoteServer
  docs/
    interface-spec.md  # 本文件
    mqtt-bringup.md    # MQTT 链路验证记录 + 环境事实
    feishu-bringup.md  # 飞书链路验证记录（长连接 / 卡片回调 / 环境标识）
    feishu-setup.md    # 飞书自建应用 —— 建应用清单
  .env.example
  .gitignore
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

> ⚠️ **「5s 无变化 ⇒ ⚠️ 未确认」会误报（2026-10-15 实测）**：当这条命令**不需要改变任何状态**时
> （空调已经在那个状态），`merge_actions()` 合并成空 ⇒ 服务端**根本不发 `ac/cmd`**，也就不会有 `ac/state` 回来；
> 而 `pending` 是在**排队时**就置位的，于是下一个 tick 立刻报「未确认」。所以这条提示只能当**弱提示**，
> **不能当"设备没执行"的判据**。修法：固件对每条命令都回一次状态，或服务端区分「无需改变」。设计文档 §18.12.1。

---

## 11. 验收标准

1. 树莓派重启后，服务自动连上 MQTT 与飞书，卡片自动恢复为**真值**（retained 状态）。
2. 点"制冷"→ 5 秒内空调切换，卡片状态与 `ac/state` 一致。
3. 点"＋"三下 → 温度升 3℃，服务只发一条合并后的 `ac/cmd`。
4. 拔掉 ESP32 电源 → 卡片在一个心跳周期内变 `🔴 离线`。
5. ESP32 断电期间点击按钮 → 卡片提示"未确认/离线"，不崩、不刷屏。
6. 发送非法字段/越界温度 → 服务侧被拒或夹取，绝不发未知字段给固件。
7. `cards.py` / `state.py` 单元测试全绿，无需飞书与 MQTT 环境。
8. 发「开机制冷24度」→ 只有**一行** `ac/cmd`（含 3 个字段），回执列出新状态。
9. 发「太热了」→ **不发** `ac/cmd`，只回一条带方向的建议（设计 D12）。
10. 同一条 `message_id` 重投 → 只执行一次；连发三条不同消息 → 执行三次。

---

## 12. 决策记录

### 已定（v1）

1. **控制入口**：**私聊单用户**。只服务 owner 一个人的私聊，机器人**不拉群**；`OWNER_OPEN_ID` 之外一律忽略。
2. **温度交互**：**`－/＋` 步进 + 下拉选择，两者都放**。步进用于微调，下拉用于 16~30 快捷跳转。
3. **飞书接入**：**自建应用 + 机器人能力 + 长连接**（不暴露公网，无需内网穿透）。
4. **定时 / 场景**：v1 **不做**。
5. **多空调**：v1 **不做**，topic 前缀固定 `ac`。
6. **文本控制**（v1.1）：私聊里发中文短句与点卡片**等价**（解析成同样的 `(action, value)`，进同一个队列），
   规则解析、**不接 LLM**；形容词句（`太热了`）只给建议、**永不执行**；去重以 `message_id` 为准；
   可用 `TEXT_CONTROL_ENABLED=false` 整体关掉。完整设计与 14 条决策见
   [`superpowers/specs/2026-10-13-feishu-text-control-design.md`](superpowers/specs/2026-10-13-feishu-text-control-design.md) §10。

### 待确认（不阻塞 v1 开工）

1. **MQTT 鉴权**：内网 `allow_anonymous` 是否可接受，还是要上账号 / ACL？
2. **飞书自建应用权限**：确认你能创建自建应用并拿到 `App ID/Secret`，并开通长连接所需的事件与权限（接收消息、卡片回调、发送 / 更新消息）。

> 未来扩展路径：群聊 → 把 `OWNER_OPEN_ID` 换成白名单、每 chat 各一张卡；多空调 → topic 前缀改 `ac/<room>`，并在固件配网页同步改 `topic_prefix`。
