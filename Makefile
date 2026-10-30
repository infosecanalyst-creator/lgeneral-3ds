# LGeneral for Nintendo 3DS - devkitARM build
# usage (devkitPro MSYS2 shell):  make            -> lgeneral.3dsx
#                                  make clean
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "DEVKITARM is not set. Run this from the devkitPro MSYS2 shell (or export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM)")
endif

include $(DEVKITARM)/3ds_rules

TARGET          := lgeneral
BUILD           := build
APP_TITLE       := LGeneral
APP_DESCRIPTION := Open-source turn-based strategy
APP_AUTHOR      := LGeneral (M. Speck) / 3DS port
APP_ICON        := platform/3ds/icon.png

# DSUI = native dual-screen front end (platform/dsui); make DSUI=0 for the plain 640x480 port
DSUI ?= 1
SRCS := $(wildcard lgeneral/src/*.c) $(wildcard lgeneral/util/*.c) $(wildcard platform/3ds/*.c)
ifeq ($(DSUI),1)
SRCS += $(wildcard platform/dsui/*.c)
endif
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

# SDL 1.2 from devkitPro portlibs (3ds-sdl); prefer its sdl-config when present
SDL_CONFIG := $(PORTLIBS)/bin/sdl-config
ifneq ($(wildcard $(SDL_CONFIG)),)
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags)
SDL_LIBS   := $(shell $(SDL_CONFIG) --libs)
else
SDL_CFLAGS := -I$(PORTLIBS)/include/SDL -I$(PORTLIBS)/include
SDL_LIBS   := -L$(PORTLIBS)/lib -lSDL -lcitro3d -lctru -lm
endif

CFLAGS := -g -O2 -Wall -Wno-unused -Wno-pointer-sign -Wno-format-overflow \
          -mword-relocations -ffunction-sections -fgnu89-inline $(ARCH) \
          -D__3DS__ -DHAVE_CONFIG_H -DINSTALLDIR=\"sdmc:/3ds/lgeneral\" \
          -Ilgeneral -Ilgeneral/src -Ilgeneral/util -Iplatform/3ds \
          $(SDL_CFLAGS) -I$(CTRULIB)/include

ifeq ($(DSUI),1)
CFLAGS += -DDSUI -Iplatform/dsui
endif

# sound: on by default (needs 3ds-sdl_mixer; at runtime the DSP firmware, sdmc:/3ds/dspfirm.cdc).
# Without the firmware the game runs silently. make SOUND=0 builds without SDL_mixer.
SOUND ?= 1
ifeq ($(SOUND),1)
CFLAGS   += -DWITH_SOUND
MIX_LIBS := -lSDL_mixer -lmikmod -lmad -lvorbisidec -logg
endif

LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map -Wl,--gc-sections
LIBS    := $(MIX_LIBS) $(SDL_LIBS) -L$(CTRULIB)/lib -lcitro3d -lctru -lm

all: $(TARGET).3dsx

$(TARGET).3dsx: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).smdh
	@echo "packaging $@ (with --smdh)"
	@3dsxtool $< $@ --smdh=$(BUILD)/$(TARGET).smdh

$(BUILD)/$(TARGET).smdh: $(APP_ICON)
	@mkdir -p $(BUILD)
	@smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(APP_ICON) $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	@echo "linking $(notdir $@) (sound=$(SOUND))"
	@$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo $<
	@$(CC) -MMD -MP $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET).3dsx

-include $(DEPS)

.PHONY: all clean
