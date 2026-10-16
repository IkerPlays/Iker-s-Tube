.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error "Define DEVKITARM (export DEVKITARM=$DEVKITPRO/devkitARM)")
endif
TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET      := IkersMusic3DS
BUILD       := build
SOURCES     := source
INCLUDES    := include
APP_TITLE   := Iker's Music 3DS
APP_DESCRIPTION := Reproductor de musica con playlists
APP_AUTHOR  := Iker
APP_ICON    := $(CURDIR)/meta/icon.png

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ -std=gnu11
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lcitro2d -lcitro3d -lmpg123 -lvorbisidec -logg -lctru -lm
LIBDIRS  := $(PORTLIBS) $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(CURDIR)/$(SOURCES)
export DEPSDIR  := $(CURDIR)/$(BUILD)
export LD       := $(CC)
export OFILES   := $(patsubst %.c,%.o,$(notdir $(wildcard $(SOURCES)/*.c)))
export INCLUDE  := $(foreach d,$(INCLUDES),-I$(CURDIR)/$(d)) $(foreach d,$(LIBDIRS),-I$(d)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach d,$(LIBDIRS),-L$(d)/lib)
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh

.PHONY: $(BUILD) clean all
all: $(BUILD)
$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)
$(OUTPUT).smdh: $(TOPDIR)/Makefile
	@smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(APP_ICON) $@
-include $(DEPENDS)
endif
