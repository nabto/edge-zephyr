cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(mbedtls_test LANGUAGES C VERSION 1.0.0)

zephyr_system_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
zephyr_library_compile_definitions(-DMBEDTLS_SSL_ALPN)

target_sources(app PRIVATE src/main.c)



#zephyr_include_directories(${APPLICATION_SOURCE_DIR}/src/tls_config)
#zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/tls_config)
#zephyr_link_libraries(mbedTLS)
#zephyr_library_compile_definitions(-DNABTO_DEVICE_NO_FUTURE_QUEUE)
#zephyr_library_compile_definitions(-DNP_CONFIG_FILE=<np_config_zephyr.h>)
#set(CMAKE_CXX_FLAGS "-03")
