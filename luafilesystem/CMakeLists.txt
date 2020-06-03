set(TARGET_NAME lfs)
add_library(${TARGET_NAME} SHARED
    ${EXTERNAL_DIR}/luafilesystem/src/lfs.c
    ${EXTERNAL_DIR}/luafilesystem/src/lfs.def
    )
target_include_directories(${TARGET_NAME} PRIVATE
    ${EXTERNAL_DIR}/luafilesystem/src
    )
target_compile_definitions(${TARGET_NAME} PRIVATE
    _WIN32
    )
target_link_libraries(${TARGET_NAME} PRIVATE
    lualib
    )
