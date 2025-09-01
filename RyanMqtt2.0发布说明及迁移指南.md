# 🚀 RyanMqtt 2.0 发布

> **更轻、更快、更安全、更可靠**

## 🌟 核心亮点

| 维度                   | 变更类型                                                     | 核心价值                                                     |
| ---------------------- | :----------------------------------------------------------- | :----------------------------------------------------------- |
| **协议栈**             | **[Paho MQTT Embedded](https://github.com/eclipse-paho/paho.mqtt.embedded-c)** → **[coreMQTT](https://github.com/FreeRTOS/coreMQTT)** | 社区维护活跃、测试覆盖完善，为未来 [**MQTT 5.0**](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html) 支持奠定基础 |
| **运行时安全分析验证** | 使用 **[Sanitizer](https://clang.llvm.org/docs/index.html#sanitizers)** 进行运行时安全分析 | 调试阶段捕获内存越界、Use-after-free、数据竞争、未定义行为、内存泄漏等问题，大幅提升**代码健壮性与安全性** |
| **高质量代码保障**     | 引入 **[clang-tidy](https://clang.llvm.org/extra/clang-tidy/#clang-tidy)** 和 **[Cppcheck](https://cppcheck.sourceforge.io/)** 进行静态分析 | **高质量代码保障**，接近语法级"**零缺陷**"，显著提升可维护性 |
| **AI 辅助开发与审查**  | 使用 **[coderabbitai](https://www.coderabbit.ai)** 和 **[Copilot](https://github.com/features/copilot)** 辅助编码与代码审查 | **AI辅助开发与审查**，持续提升代码质量，构筑安全防线         |
| **内存管理优化**       | 固定缓冲区 → **动态按需分配**                                | 采用**按需动态分配**，降低运行内存占用                       |
| **线程安全强化**       | 更完善的线程安全                                             | 支撑复杂线程应用场景，杜绝竞态风险                           |
| **测试体系升级**       | 新增 7 大专项测试                                            | 覆盖广泛场景，全链路内存泄漏检测，强化稳定性与可靠性         |

## 📈 公共 API 变化

### 新增接口

```c
// 批量订阅/取消订阅
extern RyanMqttError_e RyanMqttSubscribeMany(RyanMqttClient_t *client, int32_t count,
					     RyanMqttSubscribeData_t subscribeManyData[]);
extern RyanMqttError_e RyanMqttUnSubscribeMany(RyanMqttClient_t *client, int32_t count,
					       RyanMqttUnSubscribeData_t unSubscribeManyData[]);

// 带用户数据的发布
extern RyanMqttError_e RyanMqttPublishAndUserData(RyanMqttClient_t *client, char *topic, uint16_t topicLen,
						  char *payload, uint32_t payloadLen, RyanMqttQos_e qos,
						  RyanMqttBool_e retain, void *userData);

// 线程安全的订阅查询,必须仅通过 RyanMqttSafeFreeSubscribeResources 释放。
extern RyanMqttError_e RyanMqttGetSubscribeSafe(RyanMqttClient_t *client, RyanMqttMsgHandler_t **msgHandles,
						int32_t *subscribeNum);
extern RyanMqttError_e RyanMqttSafeFreeSubscribeResources(RyanMqttMsgHandler_t *msgHandles, int32_t subscribeNum);

// 订阅数量查询
extern RyanMqttError_e RyanMqttGetSubscribeTotalCount(RyanMqttClient_t *client, int32_t *subscribeTotalCount);

// 获取MqttConfig信息
extern RyanMqttError_e RyanMqttGetConfig(RyanMqttClient_t *client, RyanMqttClientConfig_t **pclientConfig);
// 安全的释放获取的MqttConfig信息
extern RyanMqttError_e RyanMqttFreeConfigFromGet(RyanMqttClientConfig_t *clientConfig);
```

### 新增功能价值

| 接口                     | 用途                  | 优势                                   |
| ------------------------ | --------------------- | -------------------------------------- |
| `SubscribeMany`          | 批量订阅/取消多个主题 | 减少网络往返，提升吞吐效率             |
| `PublishAndUserData`     | 发布消息附带上下文    | 回调中可直接读取用户数据，简化状态管理 |
| `GetSubscribeSafe`       | 安全查询订阅状态      | 内部同步，多线程安全        |
| `GetSubscribeTotalCount` | 获取订阅总数量        | 便于监控与资源调度                     |

## 🔧 平台抽象层优化

### 1. 统一时间接口

```c
uint32_t platformUptimeMs(void);
```

- 跨平台毫秒级运行时间
- 内部处理 32 位回绕，长时间运行稳定

### 2. 网络收发模型简化

- 收发接口改为**单次调用语义**，减少冗余循环与分支
- 返回实际传输字节数，更贴近底层行为
- 异常恢复路径更明确

## 🧪 测试体系全面升级

新增 **7 大类专项测试用例**，覆盖从基础功能到极限压力场景的全流程验证：

| 测试类别                                                  | 测试目标                          |
| --------------------------------------------------------- | --------------------------------- |
| 1. 客户端销毁压力测试                                     | 验证资源释放的幂等性与完整性      |
| 2. 心跳与超时处理                                         | Keep-Alive、PINGREQ/RESP 机制验证 |
| 3. 消息链路完整性（QoS 0/1/2）                            | QoS 0/1/2 消息端到端可靠性验证    |
| 4. 自动/手动重连机制                                      | 状态机正确性与连接恢复能力        |
| 5. 批量/重复订阅一致性                                    | 订阅表一致性、内存安全、去重逻辑  |
| 6. 多客户端高并发                                         | 20+ 客户端并发运行稳定性          |
| 7. 单客户端多线程共享（20 线程 × 各 1000 条 QoS1/2 消息） | 锁机制、数据一致性、竞态防护      |

#### 📊 测试覆盖范围

- **基础连接**：100 次循环连接/断开，混合 QoS 消息与订阅
- **消息流控**：连续发送QoS0/1/2等级 1000 条消息，混合 QoS 压力测试
- **订阅管理**：批量、大量、重复、混合 QoS 订阅与取消
- **并发压测:**
  - 单客户端 20 线程 × 各 1000 条 QoS1/2 消息
  - 多客户端 20 并发实例进行双向发布/订阅
- **可靠性**：长连接、弱网、Keep-Alive、重连机制验证
- **资源安全**：全链路内存泄漏、句柄泄漏检测

## 📊 代码质量与规范

#### ✅ 工具链全面集成

| 工具                                                         | 用途                                                       |
| ------------------------------------------------------------ | ---------------------------------------------------------- |
| **[Sanitizer](https://clang.llvm.org/docs/index.html#sanitizers)** | 运行时捕获内存与线程安全问题                               |
| **[clang-tidy](https://clang.llvm.org/extra/clang-tidy/#clang-tidy)** | 静态分析潜在缺陷（空指针、资源泄漏等）                     |
| **[Cppcheck](https://cppcheck.sourceforge.io/)**             | 深度扫描内存与资源问题                                     |
| **[ClangFormat](https://clang.llvm.org/docs/ClangFormat.html)** | 统一代码风格                                               |
| **编译器警告**                                               | `-Wall -Wextra -Weffc++ -Weverything` 等编译时检测全部打开 |

#### ✅ 检查重点覆盖

- 内存安全：杜绝泄漏、越界、悬空指针
- 性能优化：减少冗余拷贝与低效算法
- 可读性：命名规范、注释完整、逻辑清晰

> ✅ **成果**：实现接近语法级"**零缺陷**"，长期维护成本大幅降低

## 🔒 安全性与可靠性全面提升

- 公共 API 全面审查
- 核心数据结构访问均受同步机制保护
- 动态内存策略更健壮，异常路径也能安全释放
- 输入验证与边界检查更严格，防御性编程增强
- 网络异常处理更完善，弱网环境下连接恢复更快
- 错误恢复路径更稳健，超时策略更精准

## 🎉 总结

本次 RyanMqtt 2.0 是一次具有里程碑意义的升级：

1. **技术栈现代化**：迁移至 **[coreMQTT](https://github.com/FreeRTOS/coreMQTT)**，拥抱更活跃的生态与未来扩展能力

2. **架构更清晰**：模块高度解耦，抽象设计更合理

3. **质量更可信**：7 类专项测试 + 静态分析 + AI 审查构筑强大防线

4. **性能再优化**：更低资源占用、更高吞吐效率

5. **维护更轻松**：统一代码规范、平台抽象降低长期开发成本

   

# 🔄 从 1.x 迁移至 2.x

> RyanMqtt 2.0 在设计上尽量减少破坏性变更。升级过程极为简单：
>

从 **V1.x** 升级到 **V2.x** 时，仅需在 `RyanMqttClientConfig_t` 结构体中**移除以下四个已废弃字段**：

```c
char *recvBuffer;        // MQTT 接收缓冲区（已废弃）
char *sendBuffer;        // MQTT 发送缓冲区（已废弃）
uint32_t recvBufferSize; // 接收缓冲区大小（已废弃）
uint32_t sendBufferSize; // 发送缓冲区大小（已废弃）
```

受影响的接口只有一个：

```c
RyanMqttError_e RyanMqttSetConfig(RyanMqttClient_t *client, RyanMqttClientConfig_t *clientConfig);
```

> 其余 API 与配置保持兼容，无需额外修改即可完成迁移。