.PHONY: all clean release build sentry package

all: release

.NOTPARALLEL:

MAKEFLAGS := --jobs=$(shell nproc)

# FEATURES += sentry

DIST=$(CURDIR)/release
BUILD=$(CURDIR)/.build
BUILDNAME?=oxide
TOOLCHAIN?=5.7.119

ifneq ($(filter debug,$(FEATURES)),)
DEFINES += CONFIG+="debug"
endif
ifneq ($(filter sentry,$(FEATURES)),)
DEFINES += DEFINES+="SENTRY"
endif

OBJ += $(BUILD)/$(BUILDNAME)/Makefile

clean-base:
	rm -rf \
		$(DIST) \
		$(BUILD)/$(BUILDNAME)/Makefile \
		$(BUILD)/$(BUILDNAME)/shared/sentry/src \
		$(BUILD)/$(BUILDNAME)/shared/cpptrace/src

clean: clean-base
	rm -rf $(BUILD)

release: clean-base build $(DIST)
ifneq ($(filter sentry,$(FEATURES)),)
	# Force sentry makefile to regenerate so that install targets get when being build in vbuild
	cd $(BUILD)/$(BUILDNAME)/shared/sentry && make qmake $(DEFINES)
endif
	# Force liboxide makefile to regenerate so that install targets get when being build in vbuild
	cd $(BUILD)/$(BUILDNAME)/shared/liboxide && make qmake $(DEFINES)
	# Force libblight makefile to regenerate so that install targets get when being build in vbuild
	cd $(BUILD)/$(BUILDNAME)/shared/libblight && make qmake $(DEFINES)
	# Force libblight_protocol makefile to regenerate so that install targets get when being build in vbuild
	cd $(BUILD)/$(BUILDNAME)/shared/libblight_protocol && make qmake $(DEFINES)
	INSTALL_ROOT=$(DIST) $(MAKE) --output-sync=target -C $(BUILD)/$(BUILDNAME) install

build: $(OBJ)
	$(MAKE) --output-sync=target -C $(BUILD)/$(BUILDNAME) all

package: REV="$(shell git rev-list --count HEAD)"
package: VERSION="$(shell bash -c "grep 'VERSION =' qmake/common.pri | awk '{print \$$3}'")"
package: version.txt $(DIST) $(BUILD)/package/oxide.tar.gz
	sed "s/~VERSION~/`cat version.txt`/" ./VELBUILD > $(BUILD)/package/VELBUILD
	vbuild -C $(BUILD)/package checksum
	CARCH=armv7 vbuild -C $(BUILD)/package
	CARCH=aarch64 vbuild -C $(BUILD)/package
	cp -a \
		$(BUILD)/package/dist/armv7/* \
		$(BUILD)/package/dist/armv7/* \
		$(DIST)

build-rm1: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rm1 \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rm1 \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/rm1/$(TOOLCHAIN)/environment-setup-cortexa9hf-neon-remarkable-linux-gnueabi; make FEATURES=$(FEATURES) release'

build-rm2: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rm2 \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rm2 \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/rm2/$(TOOLCHAIN)/environment-setup-cortexa7hf-neon-remarkable-linux-gnueabi; make FEATURES=$(FEATURES) release'

build-rmpp: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rmpp \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rmpp \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/ferrari/$(TOOLCHAIN)/environment-setup-cortexa53-crypto-remarkable-linux; make FEATURES=$(FEATURES) release'

build-rmppm: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rmppm \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rmppm \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/chiappa/$(TOOLCHAIN)/environment-setup-cortexa55-remarkable-linux; make FEATURES=$(FEATURES) release'

build-rmppure: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rmppure \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rmppure \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/tatsu/$(TOOLCHAIN)/environment-setup-cortexa55-remarkable-linux; make FEATURES=$(FEATURES) release'

version.txt:
	if [ -d .git ];then \
		echo "$(VERSION)_git$(REV)" > version.txt; \
	else \
		echo "$(VERSION)_pre" > version.txt; \
	fi;

$(DIST):
	mkdir -p $(DIST)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/.nobackup: $(BUILD)
	touch $(BUILD)/.nobackup

$(BUILD)/$(BUILDNAME): $(BUILD)/.nobackup
	mkdir -p $(BUILD)/$(BUILDNAME)

$(BUILD)/$(BUILDNAME)/Makefile: $(BUILD)/$(BUILDNAME)
	cd $(BUILD)/$(BUILDNAME) && qmake -r $(DEFINES) $(CURDIR)

$(BUILD)/package:
	mkdir -p $(BUILD)/package
	rm -rf $(BUILD)/package/build

PKG_OBJ = oxide.pro Makefile
PKG_OBJ += $(wildcard applications/**)
PKG_OBJ += $(wildcard assets/**)
PKG_OBJ += $(wildcard interfaces/**)
PKG_OBJ += $(wildcard qmake/**)
PKG_OBJ += $(wildcard shared/**)
PKG_OBJ += $(wildcard tests/**)

$(BUILD)/package/oxide.tar.gz: $(PKG_OBJ) $(BUILD)/package
	rm -f $(BUILD)/package/oxide.tar.gz
	tar \
		--exclude='$(CURDIR)/.git' \
		--exclude='$(BUILD)' \
		--exclude='$(CURDIR)/dist' \
		--exclude='$(DIST)' \
		-czvf $(BUILD)/package/oxide.tar.gz \
		applications \
		assets \
		interfaces \
		qmake \
		shared \
		tests \
		oxide.pro \
		Makefile

SRC_FILES = $(shell find -name '*.sh' | grep -v shared/sentry | grep -v shared/cpptrace | grep -v shared/doxygen-awesome-css )
SRC_FILES += package

CPP_FILES = $(wildcard applications/**/*.cpp) $(wildcard applications/**/*.h)
CPP_FILES += $(wildcard shared/**/*.cpp) $(wildcard shared/**/*.h)
CPP_FILES += $(wildcard tests/**/*.cpp) $(wildcard tests/**/*.h)

lint:
	@shfmt \
		-d\
		-s \
		-i 4 \
		-bn \
		-sr \
		$(SRC_FILES)
	@clang-format \
		--dry-run \
		--Werror \
		--fallback-style=mozilla \
		$(CPP_FILES)

format:
	@shfmt \
		-l \
		-w \
		-s \
		-i 4 \
		-bn \
		-sr \
		$(SRC_FILES)
	@clang-format \
		--fallback-style=mozilla \
		-i \
		$(CPP_FILES)
