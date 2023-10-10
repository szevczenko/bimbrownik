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

# FreeRTOS Kernel
FREERTOS_DIR := $(abspath ../lib/FreeRTOS/FreeRTOS)
FREERTOS_KERNEL_DIR := $(abspath ../lib/FreeRTOS/FreeRTOS/Source)
FREERTOS_KERNEL_INCLUDES :=
FREERTOS_KERNEL_SOURCES :=
FREERTOS_KERNEL_INCLUDE_DIRS :=
FREERTOS_KERNEL_OBJS :=
FREERTOS_KERNEL_BUILD_DIR := $(BUILD_DIR)/FreeRTOS-Kernel

## FreeRTOS Kernel includes
FREERTOS_KERNEL_INCLUDES += $(wildcard $(FREERTOS_KERNEL_DIR)/include/*.h)
FREERTOS_KERNEL_INCLUDE_DIRS += -I $(FREERTOS_KERNEL_DIR)/include \
																-I ${FREERTOS_DIR}/Demo/Common/include

## FreeRTOS Kernel sources
FREERTOS_KERNEL_SOURCES += $(wildcard $(FREERTOS_KERNEL_DIR)/*.c)
FREERTOS_KERNEL_SOURCES += $(FREERTOS_KERNEL_DIR)/portable/MemMang/heap_4.c

## FreeRTOS Kernel Windows port includes
FREERTOS_KERNEL_INCLUDES += $(FREERTOS_KERNEL_DIR)/portable/ThirdParty/GCC/Posix/portmacro.h
FREERTOS_KERNEL_INCLUDE_DIRS += -I $(FREERTOS_KERNEL_DIR)/portable/ThirdParty/GCC/Posix \
																-I $(FREERTOS_KERNEL_DIR)/portable/ThirdParty/GCC/Posix/utils

## FreeRTOS Kernel Windows port sources
FREERTOS_KERNEL_SOURCES += $(FREERTOS_KERNEL_DIR)/portable/ThirdParty/GCC/Posix/port.c \
														$(FREERTOS_KERNEL_DIR)/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/AbortDelay.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/BlockQ.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/blocktim.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/countsem.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/death.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/dynamic.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/EventGroupsDemo.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/flop.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/GenQTest.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/integer.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/IntSemTest.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/MessageBufferAMP.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/MessageBufferDemo.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/PollQ.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/QPeek.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/QueueOverwrite.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/QueueSet.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/QueueSetPolling.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/recmutex.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/semtest.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/StaticAllocation.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/StreamBufferDemo.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/StreamBufferInterrupt.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/TaskNotify.c
FREERTOS_KERNEL_SOURCES += ${FREERTOS_DIR}/Demo/Common/Minimal/TimerDemo.c

## FreeRTOS Kernel configuration includes
FREERTOS_KERNEL_INCLUDES += ./FreeRTOSConfig.h
FREERTOS_KERNEL_INCLUDE_DIRS += -I .

# FreeRTOS Kernel objects
FREERTOS_KERNEL_SOURCES_NAMES :=$(notdir $(FREERTOS_KERNEL_SOURCES))
FREERTOS_KERNEL_OBJS := $(FREERTOS_KERNEL_SOURCES_NAMES:%.c=$(FREERTOS_KERNEL_BUILD_DIR)/%.o)

# Project

PROJECT_ROOT := $(abspath ../components)
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

$(EXE) : $(MAIN_OBJS) $(FREERTOS_KERNEL_OBJS) $(UNITY_OBJS) $(PROJECT_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ -lwinmm

# Main objects rules
$(MAIN_OBJS): %.o: $(MAIN_SOURCES) $(MAIN_INCLUDES) $(FREERTOS_KERNEL_INCLUDES) $(UNITY_INCLUDES)
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(FREERTOS_KERNEL_INCLUDE_DIRS) $(UNITY_INCLUDES_DIRS) $(PROJECT_INCLUDES_DIRS)  -o $@ $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)
	@echo Compile $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)

# Project objects rules
$(PROJECT_OBJS): %.o: $(PROJECT_SRC) $(PROJECT_INCLUDES)  
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(PROJECT_INCLUDES_DIRS) $(FREERTOS_KERNEL_INCLUDE_DIRS)  -o $@ $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)
	@echo Compile $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)

# Unity objects rules
$(UNITY_OBJS): %.o: $(UNITY_SRC) $(UNITY_INCLUDES)  
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(UNITY_INCLUDES_DIRS)   -o $@ $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)
	@echo Compile $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)

# FreeRTOS Kernel objects rules
$(FREERTOS_KERNEL_OBJS): %.o: $(FREERTOS_KERNEL_SOURCES) $(FREERTOS_KERNEL_INCLUDES) 
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(FREERTOS_KERNEL_INCLUDE_DIRS)  -o $@ $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)
	@echo Compile $(filter %$(notdir $(patsubst %.o, %.c, $@)), $^)

# Clean rule
clean:
	rm -rf $(BUILD_DIR)

print_debug:
	@echo $(PROJECT_INCLUDES_DIRS)
