cmake_minimum_required(VERSION 3.16.1)

project(CLGLInterop)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    SET_PROPERTY(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
        "MinSizeRel" "RelWithDebInfo")
endif()

find_package(glfw3 3.3 REQUIRED)
find_package(OpenCL 1.2 REQUIRED)
find_package(OpenGL 3.3 REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(external_sources/glad)

file(GLOB src "src/*")
file(GLOB assets "assets/*")

set(APP_NAME OpenClPlayground)

add_executable(${APP_NAME}
    ${src}
    ${assets}
    )
target_include_directories(${APP_NAME}
    PRIVATE ${CMAKE_SOURCE_DIR}
    )
target_link_libraries(${APP_NAME}
    PRIVATE
    glfw
    OpenCL::OpenCL
    OpenGL::GL
    ${CMAKE_DL_LIBS}
    glad-interface
    $<$<PLATFORM_ID:Darwin>:X11::x11>
    fmt
    )
target_compile_definitions(${APP_NAME}
    PRIVATE
    "ASSETS_DIR=\"${ASSETS_DIR}\""
    $<$<PLATFORM_ID:Linux>:OS_LNX>
    $<$<PLATFORM_ID:DarWin>:OS_MAC>
    $<$<PLATFORM_ID:Windows>:OS_WIN>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    )
set_target_properties(${APP_NAME}
    PROPERTIES
    OUTPUT_NAME ${APP_NAME}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    )
