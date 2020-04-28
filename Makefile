PROJECT_NAME := esp-artnet

# COMPONENT_ADD_INCLUDEDIRS := components/include
# dumpasm: build/main/ws2812_bitbang.o
# 	echo "duming asm"
# 	xtensa-lx106-elf-objdump -d build/main/ws2812_bitbang.o >build/main/ws2812_bitbang.s

include $(IDF_PATH)/make/project.mk

