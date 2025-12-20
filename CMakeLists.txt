cmake_minimum_required(VERSION 3.16)

project(TransportSecureSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Ищем Qt компоненты
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql WebEngineWidgets WebChannel Network)

# 2. Ищем OpenSSL (ОБЯЗАТЕЛЬНО для SMTP/почты на Linux)
find_package(OpenSSL REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 3. Собираем исходники
# ВАЖНО: Мы ищем файлы в папках core, gui, models и корень (main.cpp),
# так как именно там мы создавали файлы в предыдущих шагах.
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS 
    "include/*.h"
)

set(RESOURCES resources/resources.qrc)

# 4. Создаем исполняемый файл
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE include)

# 5. Линковка библиотек
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Sql
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Network
    
    # Добавляем SSL для работы почты
    OpenSSL::SSL
    OpenSSL::Crypto
)