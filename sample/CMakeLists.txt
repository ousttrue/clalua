set(TARGET_NAME sample)
add_executable(${TARGET_NAME}
    main.cpp
    ClangIndex.cpp
    ClangCursorTraverser.cpp
    )
target_include_directories(${TARGET_NAME} PRIVATE
    ${EXTERNAL_DIR}/span/include
    ${EXTERNAL_DIR}/plog/include
    ${EXTERNAL_DIR}/nameof/include
    )
target_link_libraries(${TARGET_NAME} PRIVATE
    clang
    fmt
    lualib
    )
