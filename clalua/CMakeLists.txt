set(TARGET_NAME clalua)
add_library(${TARGET_NAME} SHARED
    clalua.cpp
    ClangIndex.cpp
    ClangCursorTraverser.cpp
    )
target_include_directories(${TARGET_NAME} PRIVATE
    ${EXTERNAL_DIR}/span/include
    ${EXTERNAL_DIR}/plog/include
    ${EXTERNAL_DIR}/nameof/include
    ${EXTERNAL_DIR}/perilune/include
    )
target_compile_definitions(${TARGET_NAME} PRIVATE
    CLALUA_BUILD
    )
target_link_libraries(${TARGET_NAME} PRIVATE
    clang
    fmt
    lualib
    )
