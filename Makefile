CFLAGS?= -O3
APP_CFLAGS:=$(CFLAGS) -MMD -MP
LDFLAGS?=
APP_LDFLAGS:=$(LDFLAGS) -lm

APP=ip_monitor
APP_SRC=main.c ip_monitor.c ip_renderer.c
LIB_TEV=./tev/libtev.a
LIB_SCHRIFT=./libschrift/libschrift.a
APP_STATIC_LIBS=$(LIB_TEV) $(LIB_SCHRIFT)

.PHONY: all
all: ip_monitor

ip_monitor: $(APP_SRC:.c=.o) $(APP_STATIC_LIBS)
	$(CC) -o $@ $^ $(APP_LDFLAGS)

%.o: %.c
	$(CC) $(APP_CFLAGS) -c -o $@ $<

$(LIB_TEV):
	$(MAKE) -C tev libtev.a

$(LIB_SCHRIFT):
	$(MAKE) -C libschrift libschrift.a CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS)"

-include $(APP_SRC:.c=.d)

.PHONY:clean
clean:
	rm -f $(APP) *.o *.d
	$(MAKE) -C tev clean
	$(MAKE) -C libschrift clean
