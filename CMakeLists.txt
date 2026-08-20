cmake_minimum_required(VERSION 3.21)
project(Gargantua VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GARGANTUA_ENABLE_WARNINGS "Enable warnings" ON)

if(GARGANTUA_ENABLE_WARNINGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Vendor: GLM (header-only) ---
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${CMAKE_SOURCE_DIR}/external/glm)

# --- Vendor: GLFW ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# --- Vendor: GLAD (OpenGL 4.3 Core) ---
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# --- Vendor: Dear ImGui ---
add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    external/imgui
    external/imgui/backends
)
target_link_libraries(imgui PUBLIC glfw)
target_include_directories(imgui PRIVATE external/glad/include)

# --- Project sources ---
set(GARGANTUA_HEADERS
    include/Shader.hpp
    include/Camera.hpp
    include/BlackHole.hpp
    include/Renderer.hpp
    include/Input.hpp
)

set(GARGANTUA_SOURCES
    src/Shader.cpp
    src/Camera.cpp
    src/BlackHole.cpp
    src/Renderer.cpp
    src/Input.cpp
    src/main.cpp
)

add_executable(gargantua ${GARGANTUA_SOURCES} ${GARGANTUA_HEADERS})
target_include_directories(gargantua PRIVATE include external/glad/include)
target_link_libraries(gargantua PRIVATE
    glad
    glfw
    glm
    imgui
    ${CMAKE_DL_LIBS}
)

find_package(OpenGL REQUIRED)
target_link_libraries(gargantua PRIVATE OpenGL::GL)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gargantua PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy shaders to build directory
add_custom_command(TARGET gargantua POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:gargantua>/assets
    COMMENT "Copying assets to build directory"
)

add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_BINARY_DIR}/assets
)
add_dependencies(gargantua copy_assets)

# Tests
enable_testing()
add_subdirectory(tests)

install(TARGETS gargantua DESTINATION bin)
install(DIRECTORY assets DESTINATION bin)

message(STATUS "Gargantua configured - CXX: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
