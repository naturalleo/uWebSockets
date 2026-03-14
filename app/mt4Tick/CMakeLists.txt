cmake_minimum_required(VERSION 3.14)
project(mt4Tick LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── 第三方依赖（与根项目保持完全一致）────────────────────────────
find_package(OpenSSL REQUIRED)
find_package(ZLIB    REQUIRED)
find_package(libuv   CONFIG REQUIRED)

# ── uSockets / uWS 路径（相对本文件向上两级）─────────────────────
set(USOCKETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../uSockets")
set(UWS_SRC_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

# ── 编译 uSockets 静态库 ─────────────────────────────────────────
add_library(uSockets_mt4 STATIC
    ${USOCKETS_ROOT}/src/bsd.c
    ${USOCKETS_ROOT}/src/context.c
    ${USOCKETS_ROOT}/src/loop.c
    ${USOCKETS_ROOT}/src/socket.c
    ${USOCKETS_ROOT}/src/udp.c
    ${USOCKETS_ROOT}/src/eventing/libuv.c
    ${USOCKETS_ROOT}/src/crypto/openssl.c
    ${USOCKETS_ROOT}/src/crypto/sni_tree.cpp
)
target_include_directories(uSockets_mt4 PUBLIC ${USOCKETS_ROOT}/src)
target_compile_features(uSockets_mt4 PRIVATE cxx_std_17)
target_compile_definitions(uSockets_mt4 PUBLIC
    LIBUS_USE_OPENSSL
    LIBUS_USE_LIBUV
)
target_link_libraries(uSockets_mt4 PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
)

# ── 所有子程序共用的头文件和链接项 ───────────────────────────────
set(MT4_INCLUDES  ${UWS_SRC_ROOT} ${USOCKETS_ROOT}/src)
set(MT4_LIBS      uSockets_mt4 ZLIB::ZLIB ws2_32 crypt32)

# ── 辅助宏：一行注册一个可执行程序 ───────────────────────────────
macro(add_mt4_exe NAME SRC)
    add_executable(${NAME} ${SRC})
    target_include_directories(${NAME} PRIVATE ${MT4_INCLUDES})
    target_compile_features(${NAME} PRIVATE cxx_std_20)
    target_link_libraries(${NAME} PRIVATE ${MT4_LIBS})
    set_target_properties(${NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endmacro()

# ════════════════════════════════════════════════════════════════
#  程序列表 —— 新增一个程序只需加一行 add_mt4_exe(名称 源文件)
# ════════════════════════════════════════════════════════════════
add_mt4_exe(tick_server   src/tick_server.cpp)
add_mt4_exe(tick_client   src/tick_client.cpp)
add_mt4_exe(tick_gateway  src/tick_gateway.cpp)
add_mt4_exe(tick_recorder src/tick_recorder.cpp)

install(TARGETS tick_server tick_client tick_gateway tick_recorder
        RUNTIME DESTINATION bin)

