# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-src")
  file(MAKE_DIRECTORY "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-build"
  "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-subbuild/bindings-populate-prefix"
  "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-subbuild/bindings-populate-prefix/tmp"
  "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-subbuild/bindings-populate-prefix/src/bindings-populate-stamp"
  "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-subbuild/bindings-populate-prefix/src"
  "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-subbuild/bindings-populate-prefix/src/bindings-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-subbuild/bindings-populate-prefix/src/bindings-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/Benno111/Documents/GitHub/geobot/build/_deps/bindings-subbuild/bindings-populate-prefix/src/bindings-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
