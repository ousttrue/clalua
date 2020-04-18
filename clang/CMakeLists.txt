set(TARGET_NAME clang)
add_library(${TARGET_NAME} INTERFACE)
target_include_directories(${TARGET_NAME} INTERFACE
    "C:/Program Files/LLVM/include"
    )
target_link_directories(${TARGET_NAME} INTERFACE
    "C:/Program Files/LLVM/lib"
    )
target_link_libraries(${TARGET_NAME} INTERFACE
    libclang
    )
