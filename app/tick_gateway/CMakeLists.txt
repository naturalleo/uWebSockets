# tick_gateway — 直接复用根项目已定义的 uSockets target

set(USOCKETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../uSockets")
set(UWS_SRC_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(tick_gateway tick_gateway.cpp)
target_include_directories(tick_gateway PRIVATE ${UWS_SRC_ROOT} ${USOCKETS_ROOT}/src)
target_compile_features(tick_gateway PRIVATE cxx_std_20)
target_link_libraries(tick_gateway PRIVATE uSockets ZLIB::ZLIB ws2_32 crypt32)
set_target_properties(tick_gateway PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
