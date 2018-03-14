#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drcom.h"
#include "utility.h"

int main(void) {
    FILE *fp = fopen("config.txt", "r");
    if (!fp) {
        perror("open config");
        exit(EXIT_FAILURE);
    }
    str username, password, in_huxi;
    fgets(username, BUF_SIZE, fp);
    fgets(password, BUF_SIZE, fp);
    fgets(in_huxi, BUF_SIZE, fp);
    fclose(fp);

    strtok(username, "\n");
    strtok(password, "\n");
    strtok(in_huxi, "\n");

    if (strlen(username) == 0 || strlen(password) == 0) {
        printf("config error!\n");
        exit(EXIT_FAILURE);
    }

    int is_in_huxi = strcmp(in_huxi, "true") == 0;

    start(username, password, is_in_huxi);

    return 0;
}
