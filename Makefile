.PHONY: all clean release build liboxide erode tarnish rot fret oxide decay corrupt anxiety sentry

all: release

.NOTPARALLEL:

# FEATURES += sentry

ifneq ($(filter sentry,$(FEATURES)),)
OBJ += sentry
RELOBJ += release/opt/lib/libsentry.so
DEFINES += 'DEFINES+="SENTRY"'
endif

clean:
	rm -rf .build
	rm -rf release

release: clean build $(RELOBJ)
	mkdir -p release
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/liboxide install
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/process-manager install
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/system-service install
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/settings-manager install
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/screenshot-tool install
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/screenshot-viewer install
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/launcher install
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/lockscreen install
	INSTALL_ROOT=../../release $(MAKE) -j`nproc` -C .build/task-switcher install

build: liboxide tarnish erode rot oxide decay corrupt fret anxiety

liboxide: $(OBJ) .build/liboxide/libliboxide.so

.build/liboxide/libliboxide.so:
	mkdir -p .build/liboxide
	cp -r shared/liboxide/* .build/liboxide
	cd .build/liboxide && qmake $(DEFINES) liboxide.pro
	$(MAKE) -j`nproc` -C .build/liboxide all

erode: liboxide .build/process-manager/erode

.build/process-manager/erode:
	mkdir -p .build/process-manager
	cp -r applications/process-manager/* .build/process-manager
	cd .build/process-manager && qmake $(DEFINES) erode.pro
	$(MAKE) -j`nproc` -C .build/process-manager all

tarnish: liboxide .build/system-service/tarnish

.build/system-service/tarnish:
	mkdir -p .build/system-service
	cp -r applications/system-service/* .build/system-service
	cd .build/system-service && qmake $(DEFINES) tarnish.pro
	$(MAKE) -j`nproc` -C .build/system-service all

rot: tarnish liboxide .build/settings-manager/rot

.build/settings-manager/rot:
	mkdir -p .build/settings-manager
	cp -r applications/settings-manager/* .build/settings-manager
	cd .build/settings-manager && qmake $(DEFINES) rot.pro
	$(MAKE) -j`nproc` -C .build/settings-manager all

fret: tarnish liboxide .build/screenshot-tool/fret

.build/screenshot-tool/fret:
	mkdir -p .build/screenshot-tool
	cp -r applications/screenshot-tool/* .build/screenshot-tool
	cd .build/screenshot-tool && qmake $(DEFINES) fret.pro
	$(MAKE) -j`nproc` -C .build/screenshot-tool all

oxide: tarnish liboxide .build/launcher/oxide

.build/launcher/oxide:
	mkdir -p .build/launcher
	cp -r applications/launcher/* .build/launcher
	cd .build/launcher && qmake $(DEFINES) oxide.pro
	$(MAKE) -j`nproc` -C .build/launcher all

decay: tarnish liboxide .build/lockscreen/decay

.build/lockscreen/decay:
	mkdir -p .build/lockscreen
	cp -r applications/lockscreen/* .build/lockscreen
	cd .build/lockscreen && qmake $(DEFINES) decay.pro
	$(MAKE) -j`nproc` -C .build/lockscreen all

corrupt: tarnish liboxide .build/task-switcher/corrupt

.build/task-switcher/corrupt:
	mkdir -p .build/task-switcher
	cp -r applications/task-switcher/* .build/task-switcher
	cd .build/task-switcher && qmake $(DEFINES) corrupt.pro
	$(MAKE) -j`nproc` -C .build/task-switcher all

anxiety: tarnish liboxide .build/screenshot-viewer/anxiety

.build/screenshot-viewer/anxiety:
	mkdir -p .build/screenshot-viewer
	cp -r applications/screenshot-viewer/* .build/screenshot-viewer
	cd .build/screenshot-viewer && qmake $(DEFINES) anxiety.pro
	$(MAKE) -j`nproc` -C .build/screenshot-viewer all

sentry: .build/sentry/libsentry.so

.build/sentry/libsentry.so:
	cd shared/sentry && cmake -B ../../.build/sentry \
		-DBUILD_SHARED_LIBS=ON \
		-DSENTRY_INTEGRATION_QT=ON \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DSENTRY_PIC=OFF -DSENTRY_BACKEND=breakpad \
		-DSENTRY_BREAKPAD_SYSTEM=OFF \
		-DSENTRY_PERFORMANCE_MONITORING=ON
	cd shared/sentry && cmake --build ../../.build/sentry --parallel
	cd shared/sentry && cmake --install ../../.build/sentry --prefix ../../.build/sentry --config RelWithDebInfo

release/opt/lib/libsentry.so: sentry
	mkdir -p release/opt/lib
	cp .build/sentry/libsentry.so release/opt/lib/
