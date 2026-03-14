cmake_minimum_required(VERSION 3.14)
project(my_app)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(libuv CONFIG REQUIRED)

# 编译 uSockets（路径相对于本 CMakeLists.txt 所在目录）
set(USOCKETS_SRC
    uSockets/src/bsd.c
    uSockets/src/context.c
    uSockets/src/loop.c
    uSockets/src/socket.c
    uSockets/src/udp.c
    uSockets/src/eventing/libuv.c
    uSockets/src/crypto/openssl.c
    uSockets/src/crypto/sni_tree.cpp
)

add_library(uSockets STATIC ${USOCKETS_SRC})
target_include_directories(uSockets PUBLIC uSockets/src)
target_compile_features(uSockets PRIVATE cxx_std_17)
target_compile_definitions(uSockets PUBLIC
    LIBUS_USE_OPENSSL
    LIBUS_USE_LIBUV
)
target_link_libraries(uSockets PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
)

# 你的程序
add_executable(my_app main.cpp)
target_include_directories(my_app PRIVATE
    src
    uSockets/src
)
target_compile_features(my_app PRIVATE cxx_std_20)
target_link_libraries(my_app PRIVATE
    uSockets
    ZLIB::ZLIB
    ws2_32
    crypt32
)

# ── 第三方本地依赖（app/include + app/lib/Debug|Release）────────
# 将 app/include 下的头文件和 app/lib/<Config> 下的 .lib
# 封装为一个 INTERFACE 库，所有 app exe 链接它即可自动继承
add_library(app_third_party INTERFACE)
target_include_directories(app_third_party INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/app/include"
)
target_link_directories(app_third_party INTERFACE
    "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/app/lib/Debug>"
    "$<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/app/lib/Release>"
    "$<$<CONFIG:RelWithDebInfo>:${CMAKE_CURRENT_SOURCE_DIR}/app/lib/Release>"
    "$<$<CONFIG:MinSizeRel>:${CMAKE_CURRENT_SOURCE_DIR}/app/lib/Release>"
)
# 新增第三方 .lib 时，在此处追加：
# target_link_libraries(app_third_party INTERFACE
#     "$<$<CONFIG:Debug>:fmtd>"
#     "$<$<CONFIG:Release>:fmt>"
# )

# app 子项目（每个目录对应一个独立的 exe）
add_subdirectory(app/tick_server)
add_subdirectory(app/tick_client)
add_subdirectory(app/tick_gateway)
add_subdirectory(app/tick_recorder)
add_subdirectory(app/mt4_tick)
