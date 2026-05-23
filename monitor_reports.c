#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

void handle_sigusr1(int sig) {
    printf("[MONITOR] Signal received: A new report has been added to the system.\n");
    fflush(stdout);
}

void handle_sigint(int sig) {
    printf("STATUS: STOPPED\n");
    fflush(stdout);
    
    unlink(".monitor_pid");
    exit(0);
}

int main() {
    FILE *chk = fopen(".monitor_pid", "r");
    if (chk != NULL) {
        int existing_pid;
        if (fscanf(chk, "%d", &existing_pid) == 1) {
            fclose(chk);
            printf("ERROR: Monitorul ruleaza deja cu PID-ul: %d\n", existing_pid);
            fflush(stdout);
            exit(1);
        }
        fclose(chk);
    }

    int fd = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { 
        perror("Eroare .monitor_pid"); 
        exit(1); 
    }
    
    char pid_str[16];
    sprintf(pid_str, "%d", getpid());
    write(fd, pid_str, strlen(pid_str));
    close(fd);

    struct sigaction sa_usr1, sa_int;

    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    sigaction(SIGUSR1, &sa_usr1, NULL);

    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    printf("[MONITOR] Monitor started. PID: %d. Waiting for reports...\n", getpid());
    fflush(stdout);

    while(1) {
        pause();
    }

    return 0;
}
