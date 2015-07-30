# beware of namespace collisions, all subdir makefiles are included
# into the top-level Makefile

$(D)/lb-show: CFLAGS := $(CFLAGS) -Wall -W $(WERROR) \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/lb-show.o: preload/exported.h
$(D)/lb-show: $(D)/lb-show.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ldl

#------------
# lb-ruletree, a debugging tool
$(D)/lb-ruletree: CFLAGS := $(CFLAGS) -Wall -W $(WERROR) \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/lb-ruletree.o: preload/exported.h
$(D)/lb-ruletree: $(D)/lb-ruletree.o $(D)/../rule_tree/rule_tree.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ldl

#------------
# lbrdbdctl, a tool for communicating with lbrdbd
$(D)/lbrdbdctl: override CFLAGS := $(CFLAGS) -Wall -W $(WERROR) \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/lbrdbdctl.o: preload/exported.h
$(D)/lbrdbdctl: $(D)/lbrdbdctl.o
$(D)/lbrdbdctl: rule_tree/rule_tree_rpc_client.o
$(D)/lbrdbdctl: lblib/lb_log.o
$(D)/lbrdbdctl: lbrdbd/libsupport.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ldl

targets := $(targets) $(D)/lbrdbdctl
#------------

$(D)/lb-monitor: CFLAGS := $(CFLAGS) -Wall -W $(WERROR) \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/lb-monitor: $(D)/lb-monitor.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(D)/lb-interp-wrapper: CFLAGS := $(CFLAGS) -Wall -W $(WERROR) \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/lb-interp-wrapper: $(D)/lb-interp-wrapper.o preload/liblb.$(SHLIBEXT)
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ldl


targets := $(targets) $(D)/lb-show $(D)/lb-monitor $(D)/lb-ruletree
