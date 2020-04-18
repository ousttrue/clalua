set(TARGET_NAME fmt)
add_library(${TARGET_NAME}
    ${EXTERNAL_DIR}/fmt/src/format.cc
    ${EXTERNAL_DIR}/fmt/src/os.cc
    )
target_include_directories(${TARGET_NAME} PUBLIC
    ${EXTERNAL_DIR}/fmt/include
)
