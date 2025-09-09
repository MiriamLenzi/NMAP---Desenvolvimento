cmake_minimum_required(VERSION 3.16)
project(netscan_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Geração automática do MOC/UIC/RCC p/ classes com Q_OBJECT
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6 (se seu kit for Qt5, troque para Qt5::Widgets)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

add_executable(netscan_gui
  main.cpp
)

target_link_libraries(netscan_gui PRIVATE Qt6::Widgets)

if(WIN32)
  target_link_libraries(netscan_gui PRIVATE ws2_32)
endif()

if(MSVC)
  target_compile_options(netscan_gui PRIVATE /W4 /O2)
else()
  target_compile_options(netscan_gui PRIVATE -O2 -Wall -Wextra -pedantic)
endif()

