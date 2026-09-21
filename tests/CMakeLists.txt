# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: CC0-1.0

find_package(Qt${QT_VERSION_MAJOR} ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Core Test)

add_executable(timeformat_test
    timeformat_test.cpp
    ../plugins/dde-dock/datetime/timeformat.cpp
)
target_include_directories(timeformat_test PRIVATE ../plugins/dde-dock/datetime)
target_link_libraries(timeformat_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME timeformat_test COMMAND timeformat_test)
