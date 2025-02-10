# - Try to find the LDAP client libraries
# Once done this will define
#
#  LDAP_FOUND - system has libldap
#  LDAP_INCLUDE_DIR - the ldap include directory
#  LDAP_LIBRARIES - libldap + liblber library

set(CMAKE_FIND_FRAMEWORK LAST)
find_path(LDAP_INCLUDE_DIR ldap.h Winldap.h)
find_library(LDAP_LIBRARY NAMES ldap Wldap32)
find_library(LBER_LIBRARY NAMES lber)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LDAP DEFAULT_MSG LDAP_LIBRARY)

if(LDAP_FOUND)
	if(LBER_LIBRARY)
		set(LDAP_LIBRARIES ${LDAP_LIBRARY} ${LBER_LIBRARY})
	else()
		set(LDAP_LIBRARIES ${LDAP_LIBRARY})
	endif()
endif()

mark_as_advanced(LDAP_INCLUDE_DIR LDAP_LIBRARY LBER_LIBRARY)
