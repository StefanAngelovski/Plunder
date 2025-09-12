APP_LABEL = Plunder
APP_DESCRIPTION = Rom store, crawler and scraper for several rom sites.

APP_LABEL_SLUG = $(shell echo "$(APP_LABEL)" | tr ' ' '_')
BUILD_DIR = build/$(APP_LABEL_SLUG)
OUT = $(BUILD_DIR)/$(APP_LABEL_SLUG)

TOOLCHAIN_DIR = /sdk/aarch64-linux-gnu-7.5.0-linaro
SYSROOT_DIR = /sdk/usr
SDL_DIR = /sdk/SDL2-2.26.1

# Compiler Setup
CXX = $(TOOLCHAIN_DIR)/bin/aarch64-linux-gnu-g++
CXXFLAGS = -I$(SDL_DIR)/include -I$(SYSROOT_DIR)/include -I.
LDFLAGS = -L$(SYSROOT_DIR)/lib -L$(SYSROOT_DIR)/lib/mali -Wl,-rpath-link=$(SYSROOT_DIR)/lib/mali
LIBS = -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_mixer -lmad -lfreetype -lz -lbz2 -lGLESv2 -lEGL -lIMGegl -lsrv_um -lusc -lglslcompiler -lm -lpthread -ljson-c

SRC = $(shell find src -name '*.cpp')

all: $(OUT) copy_resources

$(OUT): $(SRC)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(SRC) -o $(OUT) $(CXXFLAGS) $(LDFLAGS) $(LIBS)

copy_resources:
	mkdir -p build/Plunder/res
	cp res/blacklist.txt build/Plunder/res/blacklist.txt
	cp res/icon.png $(BUILD_DIR)/icon.png

	# Create config.json
	echo '{' > $(BUILD_DIR)/config.json
	echo '  "label":"$(APP_LABEL)",' >> $(BUILD_DIR)/config.json
	echo '  "icon":"icon.png",' >> $(BUILD_DIR)/config.json
	echo '  "iconsel":"icon.png",' >> $(BUILD_DIR)/config.json
	echo '  "launch":"launch.sh",' >> $(BUILD_DIR)/config.json
	echo '  "description":"$(APP_DESCRIPTION)"' >> $(BUILD_DIR)/config.json
	echo '}' >> $(BUILD_DIR)/config.json

	cp res/launch.sh $(BUILD_DIR)/launch.sh
	chmod +x $(BUILD_DIR)/launch.sh

	# Copy font into res folder
	mkdir -p $(BUILD_DIR)/res
	cp res/InterVariable.ttf $(BUILD_DIR)/res/

	# Copy sound effect for cancel button
	mkdir -p $(BUILD_DIR)/sounds
	cp -r src/sounds/* $(BUILD_DIR)/sounds/

	# Copy videos folder and contents
	mkdir -p $(BUILD_DIR)/videos
	cp -r src/videos/* $(BUILD_DIR)/videos/

	# Copy images folder and contents
	mkdir -p $(BUILD_DIR)/images
	cp -r src/images/* $(BUILD_DIR)/images/

	# Copy lib folder and contents
	mkdir -p $(BUILD_DIR)/lib
	cp -r src/lib/* $(BUILD_DIR)/lib/

	# Copy bin folder and contents
	mkdir -p $(BUILD_DIR)/bin
	cp -r src/bin/* $(BUILD_DIR)/bin/

clean:
	rm -rf $(BUILD_DIR)