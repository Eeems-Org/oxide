all: release

# FEATURES += sentry

ifneq ($(filter sentry,$(FEATURES)),)
OBJ += .build/sentry/libsentry.so
RELOBJ += release/opt/lib/libsentry.so
DEFINES += 'DEFINES+="SENTRY"'
endif

clean:
	rm -rf .build
	rm -rf release

release: clean build $(RELOBJ)
	mkdir -p release
	INSTALL_ROOT=../../release $(MAKE) -C .build/process-manager install
	INSTALL_ROOT=../../release $(MAKE) -C .build/system-service install
	INSTALL_ROOT=../../release $(MAKE) -C .build/settings-manager install
	INSTALL_ROOT=../../release $(MAKE) -C .build/screenshot-tool install
	INSTALL_ROOT=../../release $(MAKE) -C .build/screenshot-viewer install
	INSTALL_ROOT=../../release $(MAKE) -C .build/launcher install
	INSTALL_ROOT=../../release $(MAKE) -C .build/lockscreen install
	INSTALL_ROOT=../../release $(MAKE) -C .build/task-switcher install

build: tarnish erode rot oxide decay corrupt fret anxiety

erode: $(OBJ)
	mkdir -p .build/process-manager
	cp -r applications/process-manager/* .build/process-manager
	cd .build/process-manager && qmake $(DEFINES) erode.pro
	$(MAKE) -C .build/process-manager all

tarnish: $(OBJ)
	mkdir -p .build/system-service
	cp -r applications/system-service/* .build/system-service
	cd .build/system-service && qmake $(DEFINES) tarnish.pro
	$(MAKE) -C .build/system-service all

rot: tarnish $(OBJ)
	mkdir -p .build/settings-manager
	cp -r applications/settings-manager/* .build/settings-manager
	cd .build/settings-manager && qmake $(DEFINES) rot.pro
	$(MAKE) -C .build/settings-manager all

fret: tarnish $(OBJ)
	mkdir -p .build/screenshot-tool
	cp -r applications/screenshot-tool/* .build/screenshot-tool
	cd .build/screenshot-tool && qmake $(DEFINES) fret.pro
	$(MAKE) -C .build/screenshot-tool all

oxide: tarnish $(OBJ)
	mkdir -p .build/launcher
	cp -r applications/launcher/* .build/launcher
	cd .build/launcher && qmake $(DEFINES) oxide.pro
	$(MAKE) -C .build/launcher all

decay: tarnish $(OBJ)
	mkdir -p .build/lockscreen
	cp -r applications/lockscreen/* .build/lockscreen
	cd .build/lockscreen && qmake $(DEFINES) decay.pro
	$(MAKE) -C .build/lockscreen all

corrupt: tarnish $(OBJ)
	mkdir -p .build/task-switcher
	cp -r applications/task-switcher/* .build/task-switcher
	cd .build/task-switcher && qmake $(DEFINES) corrupt.pro
	$(MAKE) -C .build/task-switcher all
	
anxiety: tarnish $(OBJ)
	mkdir -p .build/screenshot-viewer
	cp -r applications/screenshot-viewer/* .build/screenshot-viewer
	cd .build/screenshot-viewer && qmake $(DEFINES) anxiety.pro
	$(MAKE) -C .build/screenshot-viewer all

.build/sentry/libsentry.so:
	cd shared/sentry && cmake -B ../../.build/sentry \
		-DBUILD_SHARED_LIBS=ON \
		-DSENTRY_INTEGRATION_QT=ON \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DSENTRY_PIC=OFF -DSENTRY_BACKEND=breakpad \
		-DSENTRY_BREAKPAD_SYSTEM=OFF
	cd shared/sentry && cmake --build ../../.build/sentry --parallel
	cd shared/sentry && cmake --install ../../.build/sentry --prefix ../../.build/sentry --config RelWithDebInfo

release/opt/lib/libsentry.so: .build/sentry/libsentry.so
	mkdir -p release/opt/lib
	cp .build/sentry/libsentry.so release/opt/lib/
