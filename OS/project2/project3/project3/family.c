#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

#define MAX_FAMILY_MEMBERS 100

unsigned int num_family_members;
sem_t chopsticks[MAX_FAMILY_MEMBERS];
int num_lines_printed = 0;

void get_chopsticks(unsigned int left, unsigned int right) {
    sem_wait(&chopsticks[left]);
    sem_wait(&chopsticks[right]);
}

void put_chopsticks(unsigned int left, unsigned int right) {
    sem_post(&chopsticks[left]);
    sem_post(&chopsticks[right]);
}

void* happyFamily(void* arg) {
    
    int id = *(int*)arg;
    int left_chopstick = id;
    int right_chopstick = (id + 1) % num_family_members;

    while (1) {

        get_chopsticks(left_chopstick, right_chopstick);

        printf("Family member %u is eating\n", id);
        ++num_lines_printed;

        usleep(rand() % 51 * 1000);

        put_chopsticks(left_chopstick, right_chopstick);

        if (num_lines_printed == 1000)  
            exit(0);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("You must input the number of family members!\n");
        exit(-1);
    }

    num_family_members = atoi(argv[1]);
    pthread_t threads[num_family_members];
    int ids[num_family_members];

    for (int i = 0; i < num_family_members; i++)
    {
        int err;
        ids[i] = i;
        sem_init(&chopsticks[i], 0, 1);
        err = pthread_create(&threads[i], NULL, happyFamily, &ids[i]);
        if(err != 0)
            printf("can't create thread");
    }

    for (int i = 0; i < num_family_members; i++)
        pthread_join(threads[i], NULL);

    return 0;
}
