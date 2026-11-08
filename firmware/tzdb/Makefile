PY = venv/bin/python
CC = cc
CFLAGS = -std=c99 -O3 -Wall -Wextra -Werror -pedantic
export PYTHONPYCACHEPREFIX := $(CURDIR)/build/pycache
RELEASE ?= 2026f
DATASET ?= timezones
CELL ?= 0.01

.PHONY: all setup fetch tables tzdata test sanitize clean
all: tzdata

venv/.ready: requirements.txt
	python3 -m venv venv
	venv/bin/pip install --no-cache-dir -r requirements.txt
	touch $@

setup: venv/.ready

fetch: venv/.ready
	$(PY) tools/source.py --release $(RELEASE) --dataset $(DATASET)

build/test_lookup: decoder/tz_lookup.c decoder/tz_lookup.h decoder/test_lookup.c
	mkdir -p build
	$(CC) $(CFLAGS) decoder/tz_lookup.c decoder/test_lookup.c -o $@

tables: venv/.ready
	mkdir -p build
	$(PY) tools/build_table.py --release $(RELEASE) --dataset $(DATASET) --cell $(CELL)

tzdata: tables
	wc -c < build/tzdata.bin
	shasum -a 256 build/tzdata.bin

test: tzdata build/test_lookup
	$(PY) tools/test_tables.py --cell $(CELL)

sanitize: tzdata build/test_lookup
	$(CC) -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer decoder/tz_lookup.c decoder/test_lookup.c -o build/test_lookup_sanitized
	TZ_TEST_DRIVER=$(CURDIR)/build/test_lookup_sanitized $(PY) tools/test_tables.py --cell $(CELL) --smoke

clean:
	rm -rf build
