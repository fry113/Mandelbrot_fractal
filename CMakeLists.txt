cmake_minimum_required(VERSION 3.30)
project(MandelbrotFractal  VERSION 1.0.0 LANGUAGES CXX)

cmake_policy(SET CMP0169 OLD)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Не больше одной ошибки за раз
    add_compile_options(-fmax-errors=1)
endif()

# Ищем необходимые библиотеки
find_package(GTest REQUIRED)
find_package(SFML COMPONENTS graphics window system REQUIRED)
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
endif()

include(FetchContent)
FetchContent_Declare(
  CPM
  GIT_REPOSITORY https://github.com/cpm-cmake/CPM.cmake
  GIT_TAG origin/master
)
FetchContent_MakeAvailable(CPM)

CPMAddPackage(
    NAME stdexec
    GITHUB_REPOSITORY NVIDIA/stdexec
    GIT_TAG main # This will always pull the latest code from the `main` branch. You may also use a specific release version or tag
    OPTIONS
        "STDEXEC_BUILD_EXAMPLES OFF"
        "STDEXEC_BUILD_TESTS OFF"
)

file(GLOB HEADER_FILES "${CMAKE_SOURCE_DIR}/include/*.hpp")
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/main.cpp")

# Создаём статическую библиотеку
add_library(${PROJECT_NAME}_imp STATIC "${CMAKE_SOURCE_DIR}/src/main.cpp" ${SRC_FILES} ${HEADER_FILES})

# Добавляем в проект используемые сторонние PRIVATE
target_include_directories(${PROJECT_NAME}_imp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${stdexec_SOURCE_DIR}/include
    ${SFML_INCLUDE_DIR}
)
message(STATUS "matplotplusplus_SOURCE_DIR = ${matplotplusplus_SOURCE_DIR}")
target_link_libraries(${PROJECT_NAME}_imp PUBLIC
    STDEXEC::stdexec
    sfml-graphics sfml-window sfml-system)

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME}_imp PUBLIC X11::X11)
endif()

# Создаём исполняемый таргет и линкуем к нему статическую библиотеку
add_executable(${PROJECT_NAME} "${CMAKE_SOURCE_DIR}/src/main.cpp")
target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}_imp)

#
# Тесты
#
# Рекурсивно ищем все используемые в тестах .cpp файлы
file(GLOB TEST_SRC_FILES "${CMAKE_SOURCE_DIR}/tests/*.cpp")

add_executable(${PROJECT_NAME}_tests "${TEST_SRC_FILES}")
target_link_libraries(${PROJECT_NAME}_tests PRIVATE
    ${PROJECT_NAME}_imp
    GTest::GTest
    GTest::Main
    STDEXEC::stdexec
    sfml-graphics sfml-window sfml-system)

target_include_directories(${PROJECT_NAME}_tests PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${stdexec_SOURCE_DIR}/include
    ${matplotplusplus_SOURCE_DIR}/source)

# Включаем тестирование
enable_testing()
add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)

#
# ASan build helper target
#
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_custom_target(asan-build
    COMMAND conan install ${CMAKE_SOURCE_DIR}
        -of ${CMAKE_SOURCE_DIR}/build-asan
        -s build_type=Debug
        -b missing
        COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_SOURCE_DIR}/build-asan/build
                -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/build-asan/build/conan/conan_toolchain.cmake
            -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
                -DCMAKE_CXX_FLAGS=-fsanitize=address\ -fno-omit-frame-pointer\ -g
                -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address
                -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_SOURCE_DIR}/build-asan/build --parallel
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Configure and build ASan/UBSan variant in build-asan"
        VERBATIM
    )
endif()
