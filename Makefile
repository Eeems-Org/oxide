.PHONY: all clean release build sentry

all: release

.NOTPARALLEL:

# FEATURES += sentry

ifneq ($(filter sentry,$(FEATURES)),)
OBJ += sentry
RELOBJ += release/opt/lib/libsentry.so
DEFINES += 'DEFINES+="SENTRY"'
endif

OBJ += _make
RELOBJ += _install

clean:
	rm -rf .build
	rm -rf release

release: clean build $(RELOBJ)

build: $(OBJ)

sentry: .build/sentry/libsentry.so

_make: .build/oxide/Makefile
	$(MAKE) -j`nproc` -C .build/oxide all

_install: _make
	mkdir -p release
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/oxide install

.build/oxide:
	mkdir -p .build/oxide

.build/oxide/Makefile: .build/oxide
	cd .build/oxide && qmake -r $(DEFINES) ../../oxide.pro

.build/sentry/libsentry.so:
	cd shared/sentry && cmake -B ../../.build/sentry/src \
		-DBUILD_SHARED_LIBS=ON \
		-DSENTRY_INTEGRATION_QT=ON \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DSENTRY_PIC=OFF \
		-DSENTRY_BACKEND=breakpad \
		-DSENTRY_BREAKPAD_SYSTEM=OFF \
		-DSENTRY_EXPORT_SYMBOLS=ON \
		-DSENTRY_PTHREAD=ON
	cd shared/sentry && cmake --build ../../.build/sentry/src --parallel
	cd shared/sentry && cmake --install ../../.build/sentry/src --prefix ../../.build/sentry --config RelWithDebInfo

release/opt/lib/libsentry.so: sentry
	mkdir -p release/opt/lib
	cp .build/sentry/lib/libsentry.so release/opt/lib/
