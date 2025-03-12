.PHONY: all clean release build sentry package

all: release

.NOTPARALLEL:

MAKEFLAGS := --jobs=$(shell nproc)

# FEATURES += sentry

DIST=$(CURDIR)/release
BUILD=$(CURDIR)/.build

ifneq ($(filter sentry,$(FEATURES)),)
DEFINES += DEFINES+="SENTRY"
endif

OBJ += $(BUILD)/oxide/Makefile

clean-base:
	rm -rf $(DIST) $(BUILD)/oxide

clean: clean-base
	rm -rf $(BUILD)

release: clean-base build $(DIST)
ifneq ($(filter sentry,$(FEATURES)),)
	# Force sentry makefile to regenerate so that install targets get when being build in toltecmk
	cd $(BUILD)/oxide/shared/sentry && make qmake
endif
	# Force liboxide makefile to regenerate so that install targets get when being build in toltecmk
	cd $(BUILD)/oxide/shared/liboxide && make qmake
	# Force libblight makefile to regenerate so that install targets get when being build in toltecmk
	cd $(BUILD)/oxide/shared/libblight && make qmake
	# Force libblight_protocol makefile to regenerate so that install targets get when being build in toltecmk
	cd $(BUILD)/oxide/shared/libblight_protocol && make qmake
	INSTALL_ROOT=$(DIST) $(MAKE) --output-sync=target -C $(BUILD)/oxide install

build: $(OBJ)
	$(MAKE) --output-sync=target -C $(BUILD)/oxide all

package: REV="~r$(shell git rev-list --count HEAD).$(shell git rev-parse --short HEAD)"
package: version.txt $(DIST) $(BUILD)/package/oxide.tar.gz
	toltecmk \
		--verbose \
		-w $(BUILD)/package/build \
		-d $(BUILD)/package/dist \
		$(BUILD)/package
	cp -a $(BUILD)/package/dist/rmall/*.ipk $(DIST)

version.txt:
	if [ -d .git ];then \
		echo $(REV) > version.txt; \
	else \
		echo "~manual" > version.txt; \
	fi;

$(DIST):
	mkdir -p $(DIST)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/.nobackup: $(BUILD)
	touch $(BUILD)/.nobackup

$(BUILD)/oxide: $(BUILD)/.nobackup
	mkdir -p $(BUILD)/oxide

$(BUILD)/oxide/Makefile: $(BUILD)/oxide
	cd $(BUILD)/oxide && qmake -r $(DEFINES) $(CURDIR)

$(BUILD)/package:
	mkdir -p $(BUILD)/package
	rm -rf $(BUILD)/package/build

$(BUILD)/package/package: $(BUILD)/package
	sed "s/~VERSION~/`cat version.txt`/" ./package > $(BUILD)/package/package

PKG_OBJ = oxide.pro Makefile
PKG_OBJ += $(wildcard applications/**)
PKG_OBJ += $(wildcard assets/**)
PKG_OBJ += $(wildcard interfaces/**)
PKG_OBJ += $(wildcard qmake/**)
PKG_OBJ += $(wildcard shared/**)
PKG_OBJ += $(wildcard tests/**)

$(BUILD)/package/oxide.tar.gz: $(BUILD)/package/package $(PKG_OBJ)
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

SRC_FILES = $(shell find -name '*.sh' | grep -v shared/sentry | grep -v shared/doxygen-awesome-css)
SRC_FILES += package

lint:
	shfmt \
		-d\
		-s \
		-i 4 \
		-bn \
		-sr \
		$(SRC_FILES)

format:
	shfmt \
		-l \
		-w \
		-s \
		-i 4 \
		-bn \
		-sr \
		$(SRC_FILES)
