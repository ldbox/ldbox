# Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
# Licensed under LGPL 2.1

TOPDIR = $(CURDIR)
OBJDIR = $(TOPDIR)
SRCDIR = $(TOPDIR)
VPATH = $(SRCDIR)

MACH := $(shell uname -m)
OS := $(shell uname -s)

ifeq ($(OS),Linux)
LIBLB_LDFLAGS = -Wl,-soname=$(LIBLB_SONAME) \
		-Wl,--version-script=preload/export.map

SHLIBEXT = so
else
SHLIBEXT = dylib
endif

# pick main build bitness
ifeq ($(MACH),x86_64)
PRI_OBJDIR = obj-64
else
PRI_OBJDIR = obj-32
endif

CC = gcc
CXX = g++
LD = ld
PACKAGE_VERSION = 2.3.90

ifeq ($(shell if [ -d $(SRCDIR)/.git ]; then echo y; fi),y)
GIT_PV_COMMIT := $(shell git --git-dir=$(SRCDIR)/.git log -1 --pretty="format:%h" $(PACKAGE_VERSION) -- 2>/dev/null)
GIT_CUR_COMMIT := $(shell git --git-dir=$(SRCDIR)/.git log -1 --pretty="format:%h" HEAD -- 2>/dev/null)
GIT_MODIFIED := $(shell cd $(SRCDIR); git ls-files -m)
GIT_TAG_EXISTS := $(strip $(shell git --git-dir=$(SRCDIR)/.git tag -l $(PACKAGE_VERSION) 2>/dev/null))
ifeq ("$(GIT_TAG_EXISTS)","")
# Add -rc to version to signal that the PACKAGE_VERSION release has NOT
# been yet tagged
PACKAGE_VERSION := $(PACKAGE_VERSION)-rc
endif
ifneq ($(GIT_PV_COMMIT),$(GIT_CUR_COMMIT))
PACKAGE_VERSION := $(PACKAGE_VERSION)-$(GIT_CUR_COMMIT)
endif
ifneq ($(strip "$(GIT_MODIFIED)"),"")
PACKAGE_VERSION := $(PACKAGE_VERSION)-dirty
endif
endif
PACKAGE = "LDBOX"
LIBLB_SONAME = "liblb.so.1"
LLBUILD ?= $(SRCDIR)/llbuild
PROTOTYPEWARNINGS=-Wmissing-prototypes -Wstrict-prototypes

ifdef E
WERROR = -Wno-error
else
WERROR = -Werror
endif

# targets variable will be filled by llbuild
targets = 
subdirs = preload luaif lblib pathmapping execs network rule_tree utils lbrdbd wrappers

-include config.mak

CFLAGS += -O2 -g -Wall -W
CFLAGS += -I$(OBJDIR)/include -I$(SRCDIR)/include
CFLAGS += -I$(SRCDIR)/luaif/lua-5.1.4/src
CFLAGS += -D_GNU_SOURCE=1 -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1
CFLAGS += $(MACH_CFLAG)
LDFLAGS += $(MACH_CFLAG)
CXXFLAGS = 

# Uncomment following two lines to activate the "processclock" reports:
#CFLAGS += -DUSE_PROCESSCLOCK
#LDFLAGS += -lrt

include $(LLBUILD)/Makefile.include

ifdef prefix
CONFIGURE_ARGS = --prefix=$(prefix)
else
CONFIGURE_ARGS = 
endif

ifneq ($(lib32dir),)
all: multilib
else
all: regular
endif

do-all: $(targets)

.configure:
	$(SRCDIR)/configure $(CONFIGURE_ARGS)
	@touch .configure

.PHONY: .version
.version:
	@(set -e; \
	if [ -f .version ]; then \
		version=$$(cat .version); \
		if [ "$(PACKAGE_VERSION)" != "$$version" ]; then \
			echo $(PACKAGE_VERSION) > .version; \
		fi \
	else \
		echo $(PACKAGE_VERSION) > .version; \
	fi)

$(SRCDIR)/include/ldbox_version.h: .version Makefile
	echo "/* Automatically generated file. Do not edit. */" >$(SRCDIR)/include/ldbox_version.h
	echo '#define LDBOX_VERSION "'`cat .version`'"' >>$(SRCDIR)/include/ldbox_version.h
	echo '#define LIBLB_SONAME "'$(LIBLB_SONAME)'"' >>$(SRCDIR)/include/ldbox_version.h

regular: .configure .version
	@$(MAKE) -f $(SRCDIR)/Makefile --include-dir=$(SRCDIR) SRCDIR=$(SRCDIR) do-all

multilib:
	@mkdir -p obj-32
	@mkdir -p obj-64
	@$(MAKE) MACH_CFLAG=-m32 -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. libdir=$(lib32dir) regular
	@$(MAKE) MACH_CFLAG=-m64 -C obj-64 --include-dir=.. -f ../Makefile SRCDIR=.. libdir=$(libdir) regular


gcc_bins = addr2line ar as cc c++ c++filt cpp g++ gcc gcov gdb gdbtui gprof ld nm objcopy objdump ranlib rdi-stub readelf run size strings strip
host_prefixed_gcc_bins = $(foreach v,$(gcc_bins),host-$(v))

lb_modes = emulate tools simple accel nomap emulate+toolchain emulate+toolchain+utils \
	    	obs-deb-install obs-deb-build \
	    	obs-rpm-install obs-rpm-build obs-rpm-build+pp
lb_net_modes = localhost offline online online_privatenets

tarball:
	@git archive --format=tar --prefix=ldbox-$(PACKAGE_VERSION)/ $(PACKAGE_VERSION) | bzip2 >ldbox-$(PACKAGE_VERSION).tar.bz2


install-noarch: regular
	$(P)INSTALL
	@if [ -d $(DESTDIR)$(bindir) ] ; \
	then echo "$(DESTDIR)$(bindir) present" ; \
	else install -d -m 755 $(DESTDIR)$(bindir) ; \
	fi
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/lua_scripts
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/modes
	$(Q)(set -e; for d in $(lb_modes); do \
		install -d -m 755 $(DESTDIR)$(datadir)/ldbox/modes/$$d; \
		for f in $(SRCDIR)/modes/$$d/*; do \
			install -c -m 644 $$f $(DESTDIR)$(datadir)/ldbox/modes/$$d; \
		done; \
	done)
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/net_rules
	$(Q)(set -e; for d in $(lb_net_modes); do \
		install -d -m 755 $(DESTDIR)$(datadir)/ldbox/net_rules/$$d; \
		for f in $(SRCDIR)/net_rules/$$d/*; do \
			install -c -m 644 $$f $(DESTDIR)$(datadir)/ldbox/net_rules/$$d; \
		done; \
	done)
	# "accel" == "devel" mode in 2.3.x:
	$(Q)ln -sf accel $(DESTDIR)$(datadir)/ldbox/modes/devel
	# Rule libraries
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/rule_lib
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/rule_lib/fs_rules
	$(Q)(set -e; for f in $(SRCDIR)/rule_lib/fs_rules/*; do \
		install -c -m 644 $$f $(DESTDIR)$(datadir)/ldbox/rule_lib/fs_rules; \
	done)
	# "scripts" and "wrappers" are visible to the user in some 
	# mapping modes, "lib" is for ldbox's internal use
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/lib
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/scripts
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/wrappers
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/ldbox/tests
	@if [ -d $(DESTDIR)$(mandir)/man1 ] ; \
	then echo "$(DESTDIR)$(mandir)/man1 present" ; \
	else install -d -m 755 $(DESTDIR)$(mandir)/man1 ; \
	fi
	@if [ -d $(DESTDIR)$(mandir)/man7 ] ; \
	then echo "$(DESTDIR)$(mandir)/man7 present" ; \
	else install -d -m 755 $(DESTDIR)$(mandir)/man7 ; \
	fi
	$(Q)echo "$(PACKAGE_VERSION)" > $(DESTDIR)$(datadir)/ldbox/version
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb $(DESTDIR)$(bindir)/lb
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-init $(DESTDIR)$(bindir)/lb-init
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-config $(DESTDIR)$(bindir)/lb-config
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-session $(DESTDIR)$(bindir)/lb-session
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-build-libtool $(DESTDIR)$(bindir)/lb-build-libtool
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-start-qemuserver $(DESTDIR)$(bindir)/lb-start-qemuserver
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-qemu-gdbserver-prepare $(DESTDIR)$(bindir)/lb-qemu-gdbserver-prepare

	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-cmp-checkbuilddeps-output.pl $(DESTDIR)$(datadir)/ldbox/lib/lb-cmp-checkbuilddeps-output.pl

	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-upgrade-config $(DESTDIR)$(datadir)/ldbox/scripts/lb-upgrade-config
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-parse-lb-init-args $(DESTDIR)$(datadir)/ldbox/scripts/lb-parse-lb-init-args
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-config-gcc-toolchain $(DESTDIR)$(datadir)/ldbox/scripts/lb-config-gcc-toolchain
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-config-debian $(DESTDIR)$(datadir)/ldbox/scripts/lb-config-debian
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-check-pkg-mappings $(DESTDIR)$(datadir)/ldbox/scripts/lb-check-pkg-mappings
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-exitreport $(DESTDIR)$(datadir)/ldbox/scripts/lb-exitreport
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-generate-locales $(DESTDIR)$(datadir)/ldbox/scripts/lb-generate-locales
	$(Q)install -c -m 755 $(SRCDIR)/utils/lb-logz $(DESTDIR)$(bindir)/lb-logz
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/init*.lua $(DESTDIR)$(datadir)/ldbox/lua_scripts/
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/rule_constants.lua $(DESTDIR)$(datadir)/ldbox/lua_scripts/
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/exec_constants.lua $(DESTDIR)$(datadir)/ldbox/lua_scripts/
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/argvenvp_gcc.lua $(DESTDIR)$(datadir)/ldbox/lua_scripts/argvenvp_gcc.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/argvenvp_misc.lua $(DESTDIR)$(datadir)/ldbox/lua_scripts/argvenvp_misc.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/create_reverse_rules.lua $(DESTDIR)$(datadir)/ldbox/lua_scripts/create_reverse_rules.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/argvmods_loader.lua $(DESTDIR)$(datadir)/ldbox/lua_scripts/argvmods_loader.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/add_rules_to_rule_tree.lua $(DESTDIR)$(datadir)/ldbox/lua_scripts/add_rules_to_rule_tree.lua

	$(Q)install -c -m 644 $(SRCDIR)/tests/* $(DESTDIR)$(datadir)/ldbox/tests
	$(Q)chmod a+x $(DESTDIR)$(datadir)/ldbox/tests/run.sh

	$(Q)install -c -m 644 $(SRCDIR)/docs/*.1 $(DESTDIR)$(mandir)/man1
	$(Q)install -c -m 644 $(OBJDIR)/preload/liblb_interface.7 $(DESTDIR)$(mandir)/man7
	$(Q)rm -f $(DESTDIR)$(datadir)/ldbox/host_usr
	$(Q)ln -sf /usr $(DESTDIR)$(datadir)/ldbox/host_usr
	@# Wrappers:
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/deb-pkg-tools-wrapper $(DESTDIR)$(datadir)/ldbox/wrappers/dpkg
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/deb-pkg-tools-wrapper $(DESTDIR)$(datadir)/ldbox/wrappers/apt-get
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/ldconfig $(DESTDIR)$(datadir)/ldbox/wrappers/ldconfig
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/texi2html $(DESTDIR)$(datadir)/ldbox/wrappers/texi2html
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/dpkg-checkbuilddeps $(DESTDIR)$(datadir)/ldbox/wrappers/dpkg-checkbuilddeps
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/debconf2po-update $(DESTDIR)$(datadir)/ldbox/wrappers/debconf2po-update
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/host-gcc-tools-wrapper $(DESTDIR)$(datadir)/ldbox/wrappers/host-gcc-tools-wrapper
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/gdb $(DESTDIR)$(datadir)/ldbox/wrappers/gdb
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/ldd $(DESTDIR)$(datadir)/ldbox/wrappers/ldd
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/pwd $(DESTDIR)$(datadir)/ldbox/wrappers/pwd
	$(Q)(set -e; cd $(DESTDIR)$(datadir)/ldbox/wrappers; \
	for f in $(host_prefixed_gcc_bins); do \
		ln -sf host-gcc-tools-wrapper $$f; \
	done)

ifneq ($(lib32dir),)
install: install-multilib
else
install: do-install
endif

do-install: install-noarch
	$(P)INSTALL
	@if [ -d $(DESTDIR)$(libdir) ] ; \
	then echo "$(DESTDIR)$(libdir) present" ; \
	else install -d -m 755 $(DESTDIR)$(libdir) ; \
	fi
	$(Q)install -d -m 755 $(DESTDIR)$(libdir)/liblb
	$(Q)install -d -m 755 $(DESTDIR)$(libdir)/liblb/wrappers
	$(Q)install -c -m 755 $(OBJDIR)/wrappers/fakeroot $(DESTDIR)$(libdir)/liblb/wrappers/fakeroot
	$(Q)install -c -m 755 $(OBJDIR)/preload/liblb.$(SHLIBEXT) $(DESTDIR)$(libdir)/liblb/liblb.so.$(PACKAGE_VERSION)
	$(Q)install -c -m 755 $(OBJDIR)/utils/lbrdbdctl $(DESTDIR)$(libdir)/liblb/lbrdbdctl
	$(Q)install -c -m 755 $(OBJDIR)/utils/lb-show $(DESTDIR)$(bindir)/lb-show
	$(Q)install -c -m 755 $(OBJDIR)/utils/lb-monitor $(DESTDIR)$(bindir)/lb-monitor
	$(Q)install -c -m 755 $(OBJDIR)/utils/lb-ruletree $(DESTDIR)$(bindir)/lb-ruletree
	$(Q)install -c -m 755 $(OBJDIR)/lbrdbd/lbrdbd $(DESTDIR)$(bindir)/lbrdbd
ifeq ($(OS),Linux)
	$(Q)/sbin/ldconfig -n $(DESTDIR)$(libdir)/liblb
endif

install-multilib: multilib
	@$(MAKE) -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. do-install-lib32
	@$(MAKE) -C obj-64 --include-dir=.. -f ../Makefile SRCDIR=.. do-install

do-install-lib32:
	$(P)INSTALL
	@if [ -d $(DESTDIR)$(lib32dir) ] ; \
	then echo "$(DESTDIR)$(lib32dir) present" ; \
	else install -d -m 755 $(DESTDIR)$(lib32dir) ; \
	fi
	$(Q)install -d -m 755 $(DESTDIR)$(lib32dir)/liblb
	$(Q)install -c -m 755 preload/liblb.$(SHLIBEXT) $(DESTDIR)$(lib32dir)/liblb/liblb.so.$(PACKAGE_VERSION)
ifeq ($(OS),Linux)
	$(Q)/sbin/ldconfig -n $(DESTDIR)$(lib32dir)/liblb
endif

CLEAN_FILES += $(targets) config.status config.log

superclean: clean
	$(P)CLEAN
	$(Q)rm -rf obj-32 obj-64 .configure-multilib .configure
	$(Q)rm -rf include/config.h config.mak
	$(Q)rm -rf utils/lb-config-gcc-toolchain utils/lb-config utils/lb-init utils/lb-upgrade-config utils/lb

clean-multilib:
	$(P)CLEAN
	-$(Q)$(MAKE) -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. do-clean
	-$(Q)$(MAKE) -C obj-64 --include-dir .. -f ../Makefile SRCDIR=.. do-clean

ifneq ($(lib32dir),)
clean: clean-multilib do-clean
else
clean: do-clean
endif

do-clean:
	$(P)CLEAN
	$(Q)$(ll_clean)

