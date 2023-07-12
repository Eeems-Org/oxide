.PHONY: all clean release build sentry package

all: release

.NOTPARALLEL:

# FEATURES += sentry

DIST=$(CURDIR)/release
BUILD=$(CURDIR)/.build


ifneq ($(filter sentry,$(FEATURES)),)
DEFINES += 'DEFINES+="SENTRY"'
endif

OBJ += $(BUILD)/oxide/Makefile

clean:
	rm -rf $(DIST) $(BUILD)

release: clean build $(RELOBJ)
	mkdir -p $(DIST)
	INSTALL_ROOT=$(DIST) $(MAKE) -C $(BUILD)/oxide install

build: $(BUILD) $(OBJ)
	$(MAKE) -C $(BUILD)/oxide all

package: REV="~r$(shell git rev-list --count HEAD).$(shell git rev-parse --short HEAD)"
package:
	if [ -d .git ];then \
		echo $(REV) > version.txt; \
	else \
		echo "~manual" > version.txt; \
	fi;
	mkdir -p $(BUILD)/package
	rm -rf $(BUILD)/package/build
	sed "s/~VERSION~/`cat version.txt`/" ./package > $(BUILD)/package/package
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
		oxide.pro \
		Makefile
	toltecmk \
		--verbose \
		-w $(BUILD)/package/build \
		-d $(BUILD)/package/dist \
		$(BUILD)/package
	mkdir -p $(DIST)
	cp -a $(BUILD)/package/dist/rmall/*.ipk $(DIST)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/.nobackup: $(BUILD)
	touch $(BUILD)/.nobackup

$(BUILD)/oxide: $(BUILD)/.nobackup
	mkdir -p $(BUILD)/oxide

$(BUILD)/oxide/Makefile: $(BUILD)/oxide
	cd $(BUILD)/oxide && qmake -r $(DEFINES) $(CURDIR)/oxide.pro
