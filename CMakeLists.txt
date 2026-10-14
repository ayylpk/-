cmake_minimum_required(VERSION 3.20)

project(jianqie LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# AUTOUIC 特意不开：界面全是代码搭的，项目里没有 .ui 文件。
# 开着它 CMake 会在配置阶段去试跑 uic.exe，白白多一个可能失败的环节。
# 以后真要用 Qt Designer 的 .ui 文件，再把下面这行放开。
# set(CMAKE_AUTOUIC ON)

# Qt6 位置：MSYS2 的 UCRT64 环境。
# 如果 CLion 的 CMake profile 里已经显式传了 CMAKE_PREFIX_PATH，这里不会覆盖。
if (NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql)

# 打开这个开关会保留控制台窗口，方便看 qDebug 输出。
option(JIANQIE_CONSOLE "为调试保留控制台窗口" OFF)

set(JIANQIE_SOURCES
        src/main.cpp

        src/app/Application.h
        src/app/Application.cpp

        src/core/ClipboardItem.h
        src/core/SettingsStore.h
        src/core/SettingsStore.cpp
        src/core/StorageManager.h
        src/core/StorageManager.cpp
        src/core/ClipboardMonitor.h
        src/core/ClipboardMonitor.cpp
        src/core/GlobalHotkey.h
        src/core/GlobalHotkey.cpp

        src/ui/ThemeManager.h
        src/ui/ThemeManager.cpp
        src/ui/ClipboardItemDelegate.h
        src/ui/ClipboardItemDelegate.cpp
        src/ui/MainWindow.h
        src/ui/MainWindow.cpp
        src/ui/ImageFloatWindow.h
        src/ui/ImageFloatWindow.cpp
        src/ui/SettingsWindow.h
        src/ui/SettingsWindow.cpp

        resources/resources.qrc
)

add_executable(jianqie ${JIANQIE_SOURCES})

if (NOT JIANQIE_CONSOLE)
    set_target_properties(jianqie PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(jianqie PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(jianqie PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Sql
)

if (WIN32)
    # RegisterHotKey / SendInput / 托盘图标都来自 Win32
    target_link_libraries(jianqie PRIVATE user32)
endif()

# ---------------------------------------------------------------------------
# 打包（要发给自己以外的人时再跑，第一版用不到）
#
#   cmake --build build --target jianqie
#   "C:/msys64/ucrt64/bin/windeployqt.exe" --release --no-translations <exe路径>
#
# 注意：运行时 Qt 的插件目录是靠 Qt 装的时候编译进去的绝对路径找到的，
# 在 CLion 里直接 Run 时，建议把 C:/msys64/ucrt64/bin 加进运行配置的 PATH。
# ---------------------------------------------------------------------------
