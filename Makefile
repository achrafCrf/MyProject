
CFLAGS += -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L
LDLIBS += -lpthread -lgpiod -lm

OBJS = src/main.o src/scheduler.o src/bea.o src/bel.o src/bom.o src/bts.o src/mms.o src/ArkStudio.o src/watchdog.o src/config.o

main: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	rm -f src/*.o main
