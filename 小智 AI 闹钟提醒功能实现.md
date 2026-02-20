# 小智 AI + 闹钟提醒 + 定时任务, 设备端MCP实现, 想象空间巨大

**原创** | AI码上来 | 📅 2025-09-13 09:24:55 发布 | 阅读量 1.2K | 收藏 17 | 点赞数 31 | CC 4.0 BY-SA 版权

分类专栏: AI实践 | 文章标签: 人工智能

---

**北京朝阳AI社区** 文章已被社区收录
[Join Community button]

**AI实战** 专栏收录该内容 | 111 篇文章 | [Subscribe button]

---

从某个版本开始, 小智 **AI** 从 **IoT** 协议切换到 **MCP** 协议!

可玩性更高, 想象空间更大了~

不管是 IoT 协议, 还是 **MCP** 协议, 本质上都是给小智扩展更多外部能力, 只不过 MCP 生态越发壮大, 是大势所趋。

问题来了, 有了 MCP, 都能干点啥呢?

目前, 社区中对 **定时闹钟提醒** 的呼声很高, 后台也接到了很多朋友的咨询。

今天分享: **如何通过 MCP 为小智 AI 接入 定时闹钟提醒 能力?**

先看效果:

> 当闹钟触发时: 云端下发语音提醒, 随后循环播放本地音乐, 直到用户唤醒。

https://www.bilibili.com/video/BV1sfHVzjE4o/

---

## 全文目录:

* **1. 需求分析**
    * 具体功能
    * 提醒方式
    * 断电续传
* **2. 实现思路**
    * 倒计时功能
    * 定时闹钟功能
    * 重复闹钟功能
    * 删除闹钟
    * 持久化存储

---

## 1. 需求分析

**定时闹钟提醒** 其实可以拆解为三类需求:

* **倒计时功能**: 类似 **番茄学习法**, 比如工作 25min 后提醒我休息;
* **定时闹钟功能**: 比如 **晚上 8 点** 提醒我睡觉;
* **重复闹钟功能**: 比如 **每天早 8 点** 提醒我起床

### 以什么方式提醒呢?

* 屏幕显示;
* 播放本地音乐, 直到唤醒;
* 云端下发语音通知;

### 如果设备断电了, 怎么确保 **定时闹钟提醒** 依然能准时触发?

* **云端存储**: 每个 **定时闹钟提醒** 在云端备份, 设备通电后, 再次同步;
* **设备端存储**: **nvs** **持久化存储**, 无需依赖云端。

---

## 2. 实现思路

**定时闹钟提醒** 其实非常考验 LLM 的 **语义理解** 和工具调用能力。

为了提高成功率, 现阶段还是不能对 LLM 抱有太高期望, 因此任务定义越明确、越简洁, 自然效果越好。

最简单, 就是 LLM 在工具调用时, 给设备端下发一个 **delay**, 设备端新增一个 **定时器**, 到点自动触发!

至于触发后, 要完成什么动作, 那还是你说了算!

循环播放本地音乐, 当然 OK!
向云端发请求, 然后云端下发语音提醒, 当然也 OK, 只是相对复杂一些。

### 2.1 倒计时功能

首先, 我们需要定义一个闹钟的结构体:

```cpp
struct Alarm {
    time_t time; // 开始提醒时间
    std::string name; // 提醒内容
};
```

当云端下发了 **delay** 参数, 更改闹钟触发的具体时间:

```cpp
time_t now = time(NULL);
alarm.time = now + delay;
```

当需要调度闹钟, 再次计算 **delay**:

```cpp
CreateAndStartTimer(alarm->time - now);
```

这种方式对 **倒计时任务** 完全没问题, 但是对 **定时闹钟** 完全就抓瞎了, 比如:

> User (2025-09-07 08:59:08): 7点钟提醒我吃早餐。
>
> AI (2025-09-07 08:59:08): `self.alarm.set_alarm({"time":2592000,"name":"吃早餐"})`
>
> 以上内容由 AI (qwen) 生成

你看, 给了个完全错误的 **delay**!

再次逗醒它, 给算成 10 小时之后的闹钟了:

> User (2025-09-07 08:59:23): 我是说 10点钟提醒我吃早餐。
>
> AI (2025-09-07 08:59:23): `self.alarm.del_alarm({"name":"吃早餐"})`
>
> 以上内容由 AI (qwen) 生成
>
> Tool (2025-09-07 08:59:24): `{"result":{"success":true,"message":"Alarm delete successfully"}}`
>
> AI (2025-09-07 08:59:24): `self.alarm.set_alarm({"time":36000,"name":"吃早餐"})`

必须先提醒它当前时间, 才能算正确:

> User (2025-09-07 09:01:08): 现在是几点啊?
>
> AI (2025-09-07 09:01:08): 😎 现在是 2025 年 9 月 7 日早上九点零一分, 星期天! 白露节气到啦~秋意浓浓, 记得别穿太少喔!
>
> 以上内容由 AI (qwen) 生成
>
> User (2025-09-07 09:01:29): 我让你今天 10点钟提醒我吃早餐。
>
> AI (2025-09-07 09:01:29): `self.alarm.set_alarm({"time":3540,"name":"eat_breakfast"})`

显然这么做, 用户体验太差, 必须改造~

### 2.2 定时闹钟功能

除了 **delay** 参数, 可以再加两个参数:

1.  `"hour": int`, 比如早上 7 点 the hour can be 7"
2.  `"minute": int`, 比如早上 7:00, the minute can be 0"

那么, 设置闹钟时, 就可以根据下发参数进行判断:

```cpp
if ((hour >= 0 && hour < 24) || (minute >= 0 && minute < 60)) {
    // 计算距离最近的指定hour和minute的开始时间
    struct tm* target_time = localtime(&now);
    // 设置目标时间的时和分
    target_time->tm_hour = (hour >= 0 && hour < 24) ? hour : 0;
    target_time->tm_min = (minute >= 0 && minute < 60) ? minute : 0;
    // 转换成 time_t
    time_t alarm_time = mktime(target_time);
    alarm.time = alarm_time;
} else {
    alarm.time = now + delay;
}
```

> User (2025-09-11 20:25:23): 今天晚上 10点提醒我睡觉。
>
> AI (2025-09-11 20:25:24): `self.alarm.set({"delay":5400,"hour":22,"minute":-1,"repeat":1,"interval":0,"name":"睡觉"})`

你看, 虽然给出了错误的 **delay** 参数, 不过我们可以根据 **hour** 参数进行设置了~

这样, 定时闹钟就没问题了~

如果还要重复提醒呢?

### 2.3 重复闹钟功能

继续加两参数:

1.  `"repeat": int`, 重复次数, 如果没说默认 1 次, 否则为具体的次数, 最多重复 10000 次"
2.  `"interval": int`, 间隔时间, 单位为秒; repeat > 0 时生效, 比如每小时提醒我, interval can be 3600"

这时, 完整的闹钟结构体定义如下:

```cpp
struct Alarm {
    time_t time; // 开始提醒时间
    int repeat; // 重复次数, 0 表示不重复, 大于 0 表示重复次数
    int interval; // 提醒间隔 (秒)
    std::string name; // 提醒内容
};
```

当清除闹钟时, 需要根据 **repeat** 参数判断:

```cpp
it->repeat--; // 次数减去 1
if (it->repeat > 0) {
    it->time += it->interval; // 设定下次闹钟时间
    ++it;
} else {
    it = alarms_erase(it); // 删除过期的闹钟
}
```

> User (2025-09-08 10:20:38): 每天 8点叫我起床。
>
> AI (2025-09-08 10:20:38): `self.alarm.set({"delay":28800,"hour":8,"minute":-1,"repeat":10000,"interval":86400,"name":"起床"})`

你看, LLM 成功给出了 **interval** 参数, 86400s 刚好是 24h。

如果还要删除闹钟呢?

### 2.4 删除闹钟

怎么判断要删除的是哪一个闹钟呢?

用户最无感的, 自然是根据关键词删除:

> User (2025-09-07 19:59:58): 出发的提醒。
>
> AI (2025-09-07 19:59:58): `self.alarm.del({"keyword":"出发"})`
>
> Tool (2025-09-07 19:59:58): `{"result":{"success":true,"message":"Alarm delete successfully"}}`

但问题是, 容易抓不准关键词:

> User (2025-09-07 19:59:33): 删除触发提醒。
>
> AI (2025-09-07 19:59:33): `self.alarm.del({"keyword":"触发"})`
>
> Tool (2025-09-07 19:59:33): `{"result":{"success":false,"message":"Alarm not found"}}`

怎么解决?

给每个闹钟一个 ID, 让 LLM 理解所有闹钟的上下文, 然后根据 ID 删除:

> User (2025-09-12 08:46:47): 帮我把出发提醒删了吧。
>
> AI (2025-09-12 08:46:48): `self.alarm.queryall({})`
>
> Tool (2025-09-12 08:46:48): `{"type":"text","text":"{\"success\":true,\"alarms\":[{\"id\":0,\"name\":\"起床\",\"time\":\"2025-09-13 08:00:00\",\"repeat\":9996,\"interval\":86400},{\"id\":1,\"name\":\"出发提醒\",\"time\":\"2025-09-12 10:00:00\",\"repeat\":1,\"interval\":0},{\"id\":2,\"name\":\"吃饭提醒\",\"time\":\"2025-09-12 12:00:00\",\"repeat\":1,\"interval\":0}]}"}`
>
> AI (2025-09-12 08:46:48): `self.alarm.del({"id":1})`

### 2.5 持久化存储

小智客户端中, 所有需要持久化存储的数据, 采用 **NVS** 存储, 功能封装在 **Settings** 类中, 它允许以键值对的形式存储和读取数据, 如字符串、整数和布尔值。

为此, 我们只需创建一个实例 (对象) :

```cpp
Settings settings_("alarm_clock", true);
```

传入两个参数:

* `alarm_clock`: 这是命名空间, 用于在 NVS 中标识存储区域
* `true`: 表示该实例具有读写权限

> **注**: ESP-IDF 中 NVS 键名的最大长度是 15 个字符 (包括结尾的空字符)

然后, 设置闹钟时, 同时写入 NVS:

```cpp
settings_.SetString("alarm_" + std::to_string(i), alarm.name);
settings_.SetInt("alarm_time_" + std::to_string(i), alarm.time);
settings_.SetInt("alarm_rpt_" + std::to_string(i), repeat);
settings_.SetInt("alarm_itv_" + std::to_string(i), interval);
```

设备重启后, 从 NVS 读出来:

```cpp
alarm.name = settings_.GetString("alarm_" + std::to_string(i));
alarm.time = settings_.GetInt("alarm_time_" + std::to_string(i));
alarm.repeat = settings_.GetInt("alarm_rpt_" + std::to_string(i));
alarm.interval = settings_.GetInt("alarm_itv_" + std::to_string(i));
```

### 2.6 效果测试

**定时提醒:**

```
>> 2 分钟后提醒我出发。
I (30764) Application: >> 2分钟后提醒我出发。
I (31454) Application: << % self.alarm.set...
I (31464) MCP: SetAlarm delay 120 hour -1 minute -1 repeat 1 interval 0 name 出发
I (31464) AlarmManager: 当前时间戳: 1757326600 (2025-09-08 10:16:40)
I (31484) AlarmManager: Alarm 0 出发 set at 2025-09-08 10:18:40 repeat 1 interval 0
I (31484) AlarmManager: === Alarm Settings Debug Info ===
I (31484) AlarmManager: Alarm 0: name=出发, time=1757326720, repeat=1, interval=0
I (31504) AlarmManager: Next Alarm 0 出发 set at 2025-09-08 10:18:40 repeat 1 interval 0
I (31504) AlarmManager: Start timer, trigger in 120 sec
I (31974) Application: << 两分钟后出发提醒已设定,时间到会叫你喔~
```

**重复闹钟:**

```
>> 每天8点叫我起床。
I (268624) Application: >> 每天8点叫我起床。
I (269384) Application: << % self.alarm.set...
I (269384) MCP: SetAlarm delay 28800 hour 8 minute -1 repeat 10000 interval 86400 name 起床
I (269384) AlarmManager: 当前时间戳: 1757326838 (2025-09-08 10:20:38)
I (269484) AlarmManager: Alarm 0 起床 set at 2025-09-09 08:00:00 repeat 10000 interval 86400
I (269484) AlarmManager: === Alarm Settings Debug Info ===
I (269484) AlarmManager: Alarm 0: name=起床, time=1757404800, repeat=10000, interval=86400
I (269424) AlarmManager: Next Alarm 0 起床 set at 2025-09-09 08:00:00 repeat 10000 interval 86400
I (269424) AlarmManager: Start timer, trigger in 77962 sec
I (269944) Application: << 好喔, 每天早上八点叫你起床, 已经帮你设好啦~
I (274334) Application: << 不用担心赖床喔~
```

**查询闹钟提醒:**

```
现在有哪些闹钟提醒?
I (61994) Application: >> 现在有哪些闹钟提醒?
I (62314) Application: << % self.alarm.queryall...
I (62324) MCP: Query All Alarm
I (62844) Application: << 目前有一个提醒: 10:18:40 出发 。
```

**删除闹钟提醒:**

```
>> 删除这个提醒吧
I (84104) Application: >> 删除这个提醒吧
I (84104) Application: STATE: speaking
I (84424) Application: << % self.alarm.del...
I (84434) MCP: DelAlarm
I (84434) AlarmManager: Alarm 0 出发 deleted
I (84444) AlarmManager: Alarm 0 出发 deleted
I (84444) AlarmManager: No more alarms
I (84844) Application: << “出发”提醒已删除, 清净啦~
```

**断电重启:**

```
I (11194) Application: Server time synchronized, start alarm manager
I (11194) AlarmManager: AlarmManager init
I (11194) AlarmManager: Alarm 0 起床 add again at 2025-09-09 08:00:00
I (11194) AlarmManager: Start timer, trigger in 77452 sec
```

**查询所有闹钟:**

> Tool (2025-09-12 09:08:34): `{"type":"text","text":"{\"success\":true,\"alarms\":[{\"id\":0,\"name\":\"起床\",\"time\":\"2025-09-13 08:00:00\",\"repeat\":9996,\"interval\":86400},{\"id\":2,\"name\":\"吃饭提醒\",\"time\":\"2025-09-12 12:00:00\",\"repeat\":1,\"interval\":0},{\"id\":1,\"name\":\"出发\",\"time\":\"2025-09-12 09:30:00\",\"repeat\":1,\"interval\":0}]}"}`
>
> AI (2025-09-12 09:08:35):
> 😃 目前有三个闹钟喔:
>
> 出发 - 今天 09:30
> 吃饭提醒 - 今天 12:00
> 起床 - 明天 08:00 (每天响)
>
> 要改哪个吗? 😇

## 写在最后

本文分享了 **小智 AI + 闹钟提醒** 的具体实现思路。

如果对你有帮助, 不妨点赞收藏备用。

有了定时触发, 可玩的就不再仅仅是 **闹钟提醒**, 一切可以自动化执行的任务, 都可以交给它:

比如: 每天帮你爬取热点资讯, 整理后发到你微信. 。 。

尽情发挥你的想象吧~