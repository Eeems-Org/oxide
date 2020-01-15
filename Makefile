all: build examples docker-test

DEVICE_IP ?= "10.11.99.1"
# DEVICE_IP ?= "192.168.0.34"

# DEVICE_SERVICE ?= "xochitl"
DEVICE_SERVICE ?= "draft"

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

.PHONY: docker-cargo
docker-cargo:
	cd docker-toolchain/cargo && docker build \
		--build-arg UNAME=builder \
		--build-arg UID=$(shell id -u) \
		--build-arg GID=$(shell id -g) \
		--build-arg ostype=${shell uname} \
		--tag rust-build-remarkable:latest .

.PHONY: docker-qtcreator
docker-qtcreator:
	cd docker-toolchain/qtcreator && docker build \
		--tag rm-qtcreator .

examples: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v '$(BUILD_DIR):/home/builder/oxidize:rw' \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo build --examples --release --target=armv7-unknown-linux-gnueabihf

build: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v '$(BUILD_DIR):/home/builder/oxidize:rw' \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo build --release --verbose --target=armv7-unknown-linux-gnueabihf

build-applications: docker-cargo
	ls applications | while read APP; do \
		if [ -f "$${APP}/Cargo.toml" ];then \
			echo "Building $${APP}"; \
			docker volume create cargo-registry; \
			docker run \
				--rm \
				--user builder \
				-v "$(BUILD_DIR)/applications/$${APP}:/home/builder/$${APP}:rw" \
				-v cargo-registry:/home/builder/.cargo/registry \
				-w /home/builder/$${APP} \
				rust-build-remarkable:latest \
				cargo build --release --verbose --target=armv7-unknown-linux-gnueabihf; \
		fi; \
	done

test: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v '$(BUILD_DIR):/home/builder/oxidize:rw' \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo test

check: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v '$(BUILD_DIR):/home/builder/oxidize:rw' \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo check

check-applications: docker-cargo $(BUILD_DIR)/applications/*
	ls applications | while read APP; do \
		if [ -f "$${APP}/Cargo.toml" ];then \
			docker volume create cargo-registry
			docker run \
				--rm \
				--user builder \
				-v '$(APP):/home/builder/$(shell basename $(APP)):rw' \
				-v cargo-registry:/home/builder/.cargo/registry \
				-w /home/builder/$(shell basename $(APP)) \
				rust-build-remarkable:latest \
				cargo check \
		fi; \
	done

check-json: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v '$(BUILD_DIR):/home/builder/oxidize:rw' \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo check --message-format=json

deploy:
	ssh root@$(DEVICE_IP) 'killall oxidize || true;'
	scp ./target/armv7-unknown-linux-gnueabihf/release/{oxidize,wifitoggle,corrode} root@$(DEVICE_IP):~/
	scp ./assets/Roboto-NotoEmoji-Regular.ttf root@$(DEVICE_IP):~/font.ttf

exec:
	ssh root@$(DEVICE_IP) 'killall oxidize || true; systemctl stop $(DEVICE_SERVICE) || true'
	ssh root@$(DEVICE_IP) './oxidize'
	ssh root@$(DEVICE_IP) 'systemctl restart $(DEVICE_SERVICE) || true'

run: build deploy exec

qtcreator: docker-qtcreator
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
