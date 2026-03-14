# mt4_tick — 复用根项目的 uSockets；通过 app_third_party 引入本地三方库（含 MT4ManagerAPI）

set(USOCKETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../uSockets")
set(UWS_SRC_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(mt4_tick
    main_quote.cpp
    MangerAPI.cpp
    MangerAPI.h
    MT4Manager.cpp
    MT4Manager.h
    MTManager.h
    MTTradeData.h
    Platform.h
    Models.h
    Common.h
)
target_include_directories(mt4_tick PRIVATE ${UWS_SRC_ROOT} ${USOCKETS_ROOT}/src)
target_compile_features(mt4_tick PRIVATE cxx_std_20)
target_compile_definitions(mt4_tick PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
)
target_compile_options(mt4_tick PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/wd4244>
)
target_link_libraries(mt4_tick PRIVATE uSockets ZLIB::ZLIB ws2_32 crypt32 app_third_party)
set_target_properties(mt4_tick PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 构建后复制 mtmanapi64.dll 到输出目录
add_custom_command(TARGET mt4_tick POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<$<CONFIG:Debug>:${MT4_DLL_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${MT4_DLL_RELEASE}>"
        "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    COMMENT "Copying mtmanapi64.dll for mt4_tick"
)
