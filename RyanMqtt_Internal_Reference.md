# RyanMqtt 内部实现参考

RyanMqtt 是一个严格遵循 [MQTT 3.1.1](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html) 协议标准的客户端库，专为资源受限的嵌入式设备设计。本参考文档将详细描述 RyanMqtt 的内部实现，包括日志、链表管理、移植指南以及编译配置等内容。

---

## 1. 系统设计与主要特性

RyanMqtt 的设计初衷是为嵌入式系统提供一个稳定可靠的 MQTT 客户端。以下是其主要特性：

- **协议支持**：严格遵循 MQTT 3.1.1 协议。
- **运行时安全验证**：
  - 使用 [Sanitizer](https://clang.llvm.org/docs/index.html#sanitizers) 捕获内存越界、数据竞争等问题。
- **代码质量保障**：
  - 引入 [clang-tidy](https://clang.llvm.org/extra/clang-tidy/#clang-tidy)、[Cppcheck](https://cppcheck.sourceforge.io/) 进行静态分析。
- **内存优化**：
  - 支持动态按需分配内存，优化资源使用。

---

## 2. 核心组件

### 2.1 日志系统

日志模块位于 `common/RyanMqttLog.c` 和 `common/RyanMqttLog.h` 中。

#### 实现亮点：
1. **日志级别控制**
   - 提供多种日志级别（Debug、Info、Warning、Error）。
2. **颜色化输出**
   - 支持终端颜色输出，便于调试。

#### 示例代码：
```c
#define RyanMqttLogLevel (RyanMqttLogLevelDebug)
RyanMqttLog_d("Debug 信息");
RyanMqttLog_e("Error 信息");
```

### 2.2 链表管理

链表模块位于 `common/RyanMqttList.c` 和 `common/RyanMqttList.h` 中。

#### 实现亮点：
1. **灵活的节点操作**
   - 支持链表的正向、反向遍历。
2. **安全遍历**
   - 提供支持遍历中删除节点的安全遍历方式。

#### 示例接口：
```c
// 初始化链表
RyanMqttListInit(&list);

// 添加节点
RyanMqttListAdd(&node, &list);
```

---

## 3. 编译与配置

RyanMqtt 提供多种构建工具支持，包括 Makefile、xmake 和 SCons：

### 3.1 Makefile

- 定义目标文件和输出文件：
  ```makefile
  TARGET = app.o
  $(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ -lm -lpthread
  ```

### 3.2 xmake

- 启用优化和警告：
  ```lua
  set_optimize("aggressive")
  set_warnings("everything")
  ```

- 启用运行时错误检测：
  ```lua
  add_ldflags("-fsanitize=address", "-fsanitize=leak")
  ```

### 3.3 SCons

- 定义依赖关系：
  ```python
  group = DefineGroup('RyanMqtt', src, depend=['PKG_USING_RYANMQTT'], CPPPATH=path)
  ```

---

## 4. 迁移指南

RyanMqtt 2.0 发布说明中提供了详细的迁移指南（详见 `RyanMqtt2.0发布说明及迁移指南.md`）。

- **协议栈迁移**：从 Paho MQTT Embedded 切换到 coreMQTT。
- **API 变化**：新增批量订阅/取消订阅接口。

#### 新增接口：
```c
extern RyanMqttError_e RyanMqttSubscribeMany(RyanMqttClient_t *client, int32_t count,
          RyanMqttSubscribeData_t subscribeManyData[]);
```

---

## 5. 相关链接

- [RyanMqtt 仓库](https://github.com/Ryan-CW-Code/RyanMqtt)
- [MQTT 3.1.1 协议标准](https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html)
- [RT-Thread 社区](https://club.rt-thread.org/index.html)