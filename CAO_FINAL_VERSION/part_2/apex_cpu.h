/*
 * apex_cpu.h
 * Contains APEX cpu pipeline declarations
 *
 * Author:
 * Copyright (c) 2020, Gaurav Kothari (gkothar1@binghamton.edu)
 * State University of New York at Binghamton
 */
#ifndef _APEX_CPU_H_
#define _APEX_CPU_H_

#include "apex_macros.h"

/* Format of an APEX instruction  */
typedef struct APEX_Instruction
{
    char opcode_str[128];
    int opcode;
    int rd;
    int rs1;
    int rs2;
    int imm;
} APEX_Instruction;

/* Model of CPU stage latch */
typedef struct CPU_Stage
{
    int pc;
    char opcode_str[128];
    int opcode;
    int rs1;
    int rs2;
    int rd;
    int imm;
    int rs1_value;
    int rs2_value;
    int result_buffer;
    int incrementor;
    int isJALR;
    int isJUMP;
    int memory_address;
    int has_insn;
    int decode_stall;
    int forwarded_data_value1;
    int forwarded_data_value2;

} CPU_Stage;


typedef struct RegisterStatusIndicator
{
    int isInvalid;
} RegisterStatusIndicator;

/* Model of APEX CPU */
typedef struct APEX_CPU
{
    int pc;                            /* Current program counter */
    int clock;                         /* Clock cycles elapsed */
    int insn_completed;                /* Instructions retired */
    int regs[REG_FILE_SIZE];           /* Integer register file */
    int code_memory_size;              /* Number of instruction in the input file */
    APEX_Instruction *code_memory;     /* Code Memory */
    int data_memory[DATA_MEMORY_SIZE]; /* Data Memory */
    int single_step;                   /* Wait for user input after every cycle */
    int zero_flag;                     /* {TRUE, FALSE} Used by BZ and BNZ to branch */
    int p_flag;
    int n_flag;
    int fetch_from_next_cycle;
    int decode_from_next_cycle;
    int forwared_from_execute;
    int forwared_from_memory;


    RegisterStatusIndicator regStatus[REG_FILE_SIZE];
    int writeToDestination;
    int writeToSource1;
    int writeToSource2;
    int num_of_cycles_to_run;


    /* Pipeline stages */
    CPU_Stage fetch;
    CPU_Stage decode;
    CPU_Stage execute;
    CPU_Stage memory;
    CPU_Stage writeback;
} APEX_CPU;

APEX_Instruction *create_code_memory(const char *filename, int *size);
APEX_CPU *APEX_cpu_init(const char *filename);
void APEX_cpu_run(APEX_CPU *cpu);
void APEX_cpu_stop(APEX_CPU *cpu);
#endif
