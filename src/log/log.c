#include <stdbool.h>
#include <stdio.h>
#include "log.h"

FILE *log_error_stream = NULL;
bool log_error_stream_set = false;

void log_set_stream (FILE *stream) {
	log_error_stream = stream;
	log_error_stream_set = true;
}

FILE *log_get_stream() {
	if ( log_error_stream_set ) {
		return log_error_stream;
	}
	return stderr;
}