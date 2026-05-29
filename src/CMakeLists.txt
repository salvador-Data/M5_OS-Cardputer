# PlatformIO ESP-IDF: stub app when building custom bootloader only.

if(M5OS_BOOTLOADER_STUB)
    idf_component_register(SRCS "${CMAKE_SOURCE_DIR}/idf_boot_stub/main.c")
else()
    FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)
    idf_component_register(SRCS ${app_sources})
endif()
