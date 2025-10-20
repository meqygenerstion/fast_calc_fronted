cmake_minimum_required(VERSION 3.14)
project(fast_calc)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Intl REQUIRED)

function(target_link_with_intl target)
  if (TARGET Intl::Intl)
    target_link_libraries(${target} PRIVATE Intl::Intl)
  else()
    target_link_libraries(${target} PRIVATE ${Intl_LIBRARIES})
    if (Intl_INCLUDE_DIRS)
      target_include_directories(${target} PRIVATE ${Intl_INCLUDE_DIRS})
    endif()
  endif()
endfunction()

# # --- FetchContent для FTXUI ---
include(FetchContent)
include(CTest)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9
)

FetchContent_MakeAvailable(ftxui)

include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(catch2)

add_executable(fast_calc
src/main.cpp
src/ui/main_screen.cpp
src/ui/calc_screen.cpp
src/ui/text_screen.cpp
src/ui/history_now_screen.cpp
src/core/file_manager.cpp
src/core/history_manager.cpp
src/core/localization.cpp
src/core/config_manager.cpp
src/core/detect_locale.cpp

)

target_link_libraries(fast_calc
  PRIVATE tomlplusplus::tomlplusplus
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)
target_link_with_intl(fast_calc)

if (APPLE)
  target_link_libraries(fast_calc PRIVATE "-framework CoreFoundation")
endif()

enable_testing()

add_executable(file_manager_tests
  tests/file_manager_tests.cpp
  src/core/file_manager.cpp
)

target_link_libraries(file_manager_tests
  PRIVATE tomlplusplus::tomlplusplus
  PRIVATE ftxui::component
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE Catch2::Catch2WithMain
)

if (BUILD_TESTING)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)
  catch_discover_tests(file_manager_tests)
endif()

add_executable(history_manager_tests
  tests/history_manager_tests.cpp
  src/core/history_manager.cpp
  src/core/file_manager.cpp
)

target_link_libraries(history_manager_tests
  PRIVATE tomlplusplus::tomlplusplus
  PRIVATE ftxui::component
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE Catch2::Catch2WithMain
)

if (BUILD_TESTING)
  catch_discover_tests(history_manager_tests)
endif()

add_executable(config_manager_tests
  tests/config_manager_tests.cpp
  src/core/config_manager.cpp
  src/core/file_manager.cpp
)

target_link_libraries(config_manager_tests
  PRIVATE tomlplusplus::tomlplusplus
  PRIVATE ftxui::component
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE Catch2::Catch2WithMain
)

if (BUILD_TESTING)
  catch_discover_tests(config_manager_tests)
endif()

add_executable(localization_manager_tests
  tests/localization_manager_tests.cpp
  src/core/localization.cpp
  src/core/file_manager.cpp
)

target_link_libraries(localization_manager_tests
  PRIVATE tomlplusplus::tomlplusplus
  PRIVATE ftxui::component
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE Catch2::Catch2WithMain
)
target_link_with_intl(localization_manager_tests)

if (APPLE)
  target_link_libraries(localization_manager_tests PRIVATE "-framework CoreFoundation")
endif()

if (BUILD_TESTING)
  catch_discover_tests(localization_manager_tests)
endif()
