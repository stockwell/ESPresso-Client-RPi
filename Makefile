#
# Makefile
# WARNING: relies on invocation setting current working directory to Makefile location
# This is done in .vscode/task.json
#
PROJECT 			?= lvgl-sdl
MAKEFLAGS 			:= -j 8
SRC_EXT      		:= c
SRC_EXT_CPP			:= cpp
OBJ_EXT				:= o
OBJ_EXT_CPP			:= cpp.o
CC 					?= lldb

SRC_DIR				:= ./
WORKING_DIR			:= ./build
BUILD_DIR			:= $(WORKING_DIR)/obj
BIN_DIR				:= $(WORKING_DIR)/bin
UI_DIR 				:= ui

WARNINGS 			:= -Wall -Wextra \
						-Wshadow -Wundef -Wmissing-prototypes \
						-Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized \
						-Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default  \
					  	-Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wdeprecated  \
						-Wempty-body -Wshift-negative-value \
            			-Wtype-limits -Wsizeof-pointer-memaccess -Wpointer-arith

CFLAGS 				:= -O0 -g $(WARNINGS)

# Add simulator define to allow modification of source
DEFINES				:= -D SIMULATOR=1 -D LV_BUILD_TEST=0

# Include simulator inc folder first so lv_conf.h from custom UI can be used instead
INC 				:=  -I./ -I./src -I./src/boiler -I./vendor -I./vendor/lvgl/ -I./vendor/lvgl_drivers/ -I./vendor/espresso_ui -I./vendor/espresso_ui/settings -I./vendor/cpp-httplib -I./vendor/json/single_include
LDLIBS	 			:= -lm
BIN 				:= $(BIN_DIR)/demo

COMPILE				= $(CC) $(CFLAGS) $(INC) $(DEFINES)
CPP_COMPILE			= $(CXX) $(CFLAGS) -std=c++20 $(INC) $(DEFINES)

# Automatically include all source files
SRCS 				:= $(shell find $(SRC_DIR) -type f -name '*.c' -not -path '*/\.*')
OBJECTS    			:= $(patsubst $(SRC_DIR)%,$(BUILD_DIR)/%,$(SRCS:.$(SRC_EXT)=.$(OBJ_EXT)))

CPP_SRCS 			:= $(shell find $(SRC_DIR) -type f -name '*.cpp' -not -path '*/\.*')
OBJECTS    		+= $(patsubst $(SRC_DIR)%,$(BUILD_DIR)/%,$(CPP_SRCS:.$(SRC_EXT_CPP)=.$(OBJ_EXT_CPP)))

all: default

$(BUILD_DIR)/%.$(OBJ_EXT): $(SRC_DIR)/%.$(SRC_EXT)
	@echo 'Building c project file: $<'
	@mkdir -p $(dir $@)
	@$(COMPILE) -c -o "$@" "$<"

$(BUILD_DIR)/%.$(OBJ_EXT_CPP): $(SRC_DIR)/%.$(SRC_EXT_CPP)
	@echo 'Building cpp project file: $<'
	@mkdir -p $(dir $@)
	@$(CPP_COMPILE) -c -o "$@" "$<"

default: $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $(BIN) $(OBJECTS) $(LDFLAGS) ${LDLIBS}

clean:
	rm -rf $(WORKING_DIR)

install: ${BIN}
	install -d ${DESTDIR}/usr/lib/${PROJECT}/bin
	install $< ${DESTDIR}/usr/lib/${PROJECT}/bin/
