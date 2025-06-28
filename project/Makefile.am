PROJECT_ROOT_DIR = ..
PROJECT_ROOT_DIR2 = ../../../..
PROJECT_INCLUDE_DIR = include
PROJECT_SRC_DIR = src
PACKAGES_ARCH_DIR = packages
PATCHES_DIR = $(PACKAGES_ARCH_DIR)/patches
BUILD_DIR = build
PACKAGES_DIR = $(BUILD_DIR)/packages
STAGING_DIR = staging_dir
SCRIPTS_DIR = scripts
INSTALL_SCRIPT = install.sh
UNINSTALL_SCRIPT = uninstall.sh

ZLIB_VER = 1.3.1
OPENSSL_VER = 3.0.16

ZLIB_PACKAGE_BASE = $(PACKAGES_DIR)/zlib-$(ZLIB_VER)
OPENSSL_PACKAGE_BASE = $(PACKAGES_DIR)/openssl-$(OPENSSL_VER)

ALL_PATCHES := $(wildcard $(PATCHES_DIR)/*.patch)
ALL_PATCHES_NAMES := $(basename $(notdir $(ALL_PATCHES)))

PACKAGES_ARCH_SUFFIX = .tar.gz
ALL_PACKAGES := $(wildcard $(PACKAGES_ARCH_DIR)/*$(PACKAGES_ARCH_SUFFIX))
ALL_PACKAGES_NAMES := $(basename $(basename $(notdir $(ALL_PACKAGES))))


AM_CXXFLAGS = -O2
AM_CPPFLAGS = -I$(PROJECT_INCLUDE_DIR) \
              -I$(PACKAGES_DIR)/toml11/toml11/include \
              -I$(PACKAGES_DIR)/httpparser/httpparser/src/httpparser \
              -I$(OPENSSL_PACKAGE_BASE)/$(STAGING_DIR)/include \
              -I$(ZLIB_PACKAGE_BASE)/$(STAGING_DIR)/include
              
AM_LDFLAGS = -L$(ZLIB_PACKAGE_BASE)/$(STAGING_DIR)/lib -L$(OPENSSL_PACKAGE_BASE)/$(STAGING_DIR)/lib64
LDADD = -Wl,-Bstatic -lz -lssl -lcrypto
LDADD += -Wl,-Bdynamic -lpthread -lsystemd


SRC_FILES = \
	src/Codec.cpp \
	src/Configuration.cpp \
	src/Http1_0.cpp \
	src/Http1_1.cpp \
	src/Http2_0.cpp \
	src/Http.cpp \
	src/HttpGlobal.cpp \
	src/HttpPacketBuilderBase.cpp \
	src/HttpPacketBuilder.cpp \
	src/HttpPacketBase.cpp \
	src/HttpPacket.cpp \
	src/TcpServer.cpp \
	src/ThreadPool.cpp \
	src/SslConfig.cpp \
	src/WebServer.cpp \
	src/WebServerd.cpp \
	src/WebServerError.cpp

# WebServerd_SOURCES =
OBJS = $(addprefix $(BUILD_DIR)/, $(patsubst src/%.cpp, %.o, $(SRC_FILES)))
# bin_PROGRAMS = $(BUILD_DIR)/WebServerd$(EXEEXT)


.PHONY: pkgs init_packages extract_packages apply_patches zlib openssl install uninstall build clean_build clean_pkgs

all-local: pkgs build

build: $(BUILD_DIR)/WebServerd$(EXEEXT)
$(BUILD_DIR)/%.o: $(PROJECT_SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) $(AM_CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/WebServerd$(EXEEXT): $(OBJS)
	$(CXX) $(LDFLAGS) $(AM_LDFLAGS) -o $@ $(OBJS) $(LDADD)


pkgs: init_packages extract_packages apply_patches zlib openssl
init_packages:
	@mkdir -p $(PACKAGES_DIR)
	@echo "Creating package directories..."
	@for pckg in $(ALL_PACKAGES_NAMES); do \
		mkdir -p "$(PACKAGES_DIR)/$$pckg"; \
		mkdir -p "$(PACKAGES_DIR)/$$pckg/$(STAGING_DIR)"; \
	done

extract_packages:
	@echo "Extracting package archives..."
	@for pckg in $(ALL_PACKAGES_NAMES); do \
		tar -xzf "$(PACKAGES_ARCH_DIR)/$$pckg$(PACKAGES_ARCH_SUFFIX)" -C "$(PACKAGES_DIR)/$$pckg"; \
	done

apply_patches:
	@echo "Applying patches..." 
	@for patch_name in $(ALL_PATCHES_NAMES); do \
		cd "$(PACKAGES_DIR)/$$patch_name/$$patch_name" && cat "$(PROJECT_ROOT_DIR2)/$(PATCHES_DIR)/$$patch_name.patch" | patch -p1; \
	done

zlib:
	@echo "Building zlib..."
	@cd $(ZLIB_PACKAGE_BASE)/zlib-$(ZLIB_VER) && \
	CC=$(CC) CFLAGS="$(CFLAGS)" \
	./configure --prefix=../$(STAGING_DIR) && \
	$(MAKE) && \
	$(MAKE) install

openssl:
	@echo "Building OpenSSL..."
	@cd $(OPENSSL_PACKAGE_BASE)/openssl-$(OPENSSL_VER) && \
	CC=$(CC) CFLAGS="$(CFLAGS)" \
	./Configure --prefix=$(shell pwd)/$(OPENSSL_PACKAGE_BASE)/$(STAGING_DIR) \
	no-ssl2 no-comp no-zlib && \
	$(MAKE) && \
	$(MAKE) install
	
install:
	@echo "Installing..."
	cd $(SCRIPTS_DIR) && ./$(INSTALL_SCRIPT)

uninstall:
	@echo "Uninstalling..."
	cd $(SCRIPTS_DIR) && ./$(UNINSTALL_SCRIPT)

clean_build:
	@echo "Cleaning build directory..."
	@cd $(BUILD_DIR) && ls -A | grep -Ev '^(packages|\.gitkeep)$$' | xargs rm -rf

clean_pkgs:
	@echo "Cleaning package directories..."
	@rm -rf $(PACKAGES_DIR)
	
clean-local: clean_build clean_pkgs

