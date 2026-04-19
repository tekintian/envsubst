# envsubst Makefile
# Production-Grade environment variable substitution tool

PACKAGE_NAME := envsubst
PACKAGE_VERSION := v2.1.0

# Installation paths
prefix       ?= /usr/local
bindir       ?= ${prefix}/bin
mandir       ?= ${prefix}/share/man

# Compiler settings
CC           ?= ${CROSS_COMPILE}cc
CFLAGS       ?= -O2 -Wall -Wextra -std=c99 -pedantic
CFLAGS       += -fomit-frame-pointer
CFLAGS       += -DENVSUBST_VERSION='"${PACKAGE_VERSION}"'
LDFLAGS      ?=

# Install utility
INSTALL      ?= install
INSTALL_PROGRAM = ${INSTALL} -m755

# Source files
SRCS         = envsubst.c
OBJS         = ${SRCS:.c=.o}
TARGET       = envsubst

# Distribution
DIST_NAME    = ${PACKAGE_NAME}-${PACKAGE_VERSION}
# Detect system platform and architecture
UNAME_S      := $(shell uname -s | tr '[:upper:]' '[:lower:]')
UNAME_M      := $(shell uname -m)
# Normalize architecture names
ifeq ($(UNAME_M),x86_64)
    ARCH     := x64
else ifeq ($(UNAME_M),aarch64)
    ARCH     := arm64
else ifeq ($(UNAME_M),armv7l)
    ARCH     := arm
else
    ARCH     := $(UNAME_M)
endif
DIST_FULL    = ${DIST_NAME}-${UNAME_S}-${ARCH}
DIST_TARBALL = ${DIST_FULL}.tar.gz

# Default target
all: ${TARGET}

# Build target
${TARGET}: ${OBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o $@ ${OBJS}

# Compile source files
%.o: %.c
	${CC} ${CFLAGS} -c $< -o $@

# Clean build artifacts
clean:
	${RM} -f ${OBJS} ${TARGET}
	${RM} -f *.o

# Install to system
install: all
	${INSTALL} -d ${DESTDIR}${bindir}
	${INSTALL_PROGRAM} ${TARGET} ${DESTDIR}${bindir}/${TARGET}
	@echo "Installed ${TARGET} to ${DESTDIR}${bindir}/"

# Uninstall from system
uninstall:
	${RM} -f ${DESTDIR}${bindir}/${TARGET}
	@echo "Uninstalled ${TARGET} from ${DESTDIR}${bindir}/"

# Run tests
test: all
	@echo "Running comprehensive test suite..."
	@chmod +x test_all_features.sh
	@./test_all_features.sh

# Check (alias for test)
check: test

# Show help
help:
	@echo "envsubst Makefile - Available targets:"
	@echo ""
	@echo "  all/build    - Build the project (default)"
	@echo "  clean        - Remove build artifacts"
	@echo "  install      - Install to system (prefix=${prefix})"
	@echo "  uninstall    - Remove from system"
	@echo "  test/check   - Run tests"
	@echo "  dist         - Create distribution tarball"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  CC           - C compiler (default: ${CC})"
	@echo "  CFLAGS       - Compiler flags (default: ${CFLAGS})"
	@echo "  prefix       - Installation prefix (default: ${prefix})"
	@echo "  DESTDIR      - Destination directory for packaging"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build"
	@echo "  make install            # Install to /usr/local/bin"
	@echo "  make prefix=/opt        # Install to /opt/bin"
	@echo "  make DESTDIR=/tmp/pkg   # Install to /tmp/pkg/usr/local/bin"
	@echo "  make test               # Run tests"

# Create distribution tarball
dist: clean
	@echo "Creating distribution package..."
	@echo "Platform: ${UNAME_S}"
	@echo "Architecture: ${ARCH}"
	@mkdir -p ${DIST_FULL}
	@cp -r *.c *.h Makefile README.md test.sh ${DIST_FULL}/ 2>/dev/null || true
	@tar czf ${DIST_TARBALL} ${DIST_FULL}
	@rm -rf ${DIST_FULL}
	@echo "Created ${DIST_TARBALL}"

# Distribution check
distcheck: dist
	@echo "Testing distribution package..."
	@mkdir -p /tmp/${DIST_FULL}-test
	@tar xzf ${DIST_TARBALL} -C /tmp/${DIST_FULL}-test
	@cd /tmp/${DIST_FULL}-test/${DIST_FULL} && make && make test
	@rm -rf /tmp/${DIST_FULL}-test
	@echo "Distribution check passed!"

# Phony targets (not real files)
.PHONY: all clean install uninstall test check help dist distcheck
