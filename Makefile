all: build examples docker-test

.PHONY: docker-env
docker-env:
	cd docker-toolchain && docker build \
		--build-arg UNAME=builder \
		--build-arg UID=$(shell id -u) \
		--build-arg GID=$(shell id -g) \
		--build-arg ostype=${shell uname} \
		--tag rust-build-remarkable:latest .

examples: docker-env
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v $(shell cygpath -w `pwd`):/home/builder/oxidize:rw \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo build --examples --release --target=armv7-unknown-linux-gnueabihf

build: docker-env
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v $(shell cygpath -w `pwd`):/home/builder/oxidize:rw \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo build --release --verbose --target=armv7-unknown-linux-gnueabihf

test: docker-env
	docker volume create cargo-registry
	docker run \
		--rm \
		--user builder \
		-v $(shell cygpath -w `pwd`):/home/builder/oxidize:rw \
		-v cargo-registry:/home/builder/.cargo/registry \
		-w /home/builder/oxidize \
		rust-build-remarkable:latest \
		cargo test

# DEVICE_IP ?= "10.11.99.1"
DEVICE_IP ?= "192.168.0.34"
# DEVICE_SERVICE ?= "xochitl"
DEVICE_SERVICE ?= "draft"
deploy:
	ssh root@$(DEVICE_IP) 'killall oxidize || true; systemctl stop $(DEVICE_SERVICE) || true'
	scp ./target/armv7-unknown-linux-gnueabihf/release/oxidize root@$(DEVICE_IP):~/

exec:
	ssh root@$(DEVICE_IP) './oxidize'
	ssh root@$(DEVICE_IP) 'systemctl restart $(DEVICE_SERVICE) || true'

run: build deploy exec
