#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apex_cpu.h"
#include "apex_macros.h"

/* Converts the PC(4000 series) into array index for code memory
 *
 * Note: You are not supposed to edit this function
 */
static int
get_code_memory_index_from_pc(const int pc)
{
    return (pc - 4000) / 4;
}

static void
print_instruction(const CPU_Stage *stage)
{
    switch (stage->opcode)
    {
    case OPCODE_ADD:
    case OPCODE_SUB:
    case OPCODE_MUL:
    case OPCODE_DIV:
    case OPCODE_AND:
    case OPCODE_OR:
    case OPCODE_XOR:
    {
        printf("%s,R%d,R%d,R%d ", stage->opcode_str, stage->rd, stage->rs1,
               stage->rs2);
        break;
    }

    case OPCODE_MOVC:
    {
        printf("%s,R%d,#%d ", stage->opcode_str, stage->rd, stage->imm);
        break;
    }

    case OPCODE_LOAD:
    case OPCODE_LOADP:
    {
        printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
               stage->imm);
        break;
    }

    case OPCODE_STORE:
    case OPCODE_STOREP:
    {
        printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rs1, stage->rs2,
               stage->imm);
        break;
    }

    case OPCODE_BZ:
    case OPCODE_BNZ:
    case OPCODE_BP:
    case OPCODE_BNP:
    case OPCODE_BN:
    case OPCODE_BNN:
    {
        printf("%s,#%d ", stage->opcode_str, stage->imm);
        break;
    }

    case OPCODE_ADDL:
    case OPCODE_SUBL:
    case OPCODE_JALR:
    {
        printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
               stage->imm);
        break;
    }

    case OPCODE_CML:
    case OPCODE_JUMP:
    {
        printf("%s,R%d,#%d ", stage->opcode_str, stage->rs1,
               stage->imm);
        break;
    }

    case OPCODE_CMP:
    {
        printf("%s,R%d,R%d ", stage->opcode_str, stage->rs1,
               stage->rs2);
        break;
    }

    case OPCODE_NOP:
    {
        printf("%s", stage->opcode_str);
        break;
    }

    case OPCODE_HALT:
    {
        printf("%s", stage->opcode_str);
        break;
    }
    }
}

/* Debug function which prints the CPU stage content
 *
 * Note: You can edit this function to print in more detail
 */
static void
print_stage_content(const char *name, const CPU_Stage *stage)
{
    printf("%-15s: pc(%d) ", name, stage->pc);
    print_instruction(stage);
    printf("\n");
}

static void
print_flag_values(int p, int n, int z)
{

    printf("-----------\n%s P = %d, N = %d, Z = %d\n-----------\n", "Flag Values: ", p, n, z);
}

static void
print_memory_address_values(APEX_CPU *cpu)
{
    printf("Memory Addresses:  ");
    for (int i = 0; i < DATA_MEMORY_SIZE; i++)
    {
        if (cpu->data_memory[i] != 0)
        {
            printf("MEM[%d] = %d ", i, cpu->data_memory[i]);
        }
    }
}

/* Debug function which prints the register file
 *
 * Note: You are not supposed to edit this function
 */
static void
print_reg_file(const APEX_CPU *cpu)
{
    int i;

    printf("\n----------\n%s\n----------\n", "Registers:");

    for (int i = 0; i < REG_FILE_SIZE / 2; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");

    for (i = (REG_FILE_SIZE / 2); i < REG_FILE_SIZE; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");
}

static void
initialize_default_btb(APEX_CPU *cpu)
{
    cpu->btb_full = TRUE;
    for (int i = 0; i < BTB_SIZE; i++)
    {
        cpu->BTBEntry[i].recent_outcomes[0] = TAKEN;
        cpu->BTBEntry[i].recent_outcomes[1] = TAKEN;

        cpu->BTBEntry[i].branch_pc = ADD_ZERO;
        cpu->BTBEntry[i].target_pc = ADD_ZERO;
    }
}

static void
btb_is_full(APEX_CPU *cpu)
{
    if (!cpu->decode.btb_hit)
    {
        cpu->BTBEntry[cpu->counter].branch_pc = cpu->decode.pc;
        cpu->BTBEntry[cpu->counter].target_pc = ADD_ZERO;
        cpu->BTBEntry[cpu->counter].recent_outcomes[0] = NOT_TAKEN;
        cpu->BTBEntry[cpu->counter].recent_outcomes[1] = NOT_TAKEN;
        cpu->counter++;
    }
    if (cpu->counter == BTB_SIZE)
    {
        cpu->counter = ADD_ZERO;
    }
}

static void
btb_is_full_bnz_bp(APEX_CPU *cpu)
{
    if (!cpu->decode.btb_hit)
    {
        cpu->BTBEntry[cpu->counter].branch_pc = cpu->decode.pc;
        cpu->BTBEntry[cpu->counter].target_pc = ADD_ZERO;
        cpu->BTBEntry[cpu->counter].recent_outcomes[0] = TAKEN;
        cpu->BTBEntry[cpu->counter].recent_outcomes[1] = TAKEN;
        cpu->counter++;
    }
    if (cpu->counter == BTB_SIZE)
    {
        cpu->counter = ADD_ZERO;
    }
}

static void
establish_btb_entry(APEX_CPU *cpu)
{
    cpu->btb_full = TRUE;
    for (int i = 0; i < BTB_SIZE; i++)
    {
        if (cpu->BTBEntry[i].branch_pc == 0 || cpu->BTBEntry[i].branch_pc == cpu->decode.pc)
        {
            if (!cpu->decode.btb_hit)
            {
                cpu->BTBEntry[i].branch_pc = cpu->decode.pc;
                cpu->BTBEntry[i].recent_outcomes[0] = NOT_TAKEN;
                cpu->BTBEntry[i].recent_outcomes[1] = NOT_TAKEN;
            }

            cpu->btb_full = FALSE;
            break;
        }
    }

    if (cpu->btb_full)
    {
        btb_is_full(cpu);
    }
}

static void
establish_btb_entry_bnz_bp(APEX_CPU *cpu)
{

    cpu->btb_full = TRUE;
    for (int i = 0; i < BTB_SIZE; i++)
    {
        if (cpu->BTBEntry[i].branch_pc == 0 || cpu->BTBEntry[i].branch_pc == cpu->decode.pc)
        {
            if (!cpu->decode.btb_hit)
            {
                cpu->BTBEntry[i].branch_pc = cpu->decode.pc;
                cpu->BTBEntry[i].recent_outcomes[0] = TAKEN;
                cpu->BTBEntry[i].recent_outcomes[1] = TAKEN;
            }

            cpu->btb_full = FALSE;
            break;
        }
    }

    if (cpu->btb_full)
    {
        btb_is_full_bnz_bp(cpu);
    }
}

static void
branch_taken_flow(APEX_CPU *cpu)
{
    for (int i = 0; i < BTB_SIZE; i++)
    {
        if (cpu->execute.pc == cpu->BTBEntry[i].branch_pc)
        {
            cpu->BTBEntry[i].target_pc = cpu->execute.pc + cpu->execute.imm;
            if (!cpu->execute.taken)
            {
                cpu->pc = cpu->execute.pc + cpu->execute.imm;
                cpu->fetch_from_next_cycle = TRUE;
                cpu->decode.has_insn = FALSE;
                cpu->fetch.has_insn = TRUE;
            }
            if (!cpu->BTBEntry[i].recent_outcomes[0] && !cpu->BTBEntry[i].recent_outcomes[1])
            {
                cpu->BTBEntry[i].recent_outcomes[1] = TAKEN;
            }
            else if (!cpu->BTBEntry[i].recent_outcomes[0] && cpu->BTBEntry[i].recent_outcomes[1])
            {
                cpu->BTBEntry[i].recent_outcomes[0] = TAKEN;
                cpu->BTBEntry[i].recent_outcomes[1] = NOT_TAKEN;
            }
            else if (cpu->BTBEntry[i].recent_outcomes[0] && !cpu->BTBEntry[i].recent_outcomes[1])
            {
                cpu->BTBEntry[i].recent_outcomes[0] = TAKEN;
                cpu->BTBEntry[i].recent_outcomes[1] = TAKEN;
            }
            break;
        }
    }
}

static void
branch_not_taken_flow(APEX_CPU *cpu)
{
    for (int i = 0; i < BTB_SIZE; i++)
    {
        if (cpu->execute.pc == cpu->BTBEntry[i].branch_pc)
        {
            cpu->BTBEntry[i].target_pc = cpu->execute.pc + cpu->execute.imm;
            
            if (cpu->execute.taken)
            {
                cpu->pc = cpu->execute.pc + INCREMENTOR;
                cpu->fetch_from_next_cycle = TRUE;
                cpu->decode.has_insn = FALSE;
                cpu->fetch.has_insn = TRUE;
            }
            
            if (cpu->BTBEntry[i].recent_outcomes[0] && cpu->BTBEntry[i].recent_outcomes[1])
            {
                cpu->BTBEntry[i].recent_outcomes[1] = NOT_TAKEN;
            }
            else if (cpu->BTBEntry[i].recent_outcomes[0] && !cpu->BTBEntry[i].recent_outcomes[1])
            {
                cpu->BTBEntry[i].recent_outcomes[0] = NOT_TAKEN;
                cpu->BTBEntry[i].recent_outcomes[1] = TAKEN;
            }
            else if (!cpu->BTBEntry[i].recent_outcomes[0] && cpu->BTBEntry[i].recent_outcomes[1])
            {
                cpu->BTBEntry[i].recent_outcomes[0] = NOT_TAKEN;
                cpu->BTBEntry[i].recent_outcomes[1] = NOT_TAKEN;
            }

            break;
        }
    }
}

/*
 * Fetch Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_fetch(APEX_CPU *cpu)
{
    APEX_Instruction *current_ins;

    if (cpu->fetch.has_insn)
    {
        if (cpu->fetch_from_next_cycle == TRUE)
        {
            cpu->fetch_from_next_cycle = FALSE;

            if (ENABLE_DEBUG_MESSAGES)
            {
                printf("Fetch          :\n");
            }
            /* Skip this cycle*/
            return;
        }

        cpu->fetch.pc = cpu->pc;

        /* Index into code memory using this pc and copy all instruction fields
         * into fetch latch  */
        current_ins = &cpu->code_memory[get_code_memory_index_from_pc(cpu->pc)];
        strcpy(cpu->fetch.opcode_str, current_ins->opcode_str);
        cpu->fetch.opcode = current_ins->opcode;
        cpu->fetch.rd = current_ins->rd;
        cpu->fetch.rs1 = current_ins->rs1;
        cpu->fetch.rs2 = current_ins->rs2;
        cpu->fetch.imm = current_ins->imm;

        cpu->fetch.taken = FALSE;
        cpu->fetch.btb_hit = FALSE;

        int i = 0;
        for (i = 0; i < BTB_SIZE; i++)
        {
            if (cpu->fetch.pc == cpu->BTBEntry[i].branch_pc)
            {
                cpu->fetch.btb_hit = TRUE;
                if (cpu->BTBEntry[i].recent_outcomes[0] && cpu->BTBEntry[i].recent_outcomes[1])
                {
                    cpu->fetch.taken = TRUE;
                }
                else if (cpu->BTBEntry[i].recent_outcomes[0] && !cpu->BTBEntry[i].recent_outcomes[1])
                {
                    cpu->fetch.taken = TRUE;
                }
                break;
            }
        }

        if (!cpu->decode.decode_stall && cpu->fetch.taken)
        {
            cpu->pc = cpu->BTBEntry[i].target_pc;
            cpu->decode = cpu->fetch;
        }

        if (!cpu->decode.decode_stall && !cpu->fetch.taken)
        {
            cpu->pc += 4;
            cpu->decode = cpu->fetch;
        }

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Fetch", &cpu->fetch);
        }

        /* Stop fetching new instructions if HALT is fetched */
        if (cpu->fetch.opcode == OPCODE_HALT)
        {
            cpu->fetch.has_insn = FALSE;
        }

        if (cpu->decode.decode_stall)
        {
            cpu->fetch.has_insn = TRUE;
        }
    }
    else
    {
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("Fetch          :\n");
        }
    }
}

/*
 * Decode Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_decode(APEX_CPU *cpu)
{

    if (cpu->decode.has_insn)
    {
        /* Read operands from register file based on the instruction type */

        cpu->writeToSource1 = FALSE;
        cpu->writeToSource2 = FALSE;
        cpu->writeToDestination = FALSE;

        switch (cpu->decode.opcode)
        {

        case OPCODE_ADD:
        case OPCODE_SUB:
        case OPCODE_MUL:
        case OPCODE_DIV:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_XOR:
        {
            if ((cpu->regStatus[cpu->decode.rs1].isInvalid && cpu->decode.rs1 >= 0))
            {
                cpu->decode.decode_stall = TRUE;
            }
            else if (cpu->regStatus[cpu->decode.rs2].isInvalid && cpu->decode.rs2 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                cpu->decode.decode_stall = FALSE;
                cpu->writeToDestination = TRUE;
            }

            break;
        }

        case OPCODE_MOVC:
        {
            cpu->decode.decode_stall = FALSE;
            cpu->writeToDestination = TRUE;
            break;
        }

        case OPCODE_BNZ:
        {
            establish_btb_entry_bnz_bp(cpu);
            break;
        }

        case OPCODE_BP:
        {
            establish_btb_entry_bnz_bp(cpu);
            break;
        }

        case OPCODE_BZ:
        {
            establish_btb_entry(cpu);
            break;
        }

        case OPCODE_BNP:
        {
            establish_btb_entry(cpu);
            break;
        }

        case OPCODE_LOAD:
        {
            if (cpu->regStatus[cpu->decode.rs1].isInvalid && cpu->decode.rs1 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.decode_stall = FALSE;
                cpu->writeToDestination = TRUE;
            }
            break;
        }

        case OPCODE_LOADP:
        {
            if (cpu->regStatus[cpu->decode.rs1].isInvalid && cpu->decode.rs1 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.decode_stall = FALSE;
                cpu->writeToDestination = TRUE;
                cpu->writeToSource1 = TRUE;
            }
            break;
        }

        case OPCODE_STORE:
        case OPCODE_CMP:
        {
            if (cpu->regStatus[cpu->decode.rs1].isInvalid && cpu->decode.rs1 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else if (cpu->regStatus[cpu->decode.rs2].isInvalid && cpu->decode.rs2 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                cpu->decode.decode_stall = FALSE;
                cpu->writeToDestination = FALSE;
            }
            break;
        }

        case OPCODE_STOREP:
        {

            if (cpu->regStatus[cpu->decode.rs1].isInvalid && cpu->decode.rs1 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else if (cpu->regStatus[cpu->decode.rs2].isInvalid && cpu->decode.rs2 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                cpu->decode.decode_stall = FALSE;
                cpu->writeToSource2 = TRUE;
            }
            break;
        }

        case OPCODE_ADDL:
        case OPCODE_SUBL:
        case OPCODE_JALR:
        {
            if (cpu->regStatus[cpu->decode.rs1].isInvalid && cpu->decode.rs1 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.decode_stall = FALSE;
                cpu->writeToDestination = TRUE;
            }
            break;
        }

        case OPCODE_JUMP:
        case OPCODE_CML:
        {
            if (cpu->regStatus[cpu->decode.rs1].isInvalid && cpu->decode.rs1 >= 0)
            {
                cpu->decode.decode_stall = TRUE;
            }
            else
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.decode_stall = FALSE;
                cpu->writeToDestination = FALSE;
            }
            break;
        }

        case OPCODE_HALT:
        case OPCODE_BN:
        case OPCODE_BNN:
        case OPCODE_NOP:
        {
            break;
        }
        }

        if (!cpu->decode.decode_stall)
        {
            cpu->execute = cpu->decode;
            cpu->decode.has_insn = FALSE;
            cpu->decode.decode_stall = FALSE;
        }

        if (cpu->writeToDestination && !cpu->decode.decode_stall)
        {
            cpu->regStatus[cpu->decode.rd].isInvalid = TRUE;
        }

        if (cpu->writeToSource1 && !cpu->decode.decode_stall)
        {
            cpu->regStatus[cpu->decode.rs1].isInvalid = TRUE;
        }

        if (cpu->writeToSource2 && !cpu->decode.decode_stall)
        {
            cpu->regStatus[cpu->decode.rs2].isInvalid = TRUE;
        }

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Decode/RF", &cpu->decode);
        }
    }
    else
    {
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("Decode/RF      :\n");
        }
    }
}

/*
 * Execute Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_execute(APEX_CPU *cpu)
{
    if (cpu->execute.has_insn)
    {
        /* Execute logic based on instruction type */

        switch (cpu->execute.opcode)
        {
        case OPCODE_ADD:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value + cpu->execute.rs2_value;

            /* Set the zero, positive and negative flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.result_buffer < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }
            break;
        }

        case OPCODE_SUB:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value - cpu->execute.rs2_value;

            /* Set the zero, positive and negative flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.result_buffer < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }
            break;
        }

        case OPCODE_ADDL:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value + cpu->execute.imm;

            /* Set the zero, positive and negative flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.result_buffer < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }
            break;
        }

        case OPCODE_SUBL:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value - cpu->execute.imm;

            /* Set the zero, positive and negative flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.result_buffer < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }
            break;
        }

        case OPCODE_MUL:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value * cpu->execute.rs2_value;

            /* Set the zero, positive and negative flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.result_buffer < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }
            break;
        }

        case OPCODE_DIV:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value / cpu->execute.rs2_value;
            break;
        }

        case OPCODE_AND:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value & cpu->execute.rs2_value;

            /* Set the zero, positive and negative flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.result_buffer < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }
            break;
        }

        case OPCODE_OR:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value | cpu->execute.rs2_value;

            /* Set the zero, positive and negative flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.result_buffer < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }

            break;
        }

        case OPCODE_XOR:
        {
            cpu->execute.result_buffer = cpu->execute.rs1_value ^ cpu->execute.rs2_value;

            /* Set the zero, positive and negative flag based on the result buffer */
            if (cpu->execute.result_buffer == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.result_buffer < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }

            break;
        }

        case OPCODE_MOVC:
        {
            cpu->execute.result_buffer = cpu->execute.imm + ADD_ZERO;
            break;
        }

        case OPCODE_LOAD:
        {
            cpu->execute.memory_address = cpu->execute.rs1_value + cpu->execute.imm;
            break;
        }

        case OPCODE_LOADP:
        {
            cpu->execute.incrementor = cpu->execute.rs1_value + INCREMENTOR;
            cpu->execute.memory_address = cpu->execute.rs1_value + cpu->execute.imm;
            break;
        }

        case OPCODE_STORE:
        {
            cpu->execute.memory_address = cpu->execute.rs2_value + cpu->execute.imm;
            break;
        }
        case OPCODE_STOREP:
        {
            cpu->execute.incrementor = cpu->execute.rs2_value + INCREMENTOR;
            cpu->execute.memory_address = cpu->execute.rs2_value + cpu->execute.imm;
            break;
        }

        case OPCODE_CML:
        {

            if (cpu->execute.rs1_value - cpu->execute.imm == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.rs1_value - cpu->execute.imm < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }

            break;
        }

        case OPCODE_CMP:
        {

            if (cpu->execute.rs1_value - cpu->execute.rs2_value == 0)
            {
                cpu->zero_flag = TRUE;
                cpu->p_flag = FALSE;
                cpu->n_flag = FALSE;
            }
            else if (cpu->execute.rs1_value - cpu->execute.rs2_value < 0)
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = FALSE;
                cpu->n_flag = TRUE;
            }
            else
            {
                cpu->zero_flag = FALSE;
                cpu->p_flag = TRUE;
                cpu->n_flag = FALSE;
            }

            break;
        }

        case OPCODE_JALR:
        {
            cpu->pc = cpu->execute.rs1_value + cpu->execute.imm;
            cpu->execute.result_buffer = cpu->execute.pc + INCREMENTOR;

            /* Since we are using reverse callbacks for pipeline stages,
             * this will prevent the new instruction from being fetched in the current cycle*/
            cpu->fetch_from_next_cycle = TRUE;
            /* Flush previous stages */
            cpu->decode.has_insn = FALSE;
            /* Make sure fetch stage is enabled to start fetching from new PC */
            cpu->fetch.has_insn = TRUE;
            break;
        }

        case OPCODE_JUMP:
        {
            cpu->pc = cpu->execute.rs1_value + cpu->execute.imm;

            cpu->fetch_from_next_cycle = TRUE;
            /* Flush previous stages */
            cpu->decode.has_insn = FALSE;
            /* Make sure fetch stage is enabled to start fetching from new PC */
            cpu->fetch.has_insn = TRUE;
            break;
        }

        case OPCODE_BNZ:
        {
            if (cpu->zero_flag == FALSE)
            {
                branch_taken_flow(cpu);
            }
            else
            {
                branch_not_taken_flow(cpu);
            }

            break;
        }

        case OPCODE_BP:
        {
            if (cpu->p_flag == TRUE)
            {
                branch_taken_flow(cpu);
            }
            else
            {
                branch_not_taken_flow(cpu);
            }
            break;
        }

        case OPCODE_BZ:
        {
            if (cpu->zero_flag == TRUE)
            {
                branch_taken_flow(cpu);
            }
            else
            {
                branch_not_taken_flow(cpu);
            }
            break;
        }

        case OPCODE_BNP:
        {
            if (cpu->p_flag == FALSE)
            {
                branch_taken_flow(cpu);
            }
            else
            {
                branch_not_taken_flow(cpu);
            }
            break;
        }

        case OPCODE_BN:
        {
            if (cpu->n_flag == TRUE)
            {
                /* Calculate new PC, and send it to fetch unit */
                cpu->pc = cpu->execute.pc + cpu->execute.imm;

                /* Since we are using reverse callbacks for pipeline stages,
                 * this will prevent the new instruction from being fetched in the current cycle*/
                cpu->fetch_from_next_cycle = TRUE;

                /* Flush previous stages */
                cpu->decode.has_insn = FALSE;

                /* Make sure fetch stage is enabled to start fetching from new PC */
                cpu->fetch.has_insn = TRUE;
            }
            break;
        }
        case OPCODE_BNN:
        {
            if (cpu->zero_flag == FALSE)
            {
                /* Calculate new PC, and send it to fetch unit */
                cpu->pc = cpu->execute.pc + cpu->execute.imm;

                /* Since we are using reverse callbacks for pipeline stages,
                 * this will prevent the new instruction from being fetched in the current cycle*/
                cpu->fetch_from_next_cycle = TRUE;

                /* Flush previous stages */
                cpu->decode.has_insn = FALSE;

                /* Make sure fetch stage is enabled to start fetching from new PC */
                cpu->fetch.has_insn = TRUE;
            }
            break;
        }

        case OPCODE_HALT:
        case OPCODE_NOP:
        {
            break;
        }
        }

        /* Copy data from execute latch to memory latch*/
        cpu->memory = cpu->execute;
        cpu->execute.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Execute", &cpu->execute);
        }
    }
    else
    {
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("Execute        :\n");
        }
    }
}

/*
 * Memory Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_memory(APEX_CPU *cpu)
{
    if (cpu->memory.has_insn)
    {
        switch (cpu->memory.opcode)
        {
        case OPCODE_ADD:
        case OPCODE_SUB:
        case OPCODE_MUL:
        case OPCODE_DIV:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_XOR:
        case OPCODE_MOVC:
        {
            /* No work for ADD */
            break;
        }

        case OPCODE_LOAD:
        case OPCODE_LOADP:
        {
            /* Read from data memory */
            cpu->memory.result_buffer = cpu->data_memory[cpu->memory.memory_address];
            break;
        }

        case OPCODE_STORE:
        case OPCODE_STOREP:
        {
            cpu->data_memory[cpu->memory.memory_address] = cpu->memory.rs1_value;
            break;
        }

        case OPCODE_JALR:
        case OPCODE_ADDL:
        case OPCODE_SUBL:
        {
            break;
        }

        case OPCODE_BZ:
        case OPCODE_BNZ:
        case OPCODE_HALT:
        case OPCODE_CML:
        case OPCODE_CMP:
        case OPCODE_NOP:
        case OPCODE_BP:
        case OPCODE_BNP:
        case OPCODE_BN:
        case OPCODE_BNN:
        case OPCODE_JUMP:
        {
            break;
        }
        }

        /* Copy data from memory latch to writeback latch*/
        cpu->writeback = cpu->memory;
        cpu->memory.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Memory", &cpu->memory);
        }
    }
    else
    {
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("Memory         :\n");
        }
    }
}

/*
 * Writeback Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static int
APEX_writeback(APEX_CPU *cpu)
{
    if (cpu->writeback.has_insn)
    {
        /* Write result to register file based on instruction type */
        switch (cpu->writeback.opcode)
        {
        case OPCODE_ADD:
        case OPCODE_SUB:
        case OPCODE_MUL:
        case OPCODE_DIV:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_XOR:
        case OPCODE_ADDL:
        case OPCODE_SUBL:
        case OPCODE_LOAD:
        case OPCODE_MOVC:
        {
            cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
            cpu->regStatus[cpu->writeback.rd].isInvalid = FALSE;
            break;
        }

        case OPCODE_LOADP:
        {
            cpu->regs[cpu->writeback.rs1] = cpu->writeback.incrementor;
            cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
            cpu->regStatus[cpu->writeback.rs1].isInvalid = FALSE;
            cpu->regStatus[cpu->writeback.rd].isInvalid = FALSE;
            break;
        }

        case OPCODE_STOREP:
        {
            cpu->regs[cpu->writeback.rs2] = cpu->writeback.incrementor;
            cpu->regStatus[cpu->writeback.rs2].isInvalid = FALSE;
            break;
        }

        case OPCODE_JALR:
        {
            cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
            cpu->regStatus[cpu->writeback.rd].isInvalid = FALSE;
            break;
        }

        case OPCODE_STORE:
        case OPCODE_BZ:
        case OPCODE_BNZ:
        case OPCODE_BP:
        case OPCODE_BNP:
        case OPCODE_BN:
        case OPCODE_BNN:
        case OPCODE_NOP:
        case OPCODE_HALT:
        case OPCODE_CML:
        case OPCODE_CMP:
        case OPCODE_JUMP:
        {

            break;
        }
        }

        cpu->insn_completed++;
        cpu->writeback.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Writeback", &cpu->writeback);
        }

        if (cpu->writeback.opcode == OPCODE_HALT)
        {
            /* Stop the APEX simulator */
            return TRUE;
        }
    }
    else
    {
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("Writeback      :\n");
        }
    }

    /* Default */
    return 0;
}

/*
 * This function creates and initializes APEX cpu.
 *
 * Note: You are free to edit this function according to your implementation
 */
APEX_CPU *
APEX_cpu_init(const char *filename)
{
    int i;
    APEX_CPU *cpu;

    if (!filename)
    {
        return NULL;
    }

    cpu = calloc(1, sizeof(APEX_CPU));

    if (!cpu)
    {
        return NULL;
    }

    /* Initialize PC, Registers and all pipeline stages */
    cpu->pc = 4000;
    memset(cpu->regs, 0, sizeof(int) * REG_FILE_SIZE);
    memset(cpu->data_memory, 0, sizeof(int) * DATA_MEMORY_SIZE);
    memset(cpu->regStatus, 0, sizeof(int) * REG_FILE_SIZE);
    cpu->single_step = ENABLE_SINGLE_STEP;

    /* Parse input file and create code memory */
    cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);
    if (!cpu->code_memory)
    {
        free(cpu);
        return NULL;
    }

    if (ENABLE_DEBUG_MESSAGES)
    {
        fprintf(stderr,
                "APEX_CPU: Initialized APEX CPU, loaded %d instructions\n",
                cpu->code_memory_size);
        fprintf(stderr, "APEX_CPU: PC initialized to %d\n", cpu->pc);
        fprintf(stderr, "APEX_CPU: Printing Code Memory\n");
        printf("%-9s %-9s %-9s %-9s %-9s\n", "opcode_str", "rd", "rs1", "rs2",
               "imm");

        for (i = 0; i < cpu->code_memory_size; ++i)
        {
            printf("%-9s %-9d %-9d %-9d %-9d\n", cpu->code_memory[i].opcode_str,
                   cpu->code_memory[i].rd, cpu->code_memory[i].rs1,
                   cpu->code_memory[i].rs2, cpu->code_memory[i].imm);
        }
    }

    /* To start fetch stage */
    cpu->fetch.has_insn = TRUE;
    return cpu;
}

/*
 * APEX CPU simulation loop
 *
 * Note: You are free to edit this function according to your implementation
 */
void APEX_cpu_run(APEX_CPU *cpu)
{
    char user_prompt_val;
    cpu->clock = 1;

    initialize_default_btb(cpu);

    while (TRUE)
    {
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("--------------------------------------------\n");
            printf("Clock Cycle #: %d\n", cpu->clock);
            printf("--------------------------------------------\n");
        }

        if (APEX_writeback(cpu) || cpu->num_of_cycles_to_run == cpu->clock)
        {
            print_flag_values(cpu->p_flag, cpu->n_flag, cpu->zero_flag);
            print_memory_address_values(cpu);
            print_reg_file(cpu);
            /* Halt in writeback stage */
            printf("APEX_CPU: Simulation Complete, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
            break;
        }

        APEX_memory(cpu);
        APEX_execute(cpu);
        APEX_decode(cpu);
        APEX_fetch(cpu);

        print_flag_values(cpu->p_flag, cpu->n_flag, cpu->zero_flag);
        print_memory_address_values(cpu);
        print_reg_file(cpu);

        if (cpu->single_step)
        {
            printf("Press any key to advance CPU Clock or <q> to quit:\n");
            scanf("%c", &user_prompt_val);

            if ((user_prompt_val == 'Q') || (user_prompt_val == 'q'))
            {
                printf("APEX_CPU: Simulation Stopped, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
                break;
            }
        }

        cpu->clock++;
    }
}

/*
 * This function deallocates APEX CPU.

 *
 * Note: You are free to edit this function according to your implementation
 */
void APEX_cpu_stop(APEX_CPU *cpu)
{
    free(cpu->code_memory);
    free(cpu);
}