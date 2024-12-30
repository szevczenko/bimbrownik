# Build directory
BUILD_DIR := ./build

# Compiler - Note this expects you are using MinGW version of GCC
CC := gcc
CFLAGS := -O0 -g3 -Wextra -Wno-unused-parameter -Wall -c -fmessage-length=0 -Wcast-qual -D_WIN32_WINNT=0x0601 -DUNITY_FIXTURE_NO_EXTRAS -DprojCOVERAGE_TEST=1 \
					-Wunused-parameter -Wunused-function -Wtype-limits

# Linker - Note this expects you are using MinGW version of GCC
LD :=  gcc
LDFLAGS := -Xlinker -Map=$(BUILD_DIR)/rtosdemo.map

# Executable Targets
EXE := $(BUILD_DIR)/test.exe

# Unity Kernel
UNITY_ROOT := $(abspath ../lib/Unity)
UNITY_BUILD_DIR := $(BUILD_DIR)/Unity

UNITY_DIR := $(UNITY_ROOT)/src/
UNITY_SRC := $(wildcard $(UNITY_DIR)/*.c) \
  $(UNITY_ROOT)/extras/fixture/src/unity_fixture.c
UNITY_INCLUDES :=	$(wildcard $(UNITY_ROOT)/src/*.h)
UNITY_INCLUDES_DIRS := -I $(UNITY_ROOT)/src -I $(UNITY_ROOT)/extras/fixture/src
UNITY_SOURCES_NAMES := $(notdir $(UNITY_SRC))
UNITY_OBJS := $(UNITY_SOURCES_NAMES:%.c=$(UNITY_BUILD_DIR)/%.o)

# Project

PROJECT_ROOT := $(abspath ../components)
HQ_COMPONENTS_ROOT := $(abspath ../hq_components)
PROJECT_BUILD_DIR := $(BUILD_DIR)/project

PROJECT_DIR := $(PROJECT_ROOT)
PROJECT_SRC := $(wildcard $(PROJECT_DIR)/config/*.c) \
								$(wildcard $(PROJECT_DIR)/utils/lwjson/*.c) \
								$(PROJECT_DIR)/drivers/json_parser.c \
								$(PROJECT_DIR)/drivers/error_code.c

PROJECT_INCLUDES :=	$(wildcard $(PROJECT_DIR)/application/*.h) \
										$(wildcard $(PROJECT_DIR)/config/*.h) \
										$(wildcard $(PROJECT_DIR)/drivers/*.h) \
										$(wildcard $(PROJECT_DIR)/drivers/onewire_uart/src/include/*.h) \
										$(wildcard $(PROJECT_DIR)/utils/*.h) \
										$(wildcard $(PROJECT_DIR)/utils/lwjson/*.h) \
										$(wildcard $(PROJECT_DIR)/project_hal/*.h) \

PROJECT_INCLUDES_DIRS := -I $(PROJECT_DIR)/application \
						 							-I $(PROJECT_DIR)/config \
													 -I $(PROJECT_DIR)/drivers \
													 -I $(PROJECT_DIR)/drivers/onewire_uart/src/include \
													 -I $(PROJECT_DIR)/utils \
													 -I $(PROJECT_DIR)/utils/lwjson \
													 -I $(PROJECT_DIR)/project_hal \
													 -I $(HQ_COMPONENTS_ROOT)/backend \
													 -I $(HQ_COMPONENTS_ROOT)/drv \
													 -I $(HQ_COMPONENTS_ROOT)/mongoose_drv \
													 -I $(HQ_COMPONENTS_ROOT)/mongoose_drv/mongoose \
													 -I $(HQ_COMPONENTS_ROOT)/oled/src \
													 -I $(HQ_COMPONENTS_ROOT)/oled_ui \
													 -I ./mocks
													 

PROJECT_SOURCES_NAMES := $(notdir $(PROJECT_SRC))
PROJECT_OBJS := $(PROJECT_SOURCES_NAMES:%.c=$(PROJECT_BUILD_DIR)/%.o)

# Main
MAIN_DIR := .
MAIN_INCLUDES := 
MAIN_SOURCES :=
MAIN_INCLUDE_DIRS :=
MAIN_BUILD_DIR := $(BUILD_DIR)/main

## Main includes

## Main sources
MAIN_SOURCES += $(wildcard $(MAIN_DIR)/*.c) \
								$(wildcard $(MAIN_DIR)/mocks/*.c) \
								$(wildcard $(MAIN_DIR)/tests/*.c) \

## Main objects
MAIN_SOURCES_NAMES :=$(notdir $(MAIN_SOURCES))
MAIN_OBJS := $(MAIN_SOURCES_NAMES:%.c=$(MAIN_BUILD_DIR)/%.o)

$(EXE) : $(MAIN_OBJS) $(UNITY_OBJS) $(PROJECT_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ -lwinmm

# Main objects rules
$(MAIN_OBJS): %.o: $(MAIN_SOURCES) $(MAIN_INCLUDES) $(UNITY_INCLUDES)
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(UNITY_INCLUDES_DIRS) $(PROJECT_INCLUDES_DIRS)  -o $@ $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)
	@echo Compile $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)

# Project objects rules
$(PROJECT_OBJS): %.o: $(PROJECT_SRC) $(PROJECT_INCLUDES)  
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(PROJECT_INCLUDES_DIRS)  -o $@ $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)
	@echo Compile $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)

# Unity objects rules
$(UNITY_OBJS): %.o: $(UNITY_SRC) $(UNITY_INCLUDES)  
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(UNITY_INCLUDES_DIRS)   -o $@ $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)
	@echo Compile $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)

# Clean rule
clean:
	rm -rf $(BUILD_DIR)

print_debug:
	@echo $(PROJECT_INCLUDES_DIRS)
