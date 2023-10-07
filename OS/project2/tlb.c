#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

char ADDRESS_SPACE_SIZE = 16;           // 16 bit address space
char *PAGE_SIZE_B = NULL;               //? byte page size
short unsigned int *PPN = NULL;         // Physical Page Number
short unsigned int *NEW_ADDRESS = NULL; // New Virtual Address to service
short unsigned int *TLB_SIZE = NULL;    //? Translation Lookaside Buffer entries
unsigned char REPL;                     // 0 for FIFO, 1 for Random, 2 for Clock
int seed;
short int PAGE_TABLE_ENTRIES; // Number of page table entries declaration

typedef struct
{
    unsigned short int PFN; // Physical Frame Number
    bool validBit;             // Valid bit
} PTEs;

int main(int argc, char *argv[])
{
    int TLB_HITS = 0;
    int TLB_MISSES = 0;
    unsigned short int TLB[255];

    if (argc < 3 || argc > 4)
    {
        printf("You must enter an input file to test and a replacement policy!\n");
        exit(-1);
    }

    FILE *fp;
    PAGE_SIZE_B = malloc(1);
    NEW_ADDRESS = malloc(2);
    PPN = malloc(2);
    TLB_SIZE = malloc(1);

    fp = fopen(argv[1], "rb");
    REPL = atoi(argv[2]);
    if (argc == 4)
    {
        seed = atoi(argv[3]);
        srand(seed);
    }
    fread(PAGE_SIZE_B, 1, 1, fp); // The first byte of the binary will be the page size
    fread(TLB_SIZE, 1, 1, fp);    // The second byte of the binary will be the TLB size

    // You must figure out how many Page Table Entries there are!
    // Iterate 2 bytes at a time the correct number of PTEs to get the physical address mappings
    PAGE_TABLE_ENTRIES = pow(2, ADDRESS_SPACE_SIZE) / (*PAGE_SIZE_B);

    // Array to keep the track of page faults occurs of the VPN's
    int PageFaultsTrace[PAGE_TABLE_ENTRIES];

    // PageTable to store all the Physical Page Numbers
    PTEs *PAGE_TABLE = malloc(PAGE_TABLE_ENTRIES * sizeof(PTEs));

    for (int i = 0; i < 255; i++)
        TLB[i] = 0; // Initializing all the TLB's to zero (0)

    bool ValidTlbEntry[255];

    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++)
    {
        PAGE_TABLE[i].PFN = -1;
        PAGE_TABLE[i].validBit = false;
        PageFaultsTrace[i] = 0;

        if (fread(PPN, 2, 1, fp) == 1)
        {
            // Here we read 1 (2 byte) Physical Address at a time; you must map it to the corresponding Page Table Entry
            PAGE_TABLE[i].PFN = *PPN;
            PAGE_TABLE[i].validBit = true;
        }
        else
        {
            printf("Something went wrong with fread!\n");
            exit(-1);
        }
    }

    bool referenceBits[255];

    // Initialing the validTlbEntry and referenceBits array to false
    for (int i = 0; i < 255; i++)
    {
        ValidTlbEntry[i] = false, referenceBits[i] = false;
    }

    int clockIndex = 0;  // variable for the clocks algorithm
    int PAGE_FAULTS = 0; // declaring the variable for pageFaults count

    while (fread(NEW_ADDRESS, 2, 1, fp) == 1)
    {
        bool tlbMiss = false;
        bool tlbHit = false;
        unsigned short int TRANSLATED_PHYSICAL_ADDRESS;
        unsigned short int VIRTUAL_ADDRESS = *NEW_ADDRESS;
        unsigned short int vpn_bits = (unsigned short int)log2(PAGE_TABLE_ENTRIES);
        unsigned short int SHIFT = log2(*PAGE_SIZE_B);
        unsigned short int VPN = (VIRTUAL_ADDRESS >> SHIFT) & ((1 << vpn_bits) - 1);
        unsigned short int OFFSET = VIRTUAL_ADDRESS % (*PAGE_SIZE_B);
        unsigned short int PFN_B = (unsigned short int)log2(PAGE_TABLE_ENTRIES);
        unsigned short int PAGE_SHIFT = SHIFT + (PFN_B - vpn_bits);

        // Check if the virtual page number is in the TLB and is it a valid Tlb entry
        for (int i = 0; i < *TLB_SIZE; i++)
        {
            if (TLB[i] == VPN && ValidTlbEntry[i])
            {
                tlbHit = true; //
                TLB_HITS++;    // incrementing the tlbHit count
                break;
            }
        }

        // If TLB miss, check the page table
        if (!tlbHit)
        {
            int FifoIndex = 0, randomIndex = 0, tlbIndex = 0; // declaration of varibale for FIFO, Random and Clock Algo's

            // checking the page faults and incrementing the count
            if (PAGE_TABLE[VPN].validBit && PageFaultsTrace[VPN] == 0)
                PageFaultsTrace[VPN]++;

            TLB_MISSES++;   // incrementing the TlbMiss count
            tlbMiss = true; // setting the tlbMiss varibale to true

            if (*TLB_SIZE > 0)
            {
                if (REPL == 0) // FIFO
                {
                    FifoIndex = (TLB_MISSES) % (*TLB_SIZE);
                    TLB[FifoIndex] = VPN;
                    ValidTlbEntry[FifoIndex] = true;
                }
                else if (REPL == 1) // Random page replacement Algo
                {
                    randomIndex = rand() % (*TLB_SIZE);
                    ValidTlbEntry[randomIndex] = true;
                    TLB[randomIndex] = VPN;
                }
                else if (REPL == 2) // Clocks page replacement Algo
                {
                    while (1)
                    {
                        if (ValidTlbEntry[clockIndex] && !referenceBits[clockIndex])
                        {
                            tlbIndex = clockIndex;
                            break;
                        }
                        else if (!ValidTlbEntry[clockIndex])
                        {
                            tlbIndex = clockIndex;
                            ValidTlbEntry[clockIndex] = true;
                            break;
                        }
                        else
                        {
                            referenceBits[clockIndex] = false;
                            clockIndex = (clockIndex + 1) % (*TLB_SIZE);
                        }
                    }
                    TLB[clockIndex] = VPN;
                }
            }
        }

        referenceBits[clockIndex] = tlbHit || tlbMiss ? true : referenceBits[clockIndex];

        // The translated physical address for the given Virtual addresses
        TRANSLATED_PHYSICAL_ADDRESS = (PAGE_TABLE[VPN].PFN << PAGE_SHIFT) | OFFSET;

        // Below is how the print statement should be formatted
        printf("VA:%x -- PA:%x\n", VIRTUAL_ADDRESS, TRANSLATED_PHYSICAL_ADDRESS);
    }

    for (int i = 0; i < PAGE_TABLE_ENTRIES; i++)
        PAGE_FAULTS += PageFaultsTrace[i]; // adding up the pagefaults count

    // Below is how the print statement should be formatted
    printf("Page Faults: %d\nTLB hits: %d\nTlb misses: %d\n", PAGE_FAULTS, TLB_HITS, TLB_MISSES);

    free(PAGE_SIZE_B);
    free(NEW_ADDRESS);
    free(PPN);
    free(TLB_SIZE);
    free(PAGE_TABLE);
    fclose(fp);
    return 0;
}
