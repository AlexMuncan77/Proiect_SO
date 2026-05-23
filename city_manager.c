#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>

#define MAX_STR 64

typedef struct {
    int report_id;
    char inspector_name[MAX_STR];
    float latitude;
    float longitude;
    char issue_category[MAX_STR];
    int severity_level;
    time_t timestamp;
    char description[256];
} Report;

void get_perms_string(mode_t mode, char *str) {
    strcpy(str, "---------");
    if (mode & S_IRUSR) str[0] = 'r';
    if (mode & S_IWUSR) str[1] = 'w';
    if (mode & S_IXUSR) str[2] = 'x';
    if (mode & S_IRGRP) str[3] = 'r';
    if (mode & S_IWGRP) str[4] = 'w';
    if (mode & S_IXGRP) str[5] = 'x';
    if (mode & S_IROTH) str[6] = 'r';
    if (mode & S_IWOTH) str[7] = 'w';
    if (mode & S_IXOTH) str[8] = 'x';
}

void log_action(const char *district, const char *role, const char *user, const char *action) {
    char path[256];
    sprintf(path, "%s/logged_district", district);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    
    time_t acum = time(NULL);
    char buffer[512];
    int len = sprintf(buffer, "%ld\t%s\t%s\t%s\n", (long)acum, user, role, action);
    write(fd, buffer, len);
    close(fd);
}

void notify_monitor(const char *district, const char *role, const char *user) {
    int fd = open(".monitor_pid", O_RDONLY);
    if (fd < 0) {
        log_action(district, role, user, "ADD_REPORT - Monitor not informed");
        return;
    }
    char buf[16];
    int n = read(fd, buf, 15);
    buf[n] = '\0';
    close(fd);
    pid_t pid = atoi(buf);
    if (kill(pid, SIGUSR1) == 0) {
        log_action(district, role, user, "add");
    } else {
        log_action(district, role, user, "ADD_REPORT - Monitor error");
    }
}

int parse_condition(const char *input, char *field, char *op, char *value) {
    return sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3;
}

int match_condition(Report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity_level == val;
        if (strcmp(op, ">=") == 0) return r->severity_level >= val;
        if (strcmp(op, "<=") == 0) return r->severity_level <= val;
    }
    if (strcmp(field, "category") == 0) {
        if (strcmp(op, "==") == 0) return strcmp(r->issue_category, value) == 0;
    }
    return 0;
}

void update_threshold(const char *district, int value) {
    char path[256];
    sprintf(path, "%s/district.cfg", district);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fd < 0) return;
    char buf[16];
    int len = sprintf(buf, "%d\n", value);
    write(fd, buf, len);
    close(fd);
}

void add_report(const char *district, const char *user, const char *role) {
    mkdir(district, 0750);
    char path[256];
    sprintf(path, "%s/reports.dat", district);
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0664);
    if (fd < 0) { perror("Open failed"); return; }

    Report r;
    r.report_id = rand() % 10000;
    strncpy(r.inspector_name, user, MAX_STR);

    printf("X: "); scanf("%f", &r.latitude);
    printf("Y: "); scanf("%f", &r.longitude);
    printf("Category (road/lighting/flooding/other): "); scanf("%s", r.issue_category);
    printf("Severity level (1/2/3): "); scanf("%d", &r.severity_level);
    printf("Description: "); getchar();
    fgets(r.description, 256, stdin);
    r.description[strcspn(r.description, "\n")] = 0;

    r.timestamp = time(NULL);
    write(fd, &r, sizeof(Report));
    close(fd);

    char link_name[256];
    sprintf(link_name, "active_reports-%s", district);
    unlink(link_name);
    symlink(path, link_name);
    
    update_threshold(district, 1);
    notify_monitor(district, role, user);
}

void list_reports(const char *district) {
    char path[256];
    sprintf(path, "%s/reports.dat", district);
    struct stat st;
    if (lstat(path, &st) < 0) { printf("No reports.\n"); return; }
    int fd = open(path, O_RDONLY);
    Report r;
    while (read(fd, &r, sizeof(Report)) > 0) {
        printf("ID: %d | Cat: %s | Sev: %d | User: %s | X: %.2f | Y: %.2f\n", 
                r.report_id, r.issue_category, r.severity_level, r.inspector_name, r.latitude, r.longitude);
    }
    close(fd);
}

void remove_report(const char *district, int target_id, const char *role, const char *user) {
    if (strcmp(role, "manager") != 0) { printf("Error: Manager only.\n"); return; }
    char path[256];
    sprintf(path, "%s/reports.dat", district);
    int fd = open(path, O_RDWR);
    if (fd < 0) return;
    Report r;
    long pos = -1;
    while (read(fd, &r, sizeof(Report)) > 0) {
        if (r.report_id == target_id) {
            pos = lseek(fd, 0, SEEK_CUR) - sizeof(Report);
            break;
        }
    }
    if (pos != -1) {
        Report next;
        while (pread(fd, &next, sizeof(Report), pos + sizeof(Report)) > 0) {
            pwrite(fd, &next, sizeof(Report), pos);
            pos += sizeof(Report);
        }
        struct stat st;
        fstat(fd, &st);
        ftruncate(fd, st.st_size - sizeof(Report));
        log_action(district, role, user, "REMOVE_REPORT");
    }
    close(fd);
}

void remove_district(const char *district, const char *role) {
    if (strcmp(role, "manager") != 0) {
        printf("Error: Manager only.\n"); return;
    }
    char link_name[256];
    sprintf(link_name, "active_reports-%s", district);
    unlink(link_name);

    pid_t pid = fork();
    if (pid == 0) {
        execlp("rm", "rm", "-rf", district, NULL);
        exit(0);
    } else {
        wait(NULL);
        printf("District %s removed.\n", district);
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    if (argc < 6) {
        printf("Usage: ./city_manager --role <r> --user <u> <cmd> <dist> [extra]\n");
        return 1;
    }

    char *role = argv[2], *user = argv[4], *cmd = argv[5], *district = argv[6];
    
    if (strcmp(cmd, "add") == 0 || strcmp(cmd, "--add") == 0) add_report(district, user, role);
    else if (strcmp(cmd, "list") == 0) list_reports(district);
    else if (strcmp(cmd, "remove_report") == 0) remove_report(district, atoi(argv[7]), role, user);
    else if (strcmp(cmd, "remove_district") == 0) remove_district(district, role);
    else if (strcmp(cmd, "filter") == 0) {
        char path[256]; sprintf(path, "%s/reports.dat", district);
        int fd = open(path, O_RDONLY);
        Report r;
        while (read(fd, &r, sizeof(Report)) > 0) {
            int m = 1;
            for (int i = 7; i < argc; i++) {
                char f[50], o[5], v[100];
                if (parse_condition(argv[i], f, o, v))
                    if (!match_condition(&r, f, o, v)) { m = 0; break; }
            }
            if (m) printf("MATCH -> ID: %d | Cat: %s\n", r.report_id, r.issue_category);
        }
        close(fd);
    }
    else printf("Unknown command: %s\n", cmd);
    
    return 0;
}
