idf_component_register(
    SRCS "main.cpp" "knob.c"  "display.c" "comm-spi.c" "tinyosc.c" "fx.c" "ws2812_rmt.c"
    INCLUDE_DIRS "."
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC "ESP32")
