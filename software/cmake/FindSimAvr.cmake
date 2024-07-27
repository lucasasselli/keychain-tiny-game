find_program(SIMAVR
	NAME
		simavr

	PATHS
		/usr/bin/
		$ENV{SIMAVR_HOME}
)

if(NOT SIMAVR)
	message("-- Could not find simavr")
else(NOT SIMAVR)
	message("-- Found simavr: ${SIMAVR}")
endif(NOT SIMAVR)

find_path(SIMAVR_INCLUDE_DIR
	NAMES
		"sim_avr.h"

	PATHS
    /usr/include/simavr
	/usr/local/include/simavr
)
