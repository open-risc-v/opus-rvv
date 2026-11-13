# 上游接入：opus（CMake 原生）→ <build>/pkg/prefix，探针链接静态 libopus。
# 固定点构建，见 cmake/pkg-build.sh 的注释。

set(PKG_ROOT "${CMAKE_BINARY_DIR}/pkg")
if(NOT DEFINED PKG_PREFIX)
  set(PKG_PREFIX "${PKG_ROOT}/prefix")
  add_custom_target(pkg ALL
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pkg-build.sh" "${SRC_DIR}" "${PKG_ROOT}"
    COMMENT "构建上游 opus（${SRC_DIR}）"
    VERBATIM)
  set(HAVE_PKG_TARGET TRUE)
endif()

add_executable(opus_harness test/pkg_opus.c)
if(HAVE_PKG_TARGET)
  add_dependencies(opus_harness pkg)
endif()
target_include_directories(opus_harness PRIVATE "${PKG_PREFIX}/include/opus")
target_link_directories(opus_harness PRIVATE "${PKG_PREFIX}/lib")
target_link_libraries(opus_harness PRIVATE opus m)
target_compile_options(opus_harness PRIVATE ${RVV_FLAGS})

add_test(NAME opus_harness COMMAND opus_harness)
