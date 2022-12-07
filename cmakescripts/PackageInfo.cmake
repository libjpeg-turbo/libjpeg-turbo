# This file is included from the top-level CMakeLists.txt.  We just store it
# here to avoid cluttering up that file.

set(PKGNAME ${PROJECT_NAME} CACHE STRING
  "Distribution package name (default: ${PROJECT_NAME})")
set(PKGVENDOR "The ${PROJECT_NAME} Project" CACHE STRING
  "Vendor name to be included in distribution package descriptions (default: The ${PROJECT_NAME} Project)")
set(PKGURL "http://www.${PROJECT_NAME}.org" CACHE STRING
  "URL of project web site to be included in distribution package descriptions (default: http://www.${PROJECT_NAME}.org)")
set(PKGEMAIL "information@${PROJECT_NAME}.org" CACHE STRING
  "E-mail of project maintainer to be included in distribution package descriptions (default: information@${PROJECT_NAME}.org")
set(PKGID "com.${PROJECT_NAME}.${PKGNAME}" CACHE STRING
  "Globally unique package identifier (reverse DNS notation) (default: com.${PROJECT_NAME}.${PKGNAME})")
