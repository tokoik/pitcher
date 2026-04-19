cmake_minimum_required(VERSION 3.15)

# プログラム名をプロジェクト名と同じにする
project(pitcher C)

if(APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE "pitch-stereo.c")
    # Xcode でプロジェクトを開いたときの起動ターゲットを pitcher にする
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY XCODE_STARTUP_PROJECT ${PROJECT_NAME})
else()
    add_executable(${PROJECT_NAME} "pitch-stereo.c")
endif()

if(WIN32)
    # Windows: プログラム名をスタートアッププロジェクトに設定
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

    # freeglut をビルドするための設定
    set(FREEGLUT_BUILD_DEMOS OFF CACHE BOOL "Do not build freeglut demos" FORCE)
    set(FREEGLUT_BUILD_SHARED_LIBS ON CACHE BOOL "Build freeglut as shared library" FORCE)
    set(FREEGLUT_BUILD_STATIC_LIBS OFF CACHE BOOL "Do not build freeglut static library" FORCE)

    # freeglut のソースを GitHub からダウンロードしてビルド
    include(FetchContent)
    FetchContent_Declare(
        freeglut
        GIT_REPOSITORY "https://github.com/freeglut/freeglut.git"
        GIT_TAG "master"
    )
    FetchContent_MakeAvailable(freeglut)

    # freeglut のインクルードディレクトリを追加 (glut.h を見つけるため)
    target_include_directories(${PROJECT_NAME} PRIVATE "${freeglut_SOURCE_DIR}/include" "${freeglut_SOURCE_DIR}/include/GL")

    # freeglut ターゲットと OpenGL をリンク
    target_link_libraries(${PROJECT_NAME} PRIVATE freeglut opengl32.lib)

    # 実行時に DLL が見つかるようにビルドディレクトリへコピー
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:freeglut>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

elseif(APPLE)
    # macOS: OpenGL と GLUT フレームワークを検索してリンク
    find_library(OPENGL_FRAMEWORK OpenGL)
    find_library(GLUT_FRAMEWORK GLUT)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${OPENGL_FRAMEWORK}
        ${GLUT_FRAMEWORK}
    )

elseif(UNIX)
    # Ubuntu Linux: pkg-config を使ってコンパイル・リンクのオプションを決定
    find_package(PkgConfig REQUIRED)

    pkg_search_module(PKG_GLUT REQUIRED glut freeglut)
    pkg_search_module(PKG_GL REQUIRED gl)

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${PKG_GLUT_INCLUDE_DIRS}
        ${PKG_GL_INCLUDE_DIRS}
    )
    target_link_directories(${PROJECT_NAME} PRIVATE
        ${PKG_GLUT_LIBRARY_DIRS}
        ${PKG_GL_LIBRARY_DIRS}
    )
    target_compile_options(${PROJECT_NAME} PRIVATE
        ${PKG_GLUT_CFLAGS_OTHER}
        ${PKG_GL_CFLAGS_OTHER}
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${PKG_GLUT_LIBRARIES}
        ${PKG_GL_LIBRARIES}
    )
endif()
