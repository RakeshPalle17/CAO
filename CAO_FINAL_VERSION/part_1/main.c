/*
 * main.c
 *
 * Author:
 * Copyright (c) 2020, Gaurav Kothari (gkothar1@binghamton.edu)
 * State University of New York at Binghamton
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apex_cpu.h"

int
main(int argc, char const *argv[])
{
    APEX_CPU *cpu;

    fprintf(stderr, "APEX CPU Pipeline Simulator v%0.1lf\n", VERSION);

    if (argc < 4)
    {
        fprintf(stderr, "APEX_Help: Usage %s <input_file> simulate <n>\n", argv[0]);
        exit(1);
    }

    cpu = APEX_cpu_init(argv[1]);
    if (!cpu)
    {
        fprintf(stderr, "APEX_Error: Unable to initialize CPU\n");
        exit(1);
    }
    if(argc > 2)
    {
        if(!strcmp(argv[2],"simulate"))
        {
            cpu->num_of_cycles_to_run = atoi(argv[3]);
        }
    }

    APEX_cpu_run(cpu);
    APEX_cpu_stop(cpu);
    return 0;
}