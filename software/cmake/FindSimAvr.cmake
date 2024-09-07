find_program(SIMAVR
	NAME
		simavr

	PATHS
		$ENV{SIMAVR_HOME}
        /usr/bin/
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
	$ENV{SIMAVR_HOME}/include/simavr
    /usr/include/simavr
	/usr/local/include/simavr
)
