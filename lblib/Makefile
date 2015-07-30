
objs := $(D)/lb_log.o \
	$(D)/processclock.o \
	$(D)/lb_utils.o \
	$(D)/lb_pthread_if.o

$(D)/lb_log.o: preload/exported.h

lblib/liblblib.a: $(objs)
lblib/liblblib.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(OBJDIR)/preload -I$(SRCDIR)/preload \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes
lblib/liblblib.a: override LDFLAGS := $(LDFLAGS)
lblib/liblblib.a: override LIBS :=

lblib/lb_log.o: include/ldbox_version.h

targets := $(targets) $(D)/liblblib.a

