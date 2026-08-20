cmake_minimum_required(VERSION 3.21)

add_executable(gargantua_tests
    test_math.cpp
    ../src/BlackHole.cpp
    ../src/Camera.cpp
)

target_include_directories(gargantua_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/glm
    ${CMAKE_SOURCE_DIR}/external/glad/include
)

target_link_libraries(gargantua_tests PRIVATE glm)

add_test(NAME GargantuaMathTests COMMAND gargantua_tests)
