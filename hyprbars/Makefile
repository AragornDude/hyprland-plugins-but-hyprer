# Makefile adapted from Reference fork
CXXFLAGS = -shared -fPIC --no-gnu-unique -g -std=c++2b -Wno-c++11-narrowing
INCLUDES = `pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon`
LIBS = `pkg-config --libs pangocairo`

HYPRLAND_VER := $(shell pkg-config --modversion hyprland | cut -d. -f1,2)

ifeq ($(HYPRLAND_VER),0.49)
    CXXFLAGS += -DHYPRLAND_049
else
    CXXFLAGS += -DHYPRLAND_050
endif

SRC = main.cpp barDeco.cpp BarPassElement.cpp
TARGET = hyprbars.so

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ $> -o $@ $(LIBS) -O2

clean:
	rm ./$(TARGET)

meson-build:
	mkdir -p build
	cd build && meson .. && ninja

.PHONY: all meson-build clean
