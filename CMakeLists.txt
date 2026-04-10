cmake_minimum_required(VERSION 3.12)

project(lock_free_ds VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)

FetchContent_MakeAvailable(googletest)

#-----------------Main executable-----------------

add_executable(${PROJECT_NAME}
    main.cpp
    # src/lock_free_stack.cpp
    # src/lock_free_queue.cpp
    # src/lock_free_list.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(${PROJECT_NAME}
    PRIVATE -Wall -Wextra -Wpedantic
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

#-------------------------------------------------

#-----------------Test executable Stack-----------------

add_executable(${PROJECT_NAME}_stack_test
    # src/lock_free_stack.cpp
    # src/lock_free_queue.cpp
    # src/lock_free_list.cpp
    tests/lock_free_stack_test.cpp
    # tests/lock_free_queue_test.cpp
    # tests/lock_free_list_test.cpp
)

target_link_libraries(${PROJECT_NAME}_stack_test
    PRIVATE GTest::gtest_main
)

target_include_directories(${PROJECT_NAME}_stack_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(${PROJECT_NAME}_stack_test PRIVATE DEBUG)

target_compile_options(${PROJECT_NAME}_stack_test
    PRIVATE -Wall -Wextra -Wpedantic -fsanitize=address
)

target_link_options(${PROJECT_NAME}_stack_test
    PRIVATE -fsanitize=address
)

set_target_properties(${PROJECT_NAME}_stack_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/bin
)

#-------------------------------------------------


#-----------------Test executable Queue-----------------

add_executable(${PROJECT_NAME}_queue_test
    # src/lock_free_stack.cpp
    # src/lock_free_queue.cpp
    # src/lock_free_list.cpp
    tests/lock_free_queue_test.cpp
    # tests/lock_free_stack_test.cpp
    # tests/lock_free_list_test.cpp
)

target_link_libraries(${PROJECT_NAME}_queue_test
    PRIVATE GTest::gtest_main
)

target_include_directories(${PROJECT_NAME}_queue_test
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(${PROJECT_NAME}_queue_test PRIVATE DEBUG)

target_compile_options(${PROJECT_NAME}_queue_test
    PRIVATE -Wall -Wextra -Wpedantic -fsanitize=address
)

target_link_options(${PROJECT_NAME}_queue_test
    PRIVATE -fsanitize=address
)

set_target_properties(${PROJECT_NAME}_queue_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/bin
)

#-------------------------------------------------