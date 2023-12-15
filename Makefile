.PHONY: all clean release build sentry package

all: release

.NOTPARALLEL:

MAKEFLAGS := --jobs=$(shell nproc)

# FEATURES += sentry

DIST=$(CURDIR)/release
BUILD=$(CURDIR)/.build


ifneq ($(filter sentry,$(FEATURES)),)
OBJ += sentry
RELOBJ += $(DIST)/opt/lib/libsentry.so
DEFINES += 'DEFINES+="SENTRY"'
endif

OBJ += $(BUILD)/oxide/Makefile

clean:
	rm -rf $(DIST) $(BUILD)

release: clean build $(RELOBJ)
	mkdir -p $(DIST)
	# Force liboxide makefile to regenerate so that install targets get when being build in toltecmk
	cd $(BUILD)/oxide/shared/liboxide && make qmake
	INSTALL_ROOT=$(DIST) $(MAKE) --output-sync=target -C $(BUILD)/oxide install

build: $(BUILD) $(OBJ)
	$(MAKE) --output-sync=target -C $(BUILD)/oxide all

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

sentry: $(BUILD)/sentry/libsentry.so

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/.nobackup: $(BUILD)
	touch $(BUILD)/.nobackup

$(BUILD)/oxide: $(BUILD)/.nobackup
	mkdir -p $(BUILD)/oxide

$(BUILD)/oxide/Makefile: $(BUILD)/oxide
	cd $(BUILD)/oxide && qmake -r $(DEFINES) $(CURDIR)/oxide.pro

$(BUILD)/sentry/libsentry.so: $(BUILD)/.nobackup
	cd shared/sentry && cmake -B $(BUILD)/sentry/src \
		-DBUILD_SHARED_LIBS=ON \
		-DSENTRY_INTEGRATION_QT=ON \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DSENTRY_PIC=OFF \
		-DSENTRY_BACKEND=breakpad \
		-DSENTRY_BREAKPAD_SYSTEM=OFF \
		-DSENTRY_EXPORT_SYMBOLS=ON \
		-DSENTRY_PTHREAD=ON
	cd shared/sentry && cmake --build $(BUILD)/sentry/src --parallel
	cd shared/sentry && cmake --install $(BUILD)/sentry/src --prefix $(BUILD)/sentry --config RelWithDebInfo

$(DIST)/opt/lib/libsentry.so: sentry
	mkdir -p $(DIST)/opt/lib
	cp -a $(BUILD)/sentry/lib/libsentry.so $(DIST)/opt/lib/
