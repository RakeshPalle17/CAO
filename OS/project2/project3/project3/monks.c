#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <semaphore.h>

unsigned int num_mugs;
unsigned int num_kegs;
pthread_mutex_t servs_mt;
pthread_cond_t allKegsEmptied;
int servs, totalKegsAvailed;
pthread_cond_t emptyKeg, fetchKeg;
int totalMonks[10] = {0};
int flag = 1;

void *brewMasterThread(void *arg)
{

    for (int i = 0; i < num_kegs; i++)
    {
        pthread_mutex_lock(&servs_mt); // holding a lock on mutex

        while (servs > 0)
            pthread_cond_wait(&emptyKeg, &servs_mt); // waiting on condition variables

        servs = num_mugs;
        totalKegsAvailed++; // incrementing the total kegs drunk by monks

        pthread_cond_broadcast(&fetchKeg);
        pthread_mutex_unlock(&servs_mt);
    }

    pthread_cond_signal(&allKegsEmptied);

    return NULL;
}

void *monkThreads(void *arg)
{
    int monkId = *(int *)arg;

    while (flag)
    {

        pthread_mutex_lock(&servs_mt); // holding a lock on mutex

        while (totalKegsAvailed < num_kegs && servs == 0)
            pthread_cond_wait(&fetchKeg, &servs_mt);

        if (servs == 0 && totalKegsAvailed >= num_kegs)
        {
            pthread_mutex_unlock(&servs_mt); // unlock the mutex
            pthread_cond_signal(&allKegsEmptied);
            break; // Keg is empty, exit the loop
        }
        servs--;               
        // decrement the servings by 1

        totalMonks[monkId]++; // Incrementing the mugs drunk by each monk

        printf("Monk #%d drank!\n", monkId + 1);

        if (servs == 0) // if the servings left on keg is empty signal condition
            pthread_cond_signal(&emptyKeg);

        pthread_mutex_unlock(&servs_mt); // unlock the mutex
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("You must input number of mugs per keg and number of kegs\n");
        exit(-1);
    }

    num_mugs = atoi(argv[1]);
    num_kegs = atoi(argv[2]);

    int monksIds[10], totalMugsDrank = 0;
    pthread_t monks[10], brewMaster;
     
	// Initialize the monks with Ids from 1 to 10
    for (int i = 0; i < 10; i++)
        monksIds[i] = i;

    // Initialize mutex and condtion varibables
    pthread_mutex_init(&servs_mt, NULL);
	pthread_cond_init(&fetchKeg, NULL);

    pthread_cond_init(&allKegsEmptied, NULL);
    pthread_cond_init(&emptyKeg, NULL);


    totalKegsAvailed = 0;
    servs = 0;

    for (int i = 0; i < 10; i++) // creating threads for each of the monks
    {
        pthread_create(&monks[i], NULL, monkThreads, &monksIds[i]);
    }

    pthread_create(&brewMaster, NULL, brewMasterThread, NULL); 
    // wait for the threads to end
    pthread_join(brewMaster, NULL);

    for (int i = 0; i < 10; i++)
        pthread_join(monks[i], NULL); // waiting on threads (monk threads)

    pthread_mutex_destroy(&servs_mt);
	pthread_cond_destroy(&fetchKeg);

    pthread_cond_destroy(&allKegsEmptied);
    pthread_cond_destroy(&emptyKeg);

    for (int i = 0; i < 10; i++)
    {
        printf("Monk #%d drank %d mugs of beer\n", i + 1, totalMonks[i]);
        totalMugsDrank = totalMugsDrank + totalMonks[i];
    }

    printf("The monks drank %d mugs of beer!\n", totalMugsDrank);

    return 0;
}