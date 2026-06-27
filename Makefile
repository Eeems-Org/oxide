.PHONY: all
all: release

.ONESHELL:

MAKEFLAGS := --jobs=$(shell nproc)

DIST=$(CURDIR)/release
BUILD=$(CURDIR)/.build
BUILDNAME?=oxide
TOOLCHAIN?=5.7.119

ifneq ($(filter debug,$(FEATURES)),)
DEFINES += CONFIG+="debug"
endif

OBJ = oxide.pro
OBJ += $(wildcard applications/**)
OBJ += $(wildcard assets/**)
OBJ += $(wildcard interfaces/**)
OBJ += $(wildcard qmake/**)
OBJ += $(wildcard shared/**)
OBJ += $(wildcard tests/**)

.PHONY: clean-base
clean-base:
	rm -rf \
		$(DIST) \
		$(BUILD)/$(BUILDNAME)/Makefile \
		$(BUILD)/$(BUILDNAME)/shared/cpptrace/src

.PHONY: clean
clean: clean-base
	rm -rf $(BUILD)

.PHONY: release
release: clean-base build $(DIST)
	# Force cpptrace makefile to regenerate so that install targets get when being built in vbuild
	cd $(BUILD)/$(BUILDNAME)/shared/cpptrace && make qmake $(DEFINES)
	# Force liboxide makefile to regenerate so that install targets get when being built in vbuild
	cd $(BUILD)/$(BUILDNAME)/shared/liboxide && make qmake $(DEFINES)
	# Force libblight makefile to regenerate so that install targets get when being built in vbuild
	cd $(BUILD)/$(BUILDNAME)/shared/libblight && make qmake $(DEFINES)
	# Force libblight_protocol makefile to regenerate so that install targets get when being built in vbuild
	cd $(BUILD)/$(BUILDNAME)/shared/libblight_protocol && make qmake $(DEFINES)
	INSTALL_ROOT=$(DIST) $(MAKE) --output-sync=target -C $(BUILD)/$(BUILDNAME) install

.PHONY: build
build: $(BUILD)/$(BUILDNAME)/Makefile
	$(MAKE) --output-sync=target -C $(BUILD)/$(BUILDNAME) all

.PHONY: package
package: package-armv7 package-aarch64

.PHONY: build-armv7
build-armv7: build-rm1

.PHONY: build-aarch64
build-aarch64: build-rm1

.PHONY: package-armv7
package-armv7: $(DIST)/armv7/APKINDEX.tar.gz

.PHONY: package-aarch64
package-aarch64: $(DIST)/aarch64/APKINDEX.tar.gz

$(DIST)/%/APKINDEX.tar.gz: $(DIST) $(BUILD)/package/oxide.tar.gz $(BUILD)/package/VELBUILD
	rm -f \
		$(DIST)/$*/*.apk \
		$(BUILD)/package/dist/$*/*.apk
	CARCH=$* vbuild -C $(BUILD)/package
	@echo "Creating APKINDEX.tar.gz"
	podman run \
		--rm \
		--interactive \
		--volume="$(BUILD)/package/dist/$*:/packages" \
		--volume="$$HOME/.config/vbuild:/keys:ro" \
		--workdir=/packages \
		--env=key=$${VBUILD_KEY_NAME:-vbuild} \
		ghcr.io/eeems/vbuild-builder:main \
		sh -e <<-'EOT'
			cp /keys/$$key.rsa* /etc/apk/keys/
			apk index --no-warnings --output=APKINDEX.tar.gz *.apk
			tar xOf APKINDEX.tar.gz APKINDEX
			abuild-sign -k /keys/$$key.rsa APKINDEX.tar.gz
			cp /keys/$$key.rsa.pub .
		EOT
	mkdir -p $(DIST)/$*
	cp -a $(BUILD)/package/dist/$*/. $(DIST)/$*
	[ -f $(DIST)/$*/APKINDEX.tar.gz ]

.PHONY: deploy-armv7
deploy-armv7: _deploy-armv7

.PHONY: deploy-aarch64
deploy-aarch64: _deploy-aarch64

_deploy-%: package-%
	rsync -aP --delete $(DIST)/$* remarkable:/home/root/packages

.NOTPARALLEL: install-armv7
.PHONY: install-armv7
install-armv7: deploy-armv7 _install

.NOTPARALLEL: install-aarch64
.PHONY: install-aarch64
install-aarch64: deploy-aarch64 _install

.PHONY: _install
_install:
	ssh remarkable bash -le <<-'EOT'
		repo='@oxide /home/root/packages'
		if ! grep -qF "$$repo" /home/root/.vellum/etc/apk/repositories 2>/dev/null; then
			echo "$$repo" >> /home/root/.vellum/etc/apk/repositories
		fi
		vellum update
		vellum add launcherctl-oxide@oxide
		vellum update
	EOT

.PHONY: build-rm1
build-rm1: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rm1 \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rm1 \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/rm1/$(TOOLCHAIN)/environment-setup-cortexa9hf-neon-remarkable-linux-gnueabi; make FEATURES=$(FEATURES) release'

.PHONY: build-rm2
build-rm2: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rm2 \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rm2 \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/rm2/$(TOOLCHAIN)/environment-setup-cortexa7hf-neon-remarkable-linux-gnueabi; make FEATURES=$(FEATURES) release'

.PHONY: build-rmpp
build-rmpp: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rmpp \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rmpp \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/ferrari/$(TOOLCHAIN)/environment-setup-cortexa53-crypto-remarkable-linux; make FEATURES=$(FEATURES) release'

.PHONY: build-rmppm
build-rmppm: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rmppm \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rmppm \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/chiappa/$(TOOLCHAIN)/environment-setup-cortexa55-remarkable-linux; make FEATURES=$(FEATURES) release'

.PHONY: build-rmppure
build-rmppure: clean-base $(DIST)
	podman run \
		--env BUILDNAME=oxide-rmppure \
		--rm \
		--volume=$(CURDIR):/src \
		--workdir=/src \
		eeems/remarkable-toolchain:$(TOOLCHAIN)-rmppure \
		bash -exc 'apt-get update; apt-get install -y clang-format;source /opt/codex/tatsu/$(TOOLCHAIN)/environment-setup-cortexa55-remarkable-linux; make FEATURES=$(FEATURES) release'

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

$(BUILD)/package/oxide.tar.gz: $(OBJ) $(BUILD)/package
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

$(BUILD)/package/VELBUILD: REV="$(shell TZ=UTC git show -s --date=format:'%Y%m%d' --format=%cd HEAD)"
$(BUILD)/package/VELBUILD: VERSION="$(shell bash -c "grep 'VERSION =' qmake/common.pri | awk '{print \$$3}'")"
$(BUILD)/package/VELBUILD: $(BUILD)/package $(OBJ) VELBUILD
	sed "s/~VERSION~/$(VERSION)_git$(REV)/" ./VELBUILD > $(BUILD)/package/VELBUILD
	if git diff --quiet HEAD 2>/dev/null; then \
		TIMESTAMP=$$(git show -s --format=%ct HEAD); \
	else \
		TIMESTAMP=$$(date +%s); \
	fi; \
	MIDNIGHT=$$(TZ=UTC date -d "$(REV)" +%s); \
	sed -i "s/~TIMESTAMP~/$$((TIMESTAMP - MIDNIGHT))/" $(BUILD)/package/VELBUILD
	vbuild -C $(BUILD)/package checksum

SRC_FILES = $(shell find -name '*.sh' | grep -v shared/sentry | grep -v shared/cpptrace | grep -v shared/doxygen-awesome-css )
SRC_FILES += VELBUILD

CPP_FILES = $(wildcard applications/**/*.cpp) $(wildcard applications/**/*.h)
CPP_FILES += $(wildcard shared/**/*.cpp) $(wildcard shared/**/*.h)
CPP_FILES += $(wildcard tests/**/*.cpp) $(wildcard tests/**/*.h)

.PHONY: lint
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

.PHONY: format
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
