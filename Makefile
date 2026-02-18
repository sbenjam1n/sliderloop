# Makefile for sliderloop~ external with fixes

NAME = sliderloop
SOURCES = sliderloop.c

# Auto-detect OS
UNAME := $(shell uname -s)

# Platform-specific settings
ifeq ($(UNAME),Darwin)
    # macOS
    EXTENSION = pd_darwin
    # Try to find Pd.app automatically
    PD_APP := $(shell ls -d /Applications/Pd*.app 2>/dev/null | head -1)
    ifeq ($(PD_APP),)
        $(error Pd.app not found in /Applications)
    endif
    PD_INCLUDE = $(PD_APP)/Contents/Resources/src
    CFLAGS = -Wall -W -g -fPIC -DPD -I"$(PD_INCLUDE)"
    # Fixed linker flags for macOS
    LDFLAGS = -bundle -undefined suppress -flat_namespace
else ifeq ($(UNAME),Linux)
    # Linux
    EXTENSION = pd_linux
    PD_INCLUDE = /usr/include/pd
    CFLAGS = -Wall -W -g -fPIC -DPD -I$(PD_INCLUDE)
    LDFLAGS = -shared -fPIC -Wl,-export-dynamic -lm
endif

# Target
TARGET = $(NAME).$(EXTENSION)

# Build rules
all: $(TARGET)

$(TARGET): $(SOURCES)
	@echo "Building for $(UNAME)"
	@echo "Using Pd headers from: $(PD_INCLUDE)"
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	@echo "Installing to ~/Documents/Pd/externals/"
	@mkdir -p ~/Documents/Pd/externals
	cp $(TARGET) ~/Documents/Pd/externals/

info:
	@echo "Platform: $(UNAME)"
	@echo "Pd include path: $(PD_INCLUDE)"
	@echo "Target: $(TARGET)"

.PHONY: all clean install info
