/******************************************************************************

Welcome to GDB Online.
GDB online is an online compiler and debugger tool for C, C++, Python, Java, PHP, Ruby, Perl,
C#, OCaml, VB, Swift, Pascal, Fortran, Haskell, Objective-C, Assembly, HTML, CSS, JS, SQLite, Prolog.
Code, Compile, Run and Debug online from anywhere in world.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    int *x = malloc(sizeof(int));
    *x = 2;

    if (!fork()) {
        if (!fork()) {
            *x = *x + 1;
        } else {
            *x = *x * 2;
        }
    } else {
        wait(NULL);
        *x = *x - 1;
    }

    printf("%d\n", *x);
}

