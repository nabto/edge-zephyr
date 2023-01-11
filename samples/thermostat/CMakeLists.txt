cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C VERSION 1.0.0)

set(NABTO_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../modules/lib/nabto-embedded-sdk)
set(THERMOSTAT_DIR ${NABTO_DIR}/examples/thermostat)

zephyr_compile_options("-DHAVE_OPEN")

set(src
    src/main.c
    ${THERMOSTAT_DIR}/src/thermostat.c
    ${THERMOSTAT_DIR}/src/thermostat_coap_handler.c
    ${THERMOSTAT_DIR}/src/thermostat_state.c
    ${THERMOSTAT_DIR}/src/thermostat_state_data.c
    ${THERMOSTAT_DIR}/src/thermostat_state_file_backend.c
    ${THERMOSTAT_DIR}/src/thermostat_iam.c
    ${THERMOSTAT_DIR}/src/thermostat_get.c
    ${THERMOSTAT_DIR}/src/thermostat_set_mode.c
    ${THERMOSTAT_DIR}/src/thermostat_set_power.c
    ${THERMOSTAT_DIR}/src/thermostat_set_target.c
    ${ne_iam_src}

    ${NABTO_DIR}/apps/common/json_config.c
    ${NABTO_DIR}/apps/common/random_string.c
    ${NABTO_DIR}/apps/common/string_file.c

)

target_sources(app PRIVATE ${src})
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/src/tls_config ${NABTO_DIR}/3rdparty/tinycbor/tinycbor/src ${NABTO_DIR}/src ${NABTO_DIR}/nabto-common/components/nn/include ${NABTO_DIR}/3rdparty/tinycbor/extra ${NABTO_DIR} ${NABTO_DIR}/3rdparty/cjson ${NABTO_DIR}/examples/thermostat/src)
