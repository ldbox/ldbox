objs := wrappers.o privatewrappers.o \
	liblb.o lb_l10n.o glob.o glob64.o \
	network.o \
	execgates.o \
	miscgates.o \
	tmpnamegates.o \
	vperm_filestatgates.o \
	vperm_uid_gid_gates.o \
	chrootgate.o \
	vperm_statfuncts.o \
	fdpathdb.o procfs.o mempcpy.o \
	union_dirs.o \
	system.o \
	lbcontext.o

ifeq ($(shell uname -s),Linux)
LIBLB_LDFLAGS = -Wl,-soname=$(LIBLB_SONAME) \
		-Wl,--version-script=preload/export.map

# DO NOT add any libraries that depend on libpthread to
# LIBLB_LIBS or LIBS!! see the comments in luaif.c.

LIBLB_LIBS =
SHLIBEXT = so
else
LIBLB_LIBS =
SHLIBEXT = dylib
endif


$(D)/liblb.$(SHLIBEXT): $(call O,$(objs))
# $(D)/liblb.$(SHLIBEXT): luaif/libluaif.a luaif/liblua.a lblib/liblblib.a
$(D)/liblb.$(SHLIBEXT): lblib/liblblib.a
$(D)/liblb.$(SHLIBEXT): pathmapping/libpaths.a
$(D)/liblb.$(SHLIBEXT): execs/libexecs.a
$(D)/liblb.$(SHLIBEXT): network/liblbnet.a
$(D)/liblb.$(SHLIBEXT): rule_tree/libruletree.a
$(D)/liblb.$(SHLIBEXT): CFLAGS := $(CFLAGS) -fPIC -Wall -W \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS)
$(D)/liblb.$(SHLIBEXT): LDFLAGS := $(LDFLAGS) $(LIBLB_LDFLAGS)
$(D)/liblb.$(SHLIBEXT): LIBS := -ldl $(LIBLB_LIBS)

targets := $(targets) $(D)/liblb.$(SHLIBEXT)

$(D)/liblb.o $(D)/lb_l10n.o: preload/exported.h
$(D)/exported.h $(D)/ldexportlist: preload/wrappers.c
$(D)/wrappers.c: preload/interface.master preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-n public \
		-W preload/wrappers.c \
		-E preload/exported.h \
		-M preload/export.map \
		-V $(PACKAGE_VERSION) \
		-m preload/liblb_interface.7 \
		<$(SRCDIR)/preload/interface.master

$(D)/privatewrappers.h: preload/privatewrappers.c
$(D)/privatewrappers.c: preload/interface.private preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-n private \
		-W preload/privatewrappers.c \
		-E preload/privatewrappers.h \
		<$(SRCDIR)/preload/interface.private

generated := preload/wrappers.c preload/exported.h preload/ldexportlist
.PRECIOUS: $(generated)

CLEAN_FILES += $(generated)
