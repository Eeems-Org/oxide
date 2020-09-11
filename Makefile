all: release

DEVICE_IP ?= "10.11.99.1"
# DEVICE_IP ?= "192.168.0.34"

SSH_DIR ?= $(HOME)/.ssh
# SSH_DIR ?= $(shell cygpath -w ~/.ssh)
# SSH_DIR ?= G:\cygwin64\home\Eeems\.ssh

QTCREATOR_RUN_ARGS ?= --gpus all
# QTCREATOR_RUN_ARGS ?=

ifeq ($(OS),Windows_NT)
	BUILD_DIR ?= $(shell cygpath -w `pwd`)
	DISPLAY ?= host.docker.internal:0.0
	DISPLAY_ARGS ?=
else
	BUILD_DIR ?= $(shell pwd)
	DISPLAY ?= unix$(DISPLAY)
	DISPLAY_ARGS ?= -v /tmp/.X11-unix:/tmp/.X11-unix --device /dev/snd
endif


.PHONY: docker-qtcreator
docker-qtcreator:
	cd docker-toolchain/qtcreator && docker build \
		--tag rm-qtcreator .

qtcreator: docker-qtcreator
	xhost +SI:localuser:root > /dev/null
	docker start -a qtcreator || \
	docker run \
		$(QTCREATOR_RUN_ARGS) \
		--name qtcreator \
		-v '$(BUILD_DIR):/root/project:rw' \
		-v '$(SSH_DIR):/root/.ssh' \
		-v '$(BUILD_DIR)/docker-toolchain/qtcreator/files/config:/root/.config' \
		-w /root/project \
		-e DISPLAY=$(DISPLAY) \
		$(DISPLAY_ARGS) \
		rm-qtcreator:latest

clean:
	rm -rf .build
	rm -rf release

release: erode tarnish rot oxide
	mkdir -p release
	INSTALL_ROOT=../../release $(MAKE) -C .build/erode install
	INSTALL_ROOT=../../release $(MAKE) -C .build/tarnish install
	INSTALL_ROOT=../../release $(MAKE) -C .build/rot install
	INSTALL_ROOT=../../release $(MAKE) -C .build/oxide install

erode:
	mkdir -p .build/erode
	cp -r applications/process-manager/* .build/erode
	cd .build/erode && qmake erode.pro
	$(MAKE) -C .build/erode all

tarnish: erode
	mkdir -p .build/tarnish
	cp -r applications/system-service/* .build/tarnish
	cd .build/tarnish && qmake tarnish.pro
	$(MAKE) -C .build/tarnish all

rot: tarnish
	mkdir -p .build/rot
	cp -r applications/settings-manager/* .build/rot
	cd .build/rot && qmake rot.pro
	$(MAKE) -C .build/rot all

oxide: tarnish
	mkdir -p .build/oxide
	cp -r applications/launcher/* .build/oxide
	cd .build/oxide && qmake oxide.pro
	$(MAKE) -C .build/oxide all
