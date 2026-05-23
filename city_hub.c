#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

void handle_start_monitor() {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Fatal: pipe a esuat");
        return;
    }

    pid_t hub_mon_pid = fork();
    if (hub_mon_pid == -1) {
        perror("Fatal: fork hub_mon a esuat");
        return;
    }

    if (hub_mon_pid == 0) {
        close(pipefd[0]);

        pid_t monitor_pid = fork();
        if (monitor_pid == -1) {
            perror("Fatal: fork monitor a esuat");
            exit(1);
        }

        if (monitor_pid == 0) {         
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            execlp("./monitor_reports", "./monitor_reports", NULL);
            perror("Fatal: execlp monitor a esuat");
            exit(1);
        } else {
            close(pipefd[1]);
            int status;
            waitpid(monitor_pid, &status, 0);
            exit(0);
        }
    } else {
        close(pipefd[1]);
        
        printf("[Hub] Monitorul a fost pornit in fundal prin hub_mon.\n");
        
        char buffer[256];
        ssize_t bytesRead;
        
        if ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            printf("%s", buffer);
        }

        close(pipefd[0]);
    }
}

void handle_calculate_scores(char *districts_list) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Fatal: pipe esuat");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Fatal: fork esuat");
        return;
    }

    if (pid == 0) {
        close(pipefd[0]);
        
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execlp("./scorer", "./scorer", districts_list, NULL);
        perror("Fatal: Executarea scorer a esuat");
        exit(1);
    } else { // Proces parinte (city_hub)
        close(pipefd[1]); 
        
        char buffer[4096];
        ssize_t bytesRead;
        printf("\n=== RAPORT CENTRALIZAT VOLUM DE LUCRU ===\n");
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            printf("%s", buffer);
        }
        printf("=========================================\n");
        close(pipefd[0]);
        wait(NULL);
    }
}

int main() {
    char command[256];
    char arg[256];

    printf("=== CITY INFRASTRUCTURE HUB (Phase 3) ===\n");
    while (1) {
        printf("city_hub> ");
        fflush(stdout);
        
        if (fgets(command, sizeof(command), stdin) == NULL) break;
        
        command[strcspn(command, "\n")] = 0;

        if (strncmp(command, "start_monitor", 13) == 0) {
            handle_start_monitor();
        } else if (strncmp(command, "calculate_scores ", 17) == 0) {
            strcpy(arg, command + 17);
            handle_calculate_scores(arg);
        } else if (strcmp(command, "exit") == 0) {
            printf("Inchidere City Hub.\n");
            break;
        } else if (strlen(command) > 0) {
            printf("Comanda necunoscuta! Folositi: start_monitor, calculate_scores <liste_districte> sau exit\n");
        }
    }
    return 0;
}
