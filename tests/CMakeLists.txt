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

# An autostart entry written under the previous app id moves to the new one.
# Linux only, like the migration. Settings.cpp is compiled in rather than the
# whole app; APP_ID and APP_PREVIOUS_ID reach this target through EXTRA_TARGETS
# in the mauvely_app_info() call in the top-level CMakeLists.txt.
if(NOT WIN32)
    qt_add_executable(test_autostart_migration
        test_autostart_migration.cpp
        ${CMAKE_SOURCE_DIR}/src/core/Settings.cpp
    )
    target_include_directories(test_autostart_migration PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_autostart_migration PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME autostart_migration COMMAND test_autostart_migration)
    set_tests_properties(autostart_migration PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
