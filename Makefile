all: build examples docker-test

BUILD_DIR ?= $(shell cygpath -w `pwd`)
# BUILD_DIR ?= $(shell pwd)
#
DEVICE_IP ?= "10.11.99.1"
# DEVICE_IP ?= "192.168.0.34"

# DEVICE_SERVICE ?= "xochitl"
DEVICE_SERVICE ?= "draft"

.PHONY: docker-cargo
docker-cargo:
	cd docker-toolchain/cargo && docker build \
		--build-arg UNAME=builder \
		--build-arg UID=$(shell id -u) \
		--build-arg GID=$(shell id -g) \
		--build-arg ostype=${shell uname} \
		--tag rust-build-remarkable:latest .

examples: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v $(BUILD_DIR):/home/builder/oxidize:rw \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo build --examples --release --target=armv7-unknown-linux-gnueabihf

build: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v $(BUILD_DIR):/home/builder/oxidize:rw \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo build --release --verbose --target=armv7-unknown-linux-gnueabihf

test: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v $(BUILD_DIR):/home/builder/oxidize:rw \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo test

check: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v $(BUILD_DIR):/home/builder/oxidize:rw \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo check

check-json: docker-cargo
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v $(BUILD_DIR):/home/builder/oxidize:rw \
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

qtcreator:
	cd docker-toolchain/qtcreator \
	&& docker-compose --file qtcreator/qt.docker-compose.yml up --build
