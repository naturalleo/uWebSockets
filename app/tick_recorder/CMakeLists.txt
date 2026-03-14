# tick_recorder — 直接复用根项目已定义的 uSockets target

set(USOCKETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../uSockets")
set(UWS_SRC_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(tick_recorder tick_recorder.cpp)
target_include_directories(tick_recorder PRIVATE ${UWS_SRC_ROOT} ${USOCKETS_ROOT}/src)
target_compile_features(tick_recorder PRIVATE cxx_std_20)
target_link_libraries(tick_recorder PRIVATE uSockets ZLIB::ZLIB ws2_32 crypt32)
set_target_properties(tick_recorder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
