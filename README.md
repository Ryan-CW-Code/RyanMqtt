# RyanMqtt

**使用遇到问题可以提 issue / RT-Thread 社区提问，谢谢。**

[RT-Thread-RyanMqtt使用介绍和示例代码（一）RT-Thread问答社区 - RT-Thread](https://club.rt-thread.org/ask/article/51a25ba90fc5e1b5.html)

[RT-Thread-RyanMqtt QOS质量测试（二）RT-Thread问答社区 - RT-Thread](https://club.rt-thread.org/ask/article/e95c5b9390c53cf3.html)

[RT-Thread-RyanMqtt 移植指南（三）RT-Thread问答社区 - RT-Thread](https://club.rt-thread.org/ask/article/611b7a947f7221cf.html)

[keil使用 RyanMQTT 编译错误（中文编码问题）RT-Thread问答社区 - RT-Thread](https://club.rt-thread.org/ask/question/7269a82662b6cf31.html)


### 1、介绍

RyanMqtt 实现了 [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html) 协议的客户端。此库针对资源受限的嵌入式设备进行了优化。

初衷：在使用[RT-Thread](https://github.com/RT-Thread/rt-thread)时，没有非常合适的 mqtt 客户端。项目中 mqtt 又是非常核心的功能。参考 MQTT3.1.1 标准和项目需求设计的 mqtt 客户端，它拥有以下特点。

- **严格遵循 [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html) 协议标准实现**
- **代码规范**  引入 **[clang-tidy](https://clang.llvm.org/extra/clang-tidy/#clang-tidy)** 和 **[Cppcheck](https://cppcheck.sourceforge.io/)** 静态代码分析， 接近语法级“零缺陷”，提升可维护性
- 使用  **[coderabbitai](https://www.coderabbit.ai)** 和 **[Copilot](https://github.com/features/copilot)** 进行代码审查，持续提升代码质量，构筑安全防线
- 支持多客户端
- **稳定的全 QOS 等级实现消息实现**。**用户可控的消息丢弃机制**，避免 Qos2 / Qos1 消息无限堆积重发消耗的内存空间
- **完整的 MQTT 主题通配符支持，“/”、“#”、“+”、“$”**
- 支持批量订阅 / 取消订阅
- 可选择的 keepalive、reconnect、lwt、session 等
- 客户端多功能参数配置，丰富的用户可选的事件回调，满足实际项目的绝大部分需求（欢迎提需求）
- 优化过的并发能力，**无等待的连续 20000 条 RyanMqttQos2 消息稳定发送和接收无一丢包**(测试环境为linux，实际情况会受到单片机内存大小和网络硬件的收发能力的影响)
- **复杂线程环境下稳定运行**，已在公司多个项目使用
- 跨平台，只需实现少量的平台接口即可
- 资源占用少，依赖少
- 没有内置 TLS 支持，用户可以在platform层实现 TLS（使用 TLS 的项目也不会只有 mqtt 使用，用户自己实现可以防止 TLS 模块间冲突）
- **不支持裸机平台**，裸机想要稳定的 [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html) 实现可以参考[coreMQTT](https://github.com/FreeRTOS/coreMQTT)

### 2、设计

RyanMqtt 设计时参考了[mqttclient](https://github.com/jiejieTop/mqttclient)、[esp-mqtt](https://github.com/espressif/esp-mqtt)、[coreMQTT](https://github.com/FreeRTOS/coreMQTT)。

![组](docs/assert/README.assert/%E7%BB%84.png)

- **平台兼容层**封装不同操作系统内核接口，方便实现跨平台
- **核心库**基于 [coreMQTT](https://github.com/FreeRTOS/coreMQTT) 实现 MQTT 报文序列化与处理
- **核心线程**是每个客户端必须创建的线程，统一处理客户端所有的操作，比如 mqtt 状态机、事件回调、心跳保活、消息解析、消息超时处理、消息重发等
- **系统服务管理模块**提供 RyanMqtt 实现功能的工具，包含 session 状态、事件处理、通配符匹配、消息链表等
- **用户应用模块**提供给用户调用的丰富的接口，包含 RyanMqtt 客户端申请 / 销毁、mqtt 多参数配置、事件注册 / 注销、QOS 消息丢弃、 mqtt 连接 / 停止 / 重连 / 发布 / 订阅 / 取消订阅等

### 3、平台接口

_RyanMqtt 库希望应用程序为以下接口提供实现：_

#### system 接口

_RyanMqtt 需要 RTOS 支持，必须实现如下接口才可以保证 mqtt 客户端的正常运行_

| 函数名称                | 函数简介       |
| ----------------------- | -------------- |
| platformMemoryMalloc    | 申请内存       |
| platformMemoryFree      | 释放已申请内存 |
| platformDelay           | 毫秒延时       |
| platformPrint           | 打印字符串     |
| platformThreadInit      | 初始化线程     |
| platformThreadDestroy   | 销毁线程       |
| platformThreadStart     | 开启线程       |
| platformThreadStop      | 挂起线程       |
| platformMutexInit       | 初始化互斥锁   |
| platformMutexDestroy    | 销毁互斥锁     |
| platformMutexLock       | 获取互斥锁     |
| platformMutexUnLock     | 释放互斥锁     |
| platformCriticalInit    | 初始化临界区   |
| platformCriticalDestroy | 销毁临界区     |
| platformCriticalEnter   | 进入临界区     |
| platformCriticalExit    | 退出临界区     |

#### network 接口

_RyanMqtt 依赖于底层传输接口 API，必须实现该接口 API 才能在网络上发送和接收数据包_

_MQTT 协议要求基础传输层能够提供有序的、可靠的、双向传输（从客户端到服务端 和从服务端到客户端）的字节流_

| 函数名称                 | 函数简介                 |
| ------------------------ | ------------------------ |
| platformNetworkInit      | 网络资源初始化           |
| platformNetworkDestroy   | 网络资源销毁             |
| platformNetworkConnect   | 根据 ip 和端口连接服务器 |
| platformNetworkRecvAsync | 非阻塞接收数据           |
| platformNetworkSendAsync | 非阻塞发送数据           |
| platformNetworkClose     | 断开 mqtt 服务器连接     |

#### time 接口

_RyanMqtt 依靠函数生成毫秒时间戳，用于计算持续时间和超时，内部已经做了数值溢出处理_

| 函数名称         | 函数简介                 |
| ---------------- | ------------------------ |
| platformUptimeMs | 自系统启动以来 ms 时间戳 |

### 4、示例

#### RT-Thread 平台

- 接口示例请参考 platform/rtthread 文件夹

- RyanMqtt 使用示例请参考 example 文件夹

- 需要使能 SAL，示例使用 socket 接口进行数据收发。

- 需要 MSH 组件，示例默认挂载到 MSH 组件

  **详细使用请参考 example，提供了一些测试接口和使用范例**

  ![image-20230927112803101](docs/assert/README.assert/image-20230927112803101.png)

#### 合宙openLuat平台

- 接口示例请参考 platform/openLuat 文件夹，请根据平台差异进行修改

#### 移远QuecOpen平台

- 接口示例请参考 platform/quecopen 文件夹，请根据平台差异进行修改

#### 中移OpenCPU平台

- 接口示例请参考 platform/openCPU文件夹，请根据平台差异进行修改

#### Linux平台

- 接口示例请参考 platform/linux 文件夹，请根据平台差异进行修改

### 5、依赖

暂无

### 6、声明

- 请勿将此库 QOS 消息等级用于**支付等可能造成重大损失**的场景，如需使用请自行**深度评估后使用！！！作者不对使用此库造成的任何损失负责。不要只依靠 qos2 来保证安全！！！**。（尽管此库 RyanMqttQos2 消息等级经过很多测试，但是异步组件由于诸多因素例如波动非常大的网络甚至无法建立稳定连接、mqtt 服务端的策略配置等，无法做到绝对的准确性。需要用户用应用层做到数据的最终一致性。）