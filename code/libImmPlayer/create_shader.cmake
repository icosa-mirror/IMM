# create_shader.cmake - Converts GLSL files to C header
# Usage: cmake -D INPUT=<dir> -D OUTPUT=<output.h> -D NAMESPACE=<ns> -P create_shader.cmake

if(NOT DEFINED INPUT)
    message(FATAL_ERROR "INPUT not defined")
endif()
if(NOT DEFINED OUTPUT)
    message(FATAL_ERROR "OUTPUT not defined")
endif()

# Get parent directory of output to create tmp dir
get_filename_component(OUTPUT_DIR "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# Find all .es.glsl files in the input directory
file(GLOB SHADER_FILES "${INPUT}/*.es.glsl")

# Start the output file
file(WRITE "${OUTPUT}" "// Auto-generated shader header\n")

foreach(SHADER_FILE ${SHADER_FILES})
    # Get the filename without extension
    get_filename_component(SHADER_NAME "${SHADER_FILE}" NAME_WE)

    # Read the file content
    file(READ "${SHADER_FILE}" SHADER_CONTENT)

    # Write as C string
    file(APPEND "${OUTPUT}" "static const char* ${SHADER_NAME} = R\"(\n")
    file(APPEND "${OUTPUT}" "${SHADER_CONTENT}")
    file(APPEND "${OUTPUT}" ")\";\n\n")
endforeach()

message(STATUS "Generated shader header: ${OUTPUT}")
