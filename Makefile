.PHONY: help build clean test dev pair log key key-base64 props

KEYDNA = CN=Juergen Hock, O=Voicesmith
KEYARG = -keyalg RSA -keysize 2048 -validity 12345

SDK ?= $(or $(ANDROID_HOME),$(ANDROID_SDK_ROOT),~/Library/Android/sdk)
ADB = $(SDK)/platform-tools/adb

HOST ?= $(shell bash -c 'read -p "HOST> " HOST; echo 192.168.178.$$HOST')
PORT ?= $(shell bash -c 'read -p "PORT> " PORT; echo $$PORT')
CODE ?= $(shell bash -c 'read -p "CODE> " CODE; echo $$CODE')

help:
	@echo build
	@echo clean
	@echo test
	@echo dev
	@echo pair
	@echo log
	@echo key
	@echo key-base64
	@echo props

build:
	@./gradlew assembleRelease lintRelease

clean:
	@./gradlew clean

test:
	@cmake -S tests/native -B build/native -DCMAKE_BUILD_TYPE=Debug
	@cmake --build build/native
	@ctest --test-dir build/native --output-on-failure

dev:
	@$(ADB) devices

pair:
	@$(ADB) pair $(HOST):$(PORT) $(CODE)

log:
	@$(ADB) logcat -c
	@$(ADB) logcat -v color voicesmith.java:D voicesmith.cpp:D *:S

key:
	@keytool -genkeypair -keystore secret.keystore -alias github-jurihock-voicesmith -dname '$(KEYDNA)' $(KEYARG)

key-base64:
	@base64 < secret.keystore | tr -d '\n'

props:
	@realpath $(SDK) | sed 's/^/sdk.dir=/' > local.properties
