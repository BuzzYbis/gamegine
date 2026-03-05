function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
  set (BINARY_SHADERS_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)
  
  # Convert source paths to absolute paths and collect outputs
  set (ABSOLUTE_SOURCES "")
  set (SPV_OUTPUTS "")
  set (BINARY_SPV_OUTPUTS "")
  
  foreach (SOURCE ${SHADER_SOURCES})
    get_filename_component(ABS_SOURCE ${SOURCE} ABSOLUTE)
    get_filename_component(SOURCE_NAME ${SOURCE} NAME_WE)
    get_filename_component(SOURCE_DIR ${SOURCE} DIRECTORY)
    
    list(APPEND ABSOLUTE_SOURCES ${ABS_SOURCE})
    
    # Output files: source_dir/source_name.spv
    set (SPV_OUTPUT ${SOURCE_DIR}/${SOURCE_NAME}.spv)
    set (BINARY_SPV_OUTPUT ${BINARY_SHADERS_DIR}/${SOURCE_NAME}.spv)
    
    list(APPEND SPV_OUTPUTS ${SPV_OUTPUT})
    list(APPEND BINARY_SPV_OUTPUTS ${BINARY_SPV_OUTPUT})
    
    # Compile each shader individually
    add_custom_command (
            OUTPUT ${SPV_OUTPUT}
            COMMAND ${SLANGC_EXECUTABLE} ${ABS_SOURCE} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SPV_OUTPUT}
            DEPENDS ${ABS_SOURCE}
            COMMENT "Compiling Slang Shader: ${SOURCE_NAME}.slang"
            VERBATIM
    )
    
    # Copy each compiled shader to binary output directory
    add_custom_command (
            OUTPUT ${BINARY_SPV_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${BINARY_SHADERS_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${SPV_OUTPUT} ${BINARY_SPV_OUTPUT}
            DEPENDS ${SPV_OUTPUT}
            COMMENT "Copying shader ${SOURCE_NAME}.spv to binary directory"
            VERBATIM
    )
  endforeach()
  
  add_custom_target (${TARGET} DEPENDS ${BINARY_SPV_OUTPUTS})
endfunction()

