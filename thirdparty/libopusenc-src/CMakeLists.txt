# libopusenc 0.2.1 (Xiph.Org Foundation) - vendored sources.
#
# Upstream ships an autotools build only, so this target compiles the same source
# set for the platforms that have no prebuilt libopusenc: Android, iOS and macOS.
# Windows keeps linking the vcpkg-built binaries under thirdparty/libopusenc via
# code/projects/windows/ImmOpusPropertySheet.props.
#
# The version macros normally come from the autotools config.h; opusenc.c uses
# them for the vendor string and OpusEncGetVersionString().

cmake_minimum_required(VERSION 3.10)

add_library(opusenc STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ogg_packer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/opus_header.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/opusenc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/picture.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/unicode_support.c
)

target_include_directories(opusenc
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(opusenc PRIVATE
    PACKAGE_NAME="libopusenc"
    PACKAGE_VERSION="0.2.1"
    # Upstream's Makefile.am adds these: OUTSIDE_SPEEX makes src/resample.c use the
    # vendored speex_resampler.h instead of the system speexdsp headers, and
    # RANDOM_PREFIX keeps the vendored resampler symbols from clashing with a real
    # speexdsp in the same process.
    OUTSIDE_SPEEX
    RANDOM_PREFIX=libopusenc
)

# The encoder lives in libopus; the container writer in libogg. Both come from
# thirdparty (opus-src and ogg) and are added by the including project.
target_link_libraries(opusenc PUBLIC opus ogg)

set_target_properties(opusenc PROPERTIES POSITION_INDEPENDENT_CODE ON)
