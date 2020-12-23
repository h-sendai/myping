PROG = myping
CFLAGS += -g -O2 -Wall
CFLAGS += -std=gnu99
# CFLAGS += -pthread
# LDLIBS += -L/usr/local/lib -lmylib
# LDLIBS += -lrt
# LDFLAGS += -pthread

all: $(PROG)
	@sudo setcap 'CAP_NET_RAW+eip' myping
OBJS += $(PROG).o
OBJS += in_cksum.o
OBJS += set_timer.o
OBJS += my_signal.o
$(PROG): $(OBJS)

clean:
	rm -f *.o $(PROG)
