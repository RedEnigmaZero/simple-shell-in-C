#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
    printf("sshell: ");
    char intt;
    scanf("%s", &intt);
    printf("sshell: %s\n", &intt);

    return 0;
}