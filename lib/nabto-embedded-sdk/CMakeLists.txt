set(NABTO_EMBEDDED_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/lib/nabto-embedded-sdk)

include(${NABTO_EMBEDDED_SDK_DIR}/nabto_primary_files.cmake)

set(src
    src/nabto_platform_zephyr.c
    src/nabto_mutex_zephyr.c
    src/nabto_event_queue_zephyr.c
    #src/net/nm_zephyr_net_mdns_udp_bind.c
    src/net/nm_zephyr_net_tcp.c
    src/net/nm_zephyr_net_udp.c
    src/net/nm_zephyr_net.c
    src/dns/nm_zephyr_dns.c
    src/ts/nm_zephyr_timestamp.c
    ${ne_required_src_no_tls}
    ${ne_mbedtls_module_src}
    ${ne_api_test_src}
)

zephyr_library()
zephyr_include_directories(${NABTO_EMBEDDED_SDK_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
zephyr_library_include_directories(${ne_priv_include_dirs_no_tls})
zephyr_library_sources(${src})
zephyr_link_libraries(mbedTLS)
zephyr_library_compile_definitions(-DNABTO_DEVICE_NO_FUTURE_QUEUE)



set(NABTO_CORE_DIR ${NABTO_EMBEDDED_SDK_DIR}/src/core)

add_custom_target(GENERATE_VERSION ALL
  WORKING_DIRECTORY ${NABTO_CORE_DIR}
  BYPRODUCTS ${NABTO_CORE_DIR}/nc_version.c
  COMMAND ${CMAKE_COMMAND} -P
  ${NABTO_CORE_DIR}/version.cmake
  )

add_dependencies(zephyr_interface GENERATE_VERSION)
