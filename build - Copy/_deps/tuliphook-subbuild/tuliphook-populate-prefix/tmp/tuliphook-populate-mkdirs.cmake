# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-src")
  file(MAKE_DIRECTORY "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-build"
  "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-subbuild/tuliphook-populate-prefix"
  "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-subbuild/tuliphook-populate-prefix/tmp"
  "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-subbuild/tuliphook-populate-prefix/src/tuliphook-populate-stamp"
  "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-subbuild/tuliphook-populate-prefix/src"
  "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-subbuild/tuliphook-populate-prefix/src/tuliphook-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-subbuild/tuliphook-populate-prefix/src/tuliphook-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/Benno111/Documents/GitHub/xdBot/build/_deps/tuliphook-subbuild/tuliphook-populate-prefix/src/tuliphook-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
