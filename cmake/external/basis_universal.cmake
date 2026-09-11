# Basis Universal (Khronos KTX2 / Basis Transcoder & Encoder)
set(BASISU_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/basis_universal")

if(EXISTS "${BASISU_ROOT}/transcoder/basisu_transcoder.cpp")
    # 1. Transcoder library (used by Runtime & Engine Assets)
    add_library(engine_external_basisu_transcoder STATIC
        "${BASISU_ROOT}/transcoder/basisu_transcoder.cpp"
    )
    target_include_directories(engine_external_basisu_transcoder PUBLIC
        "${BASISU_ROOT}/transcoder"
    )
    target_compile_definitions(engine_external_basisu_transcoder PUBLIC
        BASISD_SUPPORT_KTX2=1
        BASISD_SUPPORT_KTX2_ZSTD=1
    )
    target_link_libraries(engine_external_basisu_transcoder PRIVATE libzstd_static)

    file(GLOB BASISU_ENCODER_SOURCES "${BASISU_ROOT}/encoder/*.cpp")
    list(REMOVE_ITEM BASISU_ENCODER_SOURCES
        "${BASISU_ROOT}/encoder/basisu_wasm_api.cpp"
        "${BASISU_ROOT}/encoder/basisu_wasm_transcoder_api.cpp"
    )
    list(APPEND BASISU_ENCODER_SOURCES
        "${BASISU_ROOT}/encoder/3rdparty/android_astc_decomp.cpp"
        "${BASISU_ROOT}/transcoder/basisu_transcoder.cpp"
    )
    add_library(engine_external_basisu_encoder STATIC ${BASISU_ENCODER_SOURCES})
    target_include_directories(engine_external_basisu_encoder PUBLIC
        "${BASISU_ROOT}/encoder"
        "${BASISU_ROOT}/transcoder"
    )
    target_compile_definitions(engine_external_basisu_encoder PUBLIC
        BASISD_SUPPORT_KTX2=1
        BASISD_SUPPORT_KTX2_ZSTD=1
        BASISU_NO_ITERATOR_DEBUG=1
        BASISU_SUPPORT_ASTCENC=0
    )
    target_link_libraries(engine_external_basisu_encoder PRIVATE libzstd_static)

    # Disable unity build for basisu files to prevent internal namespace collisions
    # and use C++17 to avoid standard header popcount mismatches without modifying thirdparty files
    set_target_properties(engine_external_basisu_transcoder PROPERTIES UNITY_BUILD OFF CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    set_target_properties(engine_external_basisu_encoder PROPERTIES UNITY_BUILD OFF CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
endif()
