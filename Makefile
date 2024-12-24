# 设置编译器
CC = gcc

# 设置包含路径
CFLAGS += -I common \
          -I pahoMqtt \
          -I mqttclient \
          -I platform/linux \
          -I platform/linux/valloc

# 设置编译选项
CFLAGS += -Wall -Wno-unused-parameter -Wformat=2

# 搜索所有C源文件
SRCS = $(wildcard ./test/*.c)
SRCS += $(wildcard ./common/*.c)
SRCS += $(wildcard ./platform/linux/*.c)
SRCS += $(wildcard ./platform/linux/valloc/*.c)
SRCS += $(wildcard ./pahoMqtt/*.c)
SRCS += $(wildcard ./mqttclient/*.c)

# 定义目标文件和输出文件
OBJS = $(patsubst %.c,%.o,$(SRCS))
TARGET = app.o

# 默认目标
all: $(TARGET)

# 链接所有对象文件生成最终二进制文件
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ -lm -lpthread

# 编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理生成的文件
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)