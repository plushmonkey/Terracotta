# Locate the mclib library
#
# This module defines the following variables:
#
# MCLIB_LIBRARY the name of the library;
# MCLIB_INCLUDE_DIR where to find mclib include files.
# MCLIB_FOUND true if both the MCLIB_LIBRARY and MCLIB_INCLUDE_DIR have been found.
#
# To help locate the library and include file, you can define a
# variable called MCLIB_ROOT which points to the root of the mclib library
# installation.

set( _mclib_HEADER_SEARCH_DIRS
"/usr/include"
"/usr/local/include"
"${CMAKE_SOURCE_DIR}/includes"
"C:/Program Files (x86)/mclib/include" )
set( _mclib_LIB_SEARCH_DIRS
"/usr/lib"
"/usr/local/lib"
"${CMAKE_SOURCE_DIR}/lib"
"C:/Program Files (x86)/mclib/lib" )

# Check environment for root search directory
set( _mclib_ENV_ROOT $ENV{MCLIB_ROOT} )
if( NOT MCLIB_ROOT AND _mclib_ENV_ROOT )
	set(MCILB_ROOT ${_mclib_ENV_ROOT} )
endif()

# Put user specified location at beginning of search
if( MCLIB_ROOT )
	list( INSERT _mclib_HEADER_SEARCH_DIRS 0 "${MCLIB_ROOT}/include" )
	list( INSERT _mclib_LIB_SEARCH_DIRS 0 "${MCLIB_ROOT}/lib" )
endif()

# Search for the header
FIND_PATH(MCLIB_INCLUDE_DIR "mclib/mclib.h"
PATHS ${_mclib_HEADER_SEARCH_DIRS} )

# Search for the library
FIND_LIBRARY(MCLIB_LIBRARY NAMES mclib
PATHS ${_mclib_LIB_SEARCH_DIRS} )
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MCLIB DEFAULT_MSG
MCLIB_LIBRARY MCLIB_INCLUDE_DIR)