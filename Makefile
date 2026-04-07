TARGET		:= openjazz
TITLE_ID	:= OPJAZZ001
CONTENT_ID	:= HG0000-$(TITLE_ID)_00-0000111122223333

CC      := ppu-gcc
CXX     := ppu-g++
LD      := ppu-g++
OBJCOPY := ppu-objcopy

PS3DEV:=/usr/local/ps3dev
PS3PORTLIBS:=$(PS3DEV)/portlibs/ppu
PATH:=$(PATH):$(PS3DEV)/bin:$(PS3DEV)/ppu/bin

PKG_DIR		:= "package"
DATA_SOURCE	:= "gamefiles"
USR_DIR		:= "$(PKG_DIR)/USRDIR"
EBOOT_DEST	:= "$(USR_DIR)/EBOOT.BIN"

SOURCES  := $(shell find src -name "*.cpp") $(shell find ext -name "*.cpp")
OBJS     := $(SOURCES:.cpp=.o)
INC_DIRS := $(shell find src -type d) $(shell find ext -type d)

INCLUDES := -I$(PSL1GHT)/ppu/include -I$(PS3PORTLIBS)/include -I$(PS3PORTLIBS)/include/SDL
INCLUDES += $(addprefix -I, $(INC_DIRS))

COMMON_FLAGS := -g3 $(INCLUDES) -DSDLK_FIRST=0 -DSDLK_LAST=323 -DLSB_FIRST=0 \
                -O2 -D__PS3__ -DUSE_MODPLUG -DNO_KEYBOARD_CFG -DFULLSCREEN_ONLY -DSCALE -DFULLSCREEN_FLAGS -DENABLE_JJ2 -DLOWERCASE_FILENAMES -DWORDS_BIGENDIAN -DDATAPATH=\"/dev_hdd0/game/$(TITLE_ID)/USRDIR/\"

CFLAGS   := $(COMMON_FLAGS) -std=gnu99
CXXFLAGS := $(COMMON_FLAGS) # PSL1GHT usará esto para los .cpp

LIBS     := -L$(PS3PORTLIBS)/lib -L$(PSL1GHT)/ppu/lib \
            -lSDL_mixer -lSDL -lio -lrt -lsysutil -lgcm_sys -lrsx -laudio -llv2 -lz -lm

include $(PSL1GHT)/ppu_rules

all: $(TARGET).self

$(TARGET).elf: $(OBJS)
	$(LD) $(OBJS) $(LIBS) -o $@
	rm -rf $(PKG_DIR)
	mkdir -p $(USR_DIR)
	cp "PARAM.SFO" "$(PKG_DIR)/PARAM.SFO"
	cp "PIC1.PNG" "$(PKG_DIR)/PIC1.PNG"
	cp "ICON0.PNG" "$(PKG_DIR)/ICON0.PNG"
	-cp -r "$(DATA_SOURCE)"/* "$(USR_DIR)/"
	bash fixnames.sh
	
$(TARGET).self: $(TARGET).elf
	ppu-strip $< -o $<.strip
	sprxlinker $<.strip
	make_self $<.strip $@
	fself -n $<.strip EBOOT.BIN $(CONTENT_ID)
	cp EBOOT.BIN "$(EBOOT_DEST)"
	pkg.py --contentid $(CONTENT_ID) "$(PKG_DIR)/" openjazz_ps3.pkg
	
clean:
	rm -rf "$PKG_DIR"
	mkdir -p "$USR_DIR"
	rm -f $(OBJS) $(TARGET).elf $(TARGET).elf.strip $(TARGET).self EBOOT.BIN $(TARGET)_ps3.pkg