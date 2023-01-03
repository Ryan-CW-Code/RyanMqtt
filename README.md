# RyanMqtt

**使用遇到问题可以提issue / RT-Thread社区提问，谢谢。**

### 1、介绍

RyanMqtt 实现了 MQTT3.1.1 协议的客户端。此库针对资源受限的嵌入式设备进行了优化。

初衷：在使用[RT-Thread](https://github.com/RT-Thread/rt-thread)时，没有非常合适的 mqtt 客户端。项目中 mqtt 又是非常核心的功能。随即参考 MQTT3.1.1 标准和项目需求设计的 mqtt 客户端，它拥有以下特点。

- 严格遵循 MQTT3.1.1 协议标准
- 稳定的全QOS等级实现消息实现，用户可控的消息丢弃，避免 QOS2 / QOS1 消息无限堆积重发消耗的内存空间
- 支持多客户端
- 客户端多功能参数配置，丰富的用户可选的事件回调，满足实际项目的绝大部分需求
- 完整的 MQTT 主题通配符支持，“/”、“#”、“+”、“$”
- 可选择的keepalive、reconnet、lwt、session等
- 资源占用少，依赖少
- 优化过的并发能力，无等待的连续 200 条 QOS2 消息稳定发送和接收(取决于硬件内存大小、也取决于硬件收发能力)
- 跨平台，只需实现少量的平台接口即可
- 没有内置 TLS 支持，用户可以在接口层自己实现 TLS（作者对 TLS 并不熟悉、项目中也暂未使用到、使用TLS的项目也不会只有mqtt使用，用户自己实现也可以防止TLS模块间冲突）
- 不支持裸机平台，裸机想要稳定的 MQTT3.1.1 实现可以参考[coreMQTT](https://github.com/FreeRTOS/coreMQTT)

### 2、设计

RyanMqtt 设计时参考了[mqttclient](https://github.com/jiejieTop/mqttclient)、[esp-mqtt](https://github.com/espressif/esp-mqtt)、[coreMQTT](https://github.com/FreeRTOS/coreMQTT)。

![组](docs/assert/README.assert/%E7%BB%84.png)

- **平台兼容层**封装不同操作系统内核接口，方便实现跨平台
- **核心库**使用著名的paho mqtt库作为mqtt封包库
- **核心线程**是每个客户端必须创建的线程，统一处理客户端所有的操作，比如mqtt状态机、事件回调、心跳保活、消息解析、消息超时处理、消息重发等
- **系统服务管理模块**提供RyanMqtt实现功能的工具，包含session状态、事件处理、通配符匹配、消息链表等
- **用户应用模块**提供给用户调用的丰富的接口，包含RyanMqtt客户端申请 / 销毁、mqtt多参数配置、事件注册 / 注销、QOS消息丢弃、 mqtt连接 / 停止 / 重连 / 发布 / 订阅 / 取消订阅等

### 3、平台接口

*RyanMqtt库希望应用程序为以下接口提供实现：*

#### system接口

*RyanMqtt需要RTOS支持，必须实现如下接口才可以保证mqtt客户端的正常运行*

| 函数名称              | 函数简介            |
| --------------------- | ------------------- |
| platformMemoryMalloc  | 申请内存            |
| platformMemoryFree    | 释放已申请内存      |
| platformDelay         | 毫秒延时            |
| platformThreadInit    | 初始化线程          |
| platformThreadStart   | 开启线程            |
| platformThreadStop    | 挂起线程            |
| platformThreadDestroy | 销毁线程            |
| platformMutexInit     | 初始化互斥锁        |
| platformMutexLock     | 获取互斥锁          |
| platformMutexUnLock   | 释放互斥锁          |
| platformMutexDestroy  | 销毁互斥锁          |
| platformCriticalEnter | 进入临界区 / 关中断 |
| platformCriticalExit  | 退出临界区 / 开中断 |

#### network接口

*RyanMqtt依赖于底层传输接口 API，必须实现该接口 API 才能在网络上发送和接收数据包*

*MQTT 协议要求基础传输层能够提供有序的、可靠的、双向传输（从客户端到服务端 和从服务端到客户端）的字节流*

| 函数名称                 | 函数简介               |
| ------------------------ | ---------------------- |
| platformNetworkConnect   | 根据ip和端口连接服务器 |
| platformNetworkRecvAsync | 非阻塞接收数据         |
| platformNetworkSendAsync | 非阻塞发送数据         |
| platformNetworkClose     | 断开mqtt服务器连接     |

#### time接口

*RyanMqtt依靠函数生成毫秒时间戳，用于计算持续时间和超时，内部已经做了数值溢出处理*

| 函数名称         | 函数简介               |
| ---------------- | ---------------------- |
| platformUptimeMs | 自系统启动以来ms时间戳 |



### 4、示例

RT-Thread 平台

- 接口示例请参考platform/rtthread文件夹
- RyanMqtt使用示例请参考 example 文件夹
- 需要使能 SAL 或者 LWIP，示例使用 socket 实现数据收发。
- 需要MSH组件，示例默认挂载到MSH组件

其余平台暂无示例

### 5、依赖

RT-Thread 内置 ulog 组件，方便的使用 ulog api 来管理 RyanMqtt 打印信息

如果没有使能 ulog 或者非 RT-Thread 平台，用户需要手动修改 RyanMqttLog.h 文件调整打印等级。

### 6、声明

- 请勿将此库QOS消息等级用于**支付等可能造成重大损失**的场景，如需使用请自行**深度评估后使用**，作者不对使用此库造成的任何损失负责。一般也不会选择只是用qos2来保证安全。（尽管此库QOS2消息等级经过很多测试，但是异步组件由于诸多因素例如波动非常大的网络甚至无法建立稳定连接、mqtt服务端的策略配置等，无法做到绝对的实时性，需要用户自己做到数据的最终一致性。）

