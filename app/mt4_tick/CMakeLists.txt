# main_quote — 直接复用根项目已定义的 uSockets target

set(USOCKETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../uSockets")
set(UWS_SRC_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(mt4_tick main_quote.cpp)
target_include_directories(mt4_tick PRIVATE ${UWS_SRC_ROOT} ${USOCKETS_ROOT}/src)
target_compile_features(mt4_tick PRIVATE cxx_std_20)
target_link_libraries(mt4_tick PRIVATE uSockets ZLIB::ZLIB ws2_32 crypt32)
set_target_properties(mt4_tick PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
