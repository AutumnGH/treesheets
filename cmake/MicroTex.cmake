# MicroTeX (a lightweight, embeddable LaTeX math renderer) for TeX formulas in cells, see
# src/microtex.h. Included from the top-level CMakeLists.txt after the TreeSheets target exists.

FetchContent_Declare(
    tinyxml2
    URL https://github.com/leethomason/tinyxml2/archive/refs/tags/11.0.0.tar.gz
    URL_HASH SHA256=5556deb5081fb246ee92afae73efd943c889cef0cafea92b0b82422d6a18f289
)
FetchContent_MakeAvailable(tinyxml2)

FetchContent_Declare(
    microtex
    URL https://github.com/NanoMichael/MicroTeX/archive/0e3707f6dafebb121d98b53c64364d16fefe481d.tar.gz
    URL_HASH SHA256=47476269d29c41df322bce6bdd2daa7017cc50b9eda841b2bec1767dba28daa6
)
# Only the core sources are built below, so fetch without running MicroTeX's own CMakeLists, which
# would require pkg-config and configure its optional GUI backends.
cmake_policy(PUSH)
cmake_policy(SET CMP0169 OLD)
FetchContent_GetProperties(microtex)
if(NOT microtex_POPULATED)
    FetchContent_Populate(microtex)
endif()
cmake_policy(POP)

file(GLOB microtex_sources CONFIGURE_DEPENDS
    "${microtex_SOURCE_DIR}/src/latex.cpp"
    "${microtex_SOURCE_DIR}/src/render.cpp"
    "${microtex_SOURCE_DIR}/src/atom/*.cpp"
    "${microtex_SOURCE_DIR}/src/box/*.cpp"
    "${microtex_SOURCE_DIR}/src/core/*.cpp"
    "${microtex_SOURCE_DIR}/src/fonts/*.cpp"
    "${microtex_SOURCE_DIR}/src/res/builtin/*.cpp"
    "${microtex_SOURCE_DIR}/src/res/font/*.cpp"
    "${microtex_SOURCE_DIR}/src/res/parser/*.cpp"
    "${microtex_SOURCE_DIR}/src/res/reg/*.cpp"
    "${microtex_SOURCE_DIR}/src/res/sym/*.cpp"
    "${microtex_SOURCE_DIR}/src/utils/*.cpp"
)
add_library(microtex STATIC ${microtex_sources})
target_include_directories(microtex PUBLIC "${microtex_SOURCE_DIR}/src")
target_link_libraries(microtex PRIVATE tinyxml2)
target_link_libraries(TreeSheets PRIVATE microtex)

# The Computer Modern math fonts (~0.5 MB in all) compiled into the executable; the glyph metrics
# are already in the microtex library, these files provide the glyph shapes. See src/microtex.cpp.
treesheets_embed_files(microtexfonts "${microtex_SOURCE_DIR}/res" "fonts/*.ttf")
