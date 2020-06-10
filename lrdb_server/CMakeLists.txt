set(TARGET_NAME lrdb_server)
add_library(${TARGET_NAME} SHARED
    ${EXTERNAL_DIR}/LRDB/src/debug_server_module.cpp
    ${EXTERNAL_DIR}/LRDB/src/lua_with_lrdb_server.cpp
    )
target_include_directories(${TARGET_NAME} PRIVATE
    ${EXTERNAL_DIR}/LRDB/include
    ${EXTERNAL_DIR}/LRDB/third_party/picojson
    ${EXTERNAL_DIR}/LRDB/third_party/asio/asio/include
    )
target_compile_definitions(${TARGET_NAME} PRIVATE
    _WIN32
    )
target_link_libraries(${TARGET_NAME} PRIVATE
    lualib
    )
