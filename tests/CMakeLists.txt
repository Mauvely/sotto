find_package(Qt6 6.4 REQUIRED COMPONENTS Test)

qt_add_executable(test_formatter
    test_formatter.cpp
    ${CMAKE_SOURCE_DIR}/src/format/TextFormatter.cpp
)
target_include_directories(test_formatter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_formatter PRIVATE Qt6::Core Qt6::Test)
add_test(NAME formatter COMMAND test_formatter)

qt_add_executable(test_speechgate test_speechgate.cpp)
target_include_directories(test_speechgate PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_speechgate PRIVATE Qt6::Core Qt6::Test)
add_test(NAME speechgate COMMAND test_speechgate)
