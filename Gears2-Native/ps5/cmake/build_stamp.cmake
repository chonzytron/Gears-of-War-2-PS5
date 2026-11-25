# Gears of War 2 for PS5 - which program this is, written into gow2_build_stamp.h
# at every build (the app logs it at start: a console run's log then says which
# program it was). In a git checkout, the commit ("+" after it: files changed
# since). Outside one (COGBox ships without git), "src-" and a hash of the PS5
# sources and patches, so two programs with the same stamp have the same code
# of ours. The header only changes, and the app only recompiles, when it does.
# (After the Dead Space 2 PS5 port's build_stamp.cmake.)
#   cmake -DGOW2_REPO=<dir> -DGOW2_PS5_DIR=<ps5 dir> -DGOW2_STAMP=<header> -P build_stamp.cmake
execute_process(COMMAND git -C "${GOW2_REPO}" rev-parse --short=7 HEAD
                OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(commit STREQUAL "")
  file(GLOB_RECURSE inputs "${GOW2_PS5_DIR}/src/*" "${GOW2_PS5_DIR}/sdk/*.patch" "${GOW2_PS5_DIR}/CMakeLists.txt")
  list(SORT inputs)
  set(all "")
  foreach(f IN LISTS inputs)
    file(SHA256 "${f}" h)
    string(APPEND all "${h}")
  endforeach()
  string(SHA256 digest "${all}")
  string(SUBSTRING "${digest}" 0 10 short)
  set(commit "src-${short}")
else()
  execute_process(COMMAND git -C "${GOW2_REPO}" status --porcelain --untracked-files=no
                  OUTPUT_VARIABLE changes OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(NOT changes STREQUAL "")
    string(APPEND commit "+")
  endif()
endif()
set(text "#define GOW2_PS5_BUILD \"${commit}\"\n")
if(EXISTS "${GOW2_STAMP}")
  file(READ "${GOW2_STAMP}" old)
endif()
if(NOT old STREQUAL text)
  file(WRITE "${GOW2_STAMP}" "${text}")
endif()
