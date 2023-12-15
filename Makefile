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
	# Force sentry makefile to regenerate so that install targets get when being build in toltecmk
	cd $(BUILD)/oxide/shared/sentry && make qmake
	# Force liboxide makefile to regenerate so that install targets get when being build in toltecmk
	cd $(BUILD)/oxide/shared/liboxide && make qmake
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

$(BUILD)/package/oxide.tar.gz: $(BUILD)/package/package
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
