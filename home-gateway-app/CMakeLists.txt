cmake_minimum_required(VERSION 3.5)

project(home-gateway-app LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/src/ui)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets Network REQUIRED)

file(GLOB_RECURSE SRC_FILES
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_SOURCE_DIR}/src/*.h"
)

set(UI_FILES
  ${CMAKE_SOURCE_DIR}/src/ui/MainDashboard.ui
  ${CMAKE_SOURCE_DIR}/src/ui/SettingsDashboard.ui
  ${CMAKE_SOURCE_DIR}/src/ui/FirmwareUpdatePopup.ui
  ${CMAKE_SOURCE_DIR}/src/ui/FirmwareUpdateProgress.ui
  ${CMAKE_SOURCE_DIR}/src/ui/StatusHeader.ui
)

set(QRC_FILES
  ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)

if(ANDROID)
  add_library(home-gateway-app SHARED
    ${SRC_FILES}
    ${UI_FILES}
    ${QRC_FILES}
  )
else()
  add_executable(home-gateway-app
    ${SRC_FILES}
    ${UI_FILES}
    ${QRC_FILES}
  )
endif()

find_library(MOSQUITTO_LIB mosquitto PATHS ${CMAKE_SYSROOT}/usr/lib)
target_link_libraries(home-gateway-app PRIVATE Qt5::Widgets Qt5::Network ${MOSQUITTO_LIB})
target_include_directories(home-gateway-app PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/ui
  ${CMAKE_SOURCE_DIR}/src/ota
)

set(INSTALL_DESTDIR "/opt")

install(TARGETS home-gateway-app
    RUNTIME DESTINATION "${INSTALL_DESTDIR}"
    BUNDLE DESTINATION "${INSTALL_DESTDIR}"
    LIBRARY DESTINATION "${INSTALL_DESTDIR}"
)
