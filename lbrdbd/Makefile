# beware of namespace collisions, all subdir makefiles are included
# into the top-level Makefile

$(D)/lbrdbd: CFLAGS := $(CFLAGS) -Wall -W $(WERROR) \
		$(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include \
		-I$(SRCDIR)/preload -Ipreload/

$(D)/lbrdbd: $(D)/lbrdbd.o \
		$(D)/server_socket.o \
		$(D)/libsupport.o \
		$(D)/ruletree_server.o \
		$(D)/rule_tree_luaif.o \
		lblib/lb_log.o \
		lblib/lb_utils.o \
		rule_tree/rule_tree.o \
		rule_tree/rule_tree_utils.o \
		pathmapping/paths_ruletree_maint.o \
		execs/exec_ruletree_maint.o \
		luaif/lblib_luaif.o \
		luaif/liblua.a
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm -ldl

targets := $(targets) $(D)/lbrdbd
