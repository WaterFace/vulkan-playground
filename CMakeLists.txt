cmake_minimum_required(VERSION 3.10)

project(vulkan-playground)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_executable(vulkan-playground src/main.cpp)

target_compile_features(vulkan-playground PRIVATE cxx_std_17)

find_package(Vulkan REQUIRED)
target_link_libraries(vulkan-playground Vulkan::Vulkan)

target_include_directories(vulkan-playground PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(vulkan-playground PUBLIC ${PROJECT_SOURCE_DIR}/glm)

add_subdirectory(${PROJECT_SOURCE_DIR}/glfw)
target_include_directories(vulkan-playground PUBLIC ${PROJECT_SOURCE_DIR}/glfw/include)
target_link_libraries(vulkan-playground glfw ${GLFW_LIBRARIES})

add_dependencies(vulkan-playground glfw)