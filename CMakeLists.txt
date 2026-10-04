cmake_minimum_required(VERSION 3.20)
project(jobStarr VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Test)
find_package(yaml-cpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)

# Core library
add_library(jobstarr_core STATIC
    src/UrlValidator.h
    src/UrlValidator.cpp
    src/JobRecord.h
    src/JobRecord.cpp
    src/JobJudgeResult.h
    src/JobJudgeResult.cpp
    src/PromptRenderer.h
    src/PromptRenderer.cpp
    src/ConfigManager.h
    src/ConfigManager.cpp
    src/JobSpyClient.h
    src/JobSpyClient.cpp
    src/LlmClient.h
    src/LlmClient.cpp
    src/ProjectPaths.h
    src/ProjectPaths.cpp
    src/CandidateProfile.h
    src/CandidateProfile.cpp
    src/FilenameSanitizer.h
    src/FilenameSanitizer.cpp
    src/MakeMaterialsResult.h
    src/MakeMaterialsParser.h
    src/MakeMaterialsParser.cpp
    src/CompanyAddressLookup.h
    src/CompanyAddressLookup.cpp
    src/OdtTemplateRenderer.h
    src/OdtTemplateRenderer.cpp
    src/PdfConverter.h
    src/PdfConverter.cpp
    src/OutputManager.h
    src/OutputManager.cpp
    src/MakeMaterialsController.h
    src/MakeMaterialsController.cpp
    resources/jobstarr.qrc
)

target_include_directories(jobstarr_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(jobstarr_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    yaml-cpp::yaml-cpp
    PkgConfig::LIBZIP
)

target_compile_definitions(jobstarr_core PUBLIC
    JOBSTARR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Main GUI Executable: must be named jobStarr
add_executable(jobStarr
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/ConfigurationWidget.h
    src/ConfigurationWidget.cpp
)

target_link_libraries(jobStarr PRIVATE
    jobstarr_core
    Qt6::Widgets
    Qt6::Gui
)

# Copy python bridge + the bundled JobSpy package next to the compiled executable,
# so the installed/built app is self-contained and needs no sibling repository.
add_custom_command(TARGET jobStarr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/jobspy_bridge.py
        $<TARGET_FILE_DIR:jobStarr>/jobspy_bridge.py
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/jobspy
        $<TARGET_FILE_DIR:jobStarr>/jobspy
    COMMENT "Copying jobspy_bridge.py and bundled jobspy/ next to jobStarr executable"
)

# Automated Unit Tests
enable_testing()

macro(add_jobstarr_test test_name test_source)
    add_executable(${test_name} tests/${test_source})
    target_link_libraries(${test_name} PRIVATE
        jobstarr_core
        Qt6::Test
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endmacro()

add_jobstarr_test(test_url_validation test_url_validation.cpp)
add_jobstarr_test(test_prompt_renderer test_prompt_renderer.cpp)
add_jobstarr_test(test_job_judge_parser test_job_judge_parser.cpp)
add_jobstarr_test(test_config test_config.cpp)
add_jobstarr_test(test_scraper_boundary test_scraper_boundary.cpp)
add_jobstarr_test(test_llm_boundary test_llm_boundary.cpp)

# Version 0.2.0 Unit Tests
add_jobstarr_test(test_make_materials_parser test_make_materials_parser.cpp)
add_jobstarr_test(test_filename_sanitizer test_filename_sanitizer.cpp)
add_jobstarr_test(test_candidate_profile test_candidate_profile.cpp)
add_jobstarr_test(test_address_lookup test_address_lookup.cpp)
add_jobstarr_test(test_odt_renderer test_odt_renderer.cpp)
add_jobstarr_test(test_pdf_converter test_pdf_converter.cpp)
add_jobstarr_test(test_transactional_output test_transactional_output.cpp)
