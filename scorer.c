#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_STR 64

typedef struct {
    int raport_id;
    char inspector_name[MAX_STR];
    float latitudine;
    float longitude;
    char issue_category[MAX_STR];
    int severity_level;
    long timestamp;
    char description[256];
} ReportRecord;

void calculate_district_score(const char *district_name) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "./%s/reports.dat", district_name);

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        printf("Districtul [%s]: Nu s-a putut deschide reports.dat (poate nu exista date)\n", district_name);
        return;
    }

    char inspectors[100][50];
    int scores[100] = {0};
    int total_inspectors = 0;

    ReportRecord r;
    while (fread(&r, sizeof(ReportRecord), 1, file) == 1) {
        int found = 0;
        for (int i = 0; i < total_inspectors; i++) {
            if (strcmp(inspectors[i], r.inspector_name) == 0) {
                scores[i] += r.severity_level;
                found = 1;
                break;
            }
        }
        if (!found && total_inspectors < 100) {
            strcpy(inspectors[total_inspectors], r.inspector_name);
            scores[total_inspectors] = r.severity_level;
            total_inspectors++;
        }
    }
    fclose(file);

    printf("District: %s\n", district_name);
    for (int i = 0; i < total_inspectors; i++) {
        printf("  -> Inspector: %s | Workload Score: %d\n", inspectors[i], scores[i]);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Eroare argumente scorer.\n");
        return 1;
    }

    char *list = strdup(argv[1]);
    char *token = strtok(list, " ");
    while (token != NULL) {
        calculate_district_score(token);
        token = strtok(NULL, " ");
    }
    free(list);
    return 0;
}
