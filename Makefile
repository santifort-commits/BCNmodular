RACK_DIR ?= $(HOME)/Rack-SDK
SLUG = BCNmodular

FLAGS += -I.
CFLAGS +=
CXXFLAGS +=

LDFLAGS += -shared-libgcc

SOURCES += src/plugin.cpp
SOURCES += src/Maestro.cpp

DISTRIBUTABLES += res
DISTRIBUTABLES += plugin.json

include $(RACK_DIR)/plugin.mk