set(EXE_NAME SkipMySong)

set(SOURCES 

    gsmtc/GsmtcWorker.cpp
    gsmtc/GsmtcWorker.hpp

    irc/IrcClient.cpp
    irc/IrcClient.hpp
    irc/IrcParser.cpp
    irc/IrcParser.hpp

    App.cpp
    App.hpp
    Settings.cpp
    Settings.hpp
    AppContext.hpp
    TwitchPanel.hpp
    TwitchPanel.cpp
    VoteEvent.cpp
    VoteEvent.hpp
)

add_executable(${EXE_NAME} WIN32 ${SOURCES})
set_target_properties(${EXE_NAME} 
    PROPERTIES 
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED On
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
target_link_libraries(${EXE_NAME} PRIVATE Boost::boost wx::core wx::base Microsoft::CppWinRT)
target_include_directories(${EXE_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(${EXE_NAME} PRIVATE /bigobj /EHsc)
target_compile_definitions(${EXE_NAME} PRIVATE _WIN32_WINNT=0x0A00)
