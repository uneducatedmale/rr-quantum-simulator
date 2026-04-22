#ifndef PARSER_H
#define PARSER_H

#include "process.h"

int load_workload(const char *path, Process **processes_out, int *count_out);

#endif
