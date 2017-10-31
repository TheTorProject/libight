## measurement_kit

file(
  GLOB
  MK_PROGRAM_SOURCES
  "${MK_ROOT}/src/measurement_kit/*.cpp"
  "${MK_ROOT}/src/measurement_kit/*/*.cpp"
)

add_executable(
  measurement_kit_exe
  ${MK_PROGRAM_SOURCES}
)
target_link_libraries(
  measurement_kit_exe
  measurement_kit_static
  ${MK_LIBS}
  Threads::Threads
)
