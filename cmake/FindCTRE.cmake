if (NOT TARGET CTRE::CTRE)

find_path(CTRE_INCLUDE_DIR
	NAMES ctre.hpp
	HINTS ${CTRE_ROOT}/include
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(CTRE DEFAULT_MSG CTRE_INCLUDE_DIR)

if (CTRE_FOUND)
	add_library(CTRE::CTRE INTERFACE IMPORTED GLOBAL)
	set_target_properties(CTRE::CTRE PROPERTIES IMPORTED_NAME CTRE INTERFACE_INCLUDE_DIRECTORIES "${CTRE_INCLUDE_DIR}")
	mark_as_advanced(CTRE_INCLUDE_DIR)
endif()

endif()
