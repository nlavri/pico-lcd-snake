set(LIBSSH2_SOURCE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../src)

set(LIBSSH2_PICO_SOURCES
    ${LIBSSH2_SOURCE_DIRECTORY}/bcrypt_pbkdf.c
    ${LIBSSH2_SOURCE_DIRECTORY}/channel.c
    ${LIBSSH2_SOURCE_DIRECTORY}/comp.c
    ${LIBSSH2_SOURCE_DIRECTORY}/chacha.c
    ${LIBSSH2_SOURCE_DIRECTORY}/cipher-chachapoly.c
    ${LIBSSH2_SOURCE_DIRECTORY}/crypt.c
    ${LIBSSH2_SOURCE_DIRECTORY}/crypto.c
    ${LIBSSH2_SOURCE_DIRECTORY}/global.c
    ${LIBSSH2_SOURCE_DIRECTORY}/hostkey.c
    ${LIBSSH2_SOURCE_DIRECTORY}/keepalive.c
    ${LIBSSH2_SOURCE_DIRECTORY}/kex.c
    ${LIBSSH2_SOURCE_DIRECTORY}/mac.c
    ${LIBSSH2_SOURCE_DIRECTORY}/misc.c
    ${LIBSSH2_SOURCE_DIRECTORY}/packet.c
    ${LIBSSH2_SOURCE_DIRECTORY}/pem.c
    ${LIBSSH2_SOURCE_DIRECTORY}/poly1305.c
    ${LIBSSH2_SOURCE_DIRECTORY}/session.c
    ${LIBSSH2_SOURCE_DIRECTORY}/transport.c
    ${LIBSSH2_SOURCE_DIRECTORY}/userauth.c
    ${LIBSSH2_SOURCE_DIRECTORY}/userauth_kbd_packet.c
)

set_source_files_properties(${LIBSSH2_PICO_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-include${CMAKE_CURRENT_LIST_DIR}/libssh2_pico_platform.h;-Wno-unused-function;-Wno-sign-conversion"
)

add_library(libssh2_pico STATIC
    ${LIBSSH2_PICO_SOURCES}
    ${PICO_SDK_PATH}/src/rp2_common/pico_mbedtls/pico_mbedtls.c
)

target_include_directories(libssh2_pico
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${LIBSSH2_SOURCE_DIRECTORY}
        ${CMAKE_SOURCE_DIR}/firmware/apps
        ${PICO_SDK_PATH}/lib/mbedtls/include
        ${PICO_SDK_PATH}/lib/mbedtls/library
        ${PICO_SDK_PATH}/src/rp2_common/pico_mbedtls/include
)

target_compile_definitions(libssh2_pico
    PRIVATE
        HAVE_CONFIG_H=1
        LIBSSH2_MBEDTLS=1
        LIBSSH2_MAX_BANNER_LEN=512
        LIBSSH2_MAX_SSH_PACKET_LEN=4096
        LIBSSH2_NO_3DES=1
        LIBSSH2_NO_AES_CBC=1
        LIBSSH2_NO_BLOWFISH=1
        LIBSSH2_NO_CAST=1
        LIBSSH2_NO_DEPRECATED=1
        LIBSSH2_NO_HMAC_RIPEMD=1
        LIBSSH2_NO_MD5=1
        LIBSSH2_NO_MD5_PEM=1
        LIBSSH2_NO_RC4=1
        LIBSSH2_NO_RSA_SHA1=1
        MBEDTLS_CONFIG_FILE="pico_mbedtls_config.h"
)

target_link_libraries(libssh2_pico
    PRIVATE
        pico_mbedtls_crypto
        pico_rand
        pico_cyw43_arch_lwip_sys_freertos
        FreeRTOS-Kernel-Heap4
)
