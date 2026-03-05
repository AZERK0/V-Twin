SHELL := /usr/bin/env bash

.PHONY: bootstrap configure build build-debug test run clean

bootstrap:
	./scripts/bootstrap-linux.sh

configure:
	./scripts/configure.sh linux-release

build:
	./scripts/build.sh build-release

build-debug:
	./scripts/configure.sh linux-debug
	./scripts/build.sh build-debug

test:
	./scripts/check.sh

run:
	./scripts/run.sh

clean:
	rm -rf build/linux-release build/linux-debug
