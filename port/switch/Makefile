#---------------------------------------------------------------------------------
# Halo: Combat Evolved - Nintendo Switch Port Makefile
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
	ifneq ($(wildcard /opt/devkitpro/libnx/switch_rules),)
		export DEVKITPRO := /opt/devkitpro
	endif
endif

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

export DEVKITPRO

include $(DEVKITPRO)/libnx/switch_rules

TARGET      := halo
BUILD       := build
SOURCES     := src
INCLUDES    := include
ROMFS       := romfs

APP_TITLE   := Halo: Combat Evolved
APP_AUTHOR  := Bungie / halo-ce-universal
APP_VERSION := 1.0.0

ARCH        := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS      := -g -Wall -O2 -ffunction-sections $(ARCH) \
               -I$(LIBNX)/include -I$(PORTLIBS)/include -I$(INCLUDES) \
               -D__SWITCH__ -D__LP64__

LDFLAGS     := -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) \
               -L$(LIBNX)/lib -L$(PORTLIBS)/lib -Wl,-Map,$(BUILD)/$(TARGET).map

LIBS        := -lSDL2 -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lnx -lm -lstdc++

CFILES      := $(wildcard $(SOURCES)/*.c)
SFILES      := $(wildcard $(SOURCES)/*.s)
OFILES      := $(patsubst $(SOURCES)/%.c, $(BUILD)/%.o, $(CFILES)) $(patsubst $(SOURCES)/%.s, $(BUILD)/%.o, $(SFILES))

all: $(TARGET).nro

$(TARGET).nro: $(TARGET).elf $(TARGET).nacp
	elf2nro $(TARGET).elf $(TARGET).nro --nacp=$(TARGET).nacp --romfsdir=$(ROMFS)

$(TARGET).nacp:
	nacptool --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" $(TARGET).nacp

$(TARGET).elf: $(OFILES)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/%.o: $(SOURCES)/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: $(SOURCES)/%.s
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nro $(TARGET).nacp $(TARGET).map

.PHONY: all clean
