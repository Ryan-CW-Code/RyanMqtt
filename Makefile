
CFLAGS_INC = -I common
CFLAGS_INC +=  -I pahoMqtt
CFLAGS_INC +=  -I mqttclient
CFLAGS_INC +=  -I platform/linux
CFLAGS_INC +=  -I platform/linux/valloc

src = $(wildcard ./test/*.c)
src += $(wildcard ./common/*.c)
src += $(wildcard ./platform/linux/*.c)
src += $(wildcard ./platform/linux/valloc/*.c)
src += $(wildcard ./pahoMqtt/*.c)
src += $(wildcard ./mqttclient/*.c)

obj = $(patsubst %.c, %.o, $(src))
target = app.o
CC = gcc

$(target): $(obj)
	$(CC) $(CFLAGS_INC) $(obj) -o $(target)  -lpthread

%.o: %.c
	$(CC) $(CFLAGS_INC) -c $< -o $@  -lpthread

.PHONY: clean
clean:
	rm -rf $(obj) $(target)
