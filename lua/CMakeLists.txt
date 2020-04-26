set(TARGET_NAME lua)
add_executable(${TARGET_NAME}
    ${EXTERNAL_DIR}/lua/lua.c
    )
target_link_libraries(${TARGET_NAME} PRIVATE
    lualib
    )
