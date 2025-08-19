# 🚀 RyanMqtt 2.0 发布

## 📦 升级路线图

| 维度         | 变更类型                                                     | 核心价值                                                     |
| ------------ | :----------------------------------------------------------- | :----------------------------------------------------------- |
| **协议栈**   | **[Paho MQTT Embedded](https://github.com/eclipse-paho/paho.mqtt.embedded-c)** → **[coreMQTT](https://github.com/FreeRTOS/coreMQTT)** | 社区维护活跃、测试覆盖完善，为 [**MQTT 5.0**](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html) 提供可扩展性基础 |
| **代码规范** | 引入 **[clang-tidy](https://clang.llvm.org/extra/clang-tidy/#clang-tidy)** 和 **[Cppcheck](https://cppcheck.sourceforge.io/)** 静态代码分析 | 接近语法级**`零缺陷`**，显著提升可维护性                     |
| **代码审查** | 使用 **[coderabbitai](https://www.coderabbit.ai)** 和 **[Copilot](https://github.com/features/copilot)** 进行代码审查 | 持续提升代码质量，构筑安全防线                               |
| **内存管理** | 静态缓冲区 → 动态分配                                        | 按需申请，降低内存占用                                       |
| **线程模型** | 更完善的线程安全                                             | 支撑复杂线程应用场景，杜绝竞态风险                           |
| **测试体系** | 新增 7 大专项测试                                            | 覆盖广泛场景，强化稳定性与可靠性                             |

## 🏗 架构与底层能力变更

### 1. MQTT 协议栈升级

- 从**[Paho MQTT Embedded](https://github.com/eclipse-paho/paho.mqtt.embedded-c)** 到 **[coreMQTT](https://github.com/FreeRTOS/coreMQTT)**
- **[coreMQTT](https://github.com/FreeRTOS/coreMQTT)** 社区活跃度高、维护频繁、测试覆盖更全面
- 更灵活的缓冲区管理策略
- 为 [**MQTT 5.0**](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html) 扩展奠定实现基础

### 2. 内存管理更改为动态缓冲区

- 移除固定 `recvBuffer` / `sendBuffer` 配置
- 采用**按需动态分配**，降低运行内存占用
- 增强边界检查与输入验证
- 异常路径与析构流程保证**零资源泄漏**

### 3. 线程安全强化

- 全面审查公共 API 与共享资源访问路径
- 精细化控制锁粒度，消除竞态并优化性能
- 针对高并发、多线程、多客户端场景进行专项验证

## 📈 公共 API 变化

### 新增接口

```
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
```

### 新增功能价值

| 接口                     | 用途                  | 优势                                   |
| ------------------------ | --------------------- | -------------------------------------- |
| `SubscribeMany`          | 批量订阅/取消多个主题 | 减少网络往返，提升吞吐效率             |
| `PublishAndUserData`     | 发布消息附带上下文    | 回调中可直接读取用户数据，简化状态管理 |
| `GetSubscribeSafe`       | 安全查询订阅状态      | 多线程场景下无需调用方加锁（内部已同步）                  |
| `GetSubscribeTotalCount` | 获取订阅总数量        | 便于监控与资源调度                     |

## 🔧 平台抽象层优化

### 1. 统一系统时间接口

```
uint32_t platformUptimeMs(void); // 跨平台获取系统启动毫秒数（内部已避免 32 位回绕）
```

### 2. 网络收发模型简化

- 收发接口改为**单次调用语义**，减少冗余循环与分支
- 返回值由“错误码”调整为“实际传输字节数”，更贴近底层行为
- 错误码分类更细化，异常恢复路径更明确可控

## 🧪 测试体系全面升级

新增 **7 大类专项测试用例**，覆盖从基础功能到极限压力场景的全流程验证：

1. **客户端销毁压力测试**：验证资源释放幂等性与完整性
2. **心跳与超时处理**：Keep-Alive 场景
3. **消息链路测试**：QoS 0/1/2 全等级消息完整性验证
4. **自动/手动重连验证**：确保状态机正确性与连接恢复能力
5. **批量/重复订阅测试**：订阅表一致性与内存安全
6. **多客户端高并发测试**：验证在多线程环境下的稳定性
7. **单客户端多线程共享测试**：锁机制及数据一致性验证

**覆盖范围**：

- **基础连接**：100 次循环连接/断开，涵盖 QoS0/1/2 消息和订阅，直接销毁
- **消息流控**：QoS0/1/2等级 1000 条消息，混合 QoS 压力测试
- **订阅管理**：批量、大量、重复、混合 QoS 订阅和取消订阅场景
- **并发压测:**
  - 单客户端 20 线程 × 各 1000 条 QoS1/2 消息
  - 多客户端 20 并发实例进行双向发布/订阅
- **可靠性**：长连接、弱网、Keep-Alive、重连机制验证
- **资源安全**：全链路内存与句柄泄漏检测

## 📊 代码质量与规范体系

### 工具链全面集成

- **clang-format**：遵循 LLVM 风格统一代码格式
- **clang-tidy**：静态分析潜在缺陷
- **cppcheck**：深度扫描内存与资源问题
- **编译警告全开**：`-Wall -Wextra -Weffc++ -Weverything`

### 检查覆盖重点

- ✅ 内存安全（防止泄漏、越界、悬空指针）
- ✅ 性能优化（减少低效算法与冗余拷贝）
- ✅ 代码可读性（命名规范、注释完整性）

> **成果**：语法与逻辑缺陷接近零，长期维护成本显著降低

## 🔒 安全性与可靠性提升

- 公共 API 全面通过线程安全审查
- 核心数据结构访问均具备同步保护

- 动态内存策略更健壮
- 输入验证与边界检查更严格
- 析构路径保证**资源 100% 释放**
- 改进网络异常处理与状态管理，提升弱网适应性
- 错误恢复路径更稳健，超时策略更精准

## 🎉 总结

本次 RyanMqtt 2.0 是一次具有里程碑意义的升级：

1. **技术栈现代化**：迁移至 **[coreMQTT](https://github.com/FreeRTOS/coreMQTT)**，拥抱未来

2. **架构更清晰**：模块高度解耦，抽象设计更合理

3. **质量更可信**：7 类专项测试 、 静态分析、AI审查构筑强大防线

4. **性能再优化**：动态内存与批量操作优化吞吐与响应

5. **维护更轻松**：统一规范与平台抽象降低长期成本

   

# 🔄 从 1.x 迁移至 2.x

RyanMqtt 在版本演进中尽量减少破坏性变更。 

从 **V1.x** 升级至 **V2.x** 仅需：

**删除** `RyanMqttClientConfig_t` **结构体中的以下四个字段**：

```
char *recvBuffer;        // MQTT 接收缓冲区
char *sendBuffer;        // MQTT 发送缓冲区
uint32_t recvBufferSize; // MQTT 接收缓冲区大小
uint32_t sendBufferSize; // MQTT 发送缓冲区大小
```

受影响的接口仅为：

```
RyanMqttError_e RyanMqttSetConfig(RyanMqttClient_t *client, RyanMqttClientConfig_t *clientConfig);
```