cmake_minimum_required(VERSION 3.10)

if (CMAKE_GENERATOR MATCHES "Ninja")
    file(
        WRITE "${CMAKE_BINARY_DIR}/GNUMakeRulesOverwrite.cmake" 
            "STRING(REPLACE \"-MD\" \"-MMD\" CMAKE_DEPFILE_FLAGS_C \"\${CMAKE_DEPFILE_FLAGS_C}\")\n"
            "STRING(REPLACE \"-MD\" \"-MMD\" CMAKE_DEPFILE_FLAGS_CXX \"\${CMAKE_DEPFILE_FLAGS_CXX}\")\n"
    )
    set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_BINARY_DIR}/GNUMakeRulesOverwrite.cmake" CACHE INTERNAL "")
endif()

message("Building in " ${CMAKE_BUILD_TYPE} " mode")

project(vulkan-playground)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

set(SOURCES src/main.cpp src/engine.cpp include/vulkan_init.h include/types.h)

add_executable(vulkan-playground ${SOURCES})

target_include_directories(vulkan-playground PRIVATE include/)

target_compile_features(vulkan-playground PRIVATE cxx_std_17)

find_package(Vulkan REQUIRED)
target_link_libraries(vulkan-playground Vulkan::Vulkan)

add_subdirectory(vk-bootstrap)
target_link_libraries(vulkan-playground vk-bootstrap)

target_include_directories(vulkan-playground PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(vulkan-playground PUBLIC ${PROJECT_SOURCE_DIR}/glm)

add_subdirectory(${PROJECT_SOURCE_DIR}/glfw)
target_include_directories(vulkan-playground PUBLIC ${PROJECT_SOURCE_DIR}/glfw/include)
target_link_libraries(vulkan-playground glfw ${GLFW_LIBRARIES})

target_link_libraries(vulkan-playground gdi32)

add_dependencies(vulkan-playground glfw)