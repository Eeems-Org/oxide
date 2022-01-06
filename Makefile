all: release

clean:
	rm -rf .build
	rm -rf release

release: clean
	$(MAKE) build
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

erode:
	mkdir -p .build/process-manager
	cp -r applications/process-manager/* .build/process-manager
	cd .build/process-manager && qmake erode.pro
	$(MAKE) -C .build/process-manager all

tarnish:
	mkdir -p .build/system-service
	cp -r applications/system-service/* .build/system-service
	cd .build/system-service && qmake tarnish.pro
	$(MAKE) -C .build/system-service all

rot: tarnish
	mkdir -p .build/settings-manager
	cp -r applications/settings-manager/* .build/settings-manager
	cd .build/settings-manager && qmake rot.pro
	$(MAKE) -C .build/settings-manager all

fret: tarnish
	mkdir -p .build/screenshot-tool
	cp -r applications/screenshot-tool/* .build/screenshot-tool
	cd .build/screenshot-tool && qmake fret.pro
	$(MAKE) -C .build/screenshot-tool all

oxide: tarnish
	mkdir -p .build/launcher
	cp -r applications/launcher/* .build/launcher
	cd .build/launcher && qmake oxide.pro
	$(MAKE) -C .build/launcher all

decay: tarnish
	mkdir -p .build/lockscreen
	cp -r applications/lockscreen/* .build/lockscreen
	cd .build/lockscreen && qmake decay.pro
	$(MAKE) -C .build/lockscreen all

corrupt: tarnish
	mkdir -p .build/task-switcher
	cp -r applications/task-switcher/* .build/task-switcher
	cd .build/task-switcher && qmake corrupt.pro
	$(MAKE) -C .build/task-switcher all
	
anxiety: tarnish
	mkdir -p .build/screenshot-viewer
	cp -r applications/screenshot-viewer/* .build/screenshot-viewer
	cd .build/screenshot-viewer && qmake anxiety.pro
	$(MAKE) -C .build/screenshot-viewer all
