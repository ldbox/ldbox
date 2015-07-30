
objs := $(D)/net_rules.o

network/liblbnet.a: $(objs)
network/liblbnet.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload -I$(SRCDIR)/network \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes
network/liblbnet.a: override LDFLAGS := $(LDFLAGS)
network/liblbnet.a: override LIBS :=

targets := $(targets) $(D)/liblbnet.a

