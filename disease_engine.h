#ifndef DISEASE_ENGINE_H
#define DISEASE_ENGINE_H

#define MAX_DISEASES 500
#define MAX_SYMPTOMS 100
#define MAX_NAME_LEN 200
#define MAX_SYMPTOM_LEN 100
#define MAX_LINE_LEN 2048

typedef struct {
    char name[MAX_NAME_LEN];
    char symptoms[MAX_SYMPTOMS][MAX_SYMPTOM_LEN];
    int symptom_count;
} Disease;

typedef struct {
    Disease diseases[MAX_DISEASES];
    int count;
} DiseaseDatabase;

// Function prototypes
int load_disease_database(DiseaseDatabase *db, const char *filename);
void diagnose_disease(DiseaseDatabase *db, char symptoms[][MAX_SYMPTOM_LEN], 
                     int symptom_count, char *result, int result_size);
void trim_whitespace(char *str);
int calculate_match_score(Disease *disease, char symptoms[][MAX_SYMPTOM_LEN], 
                         int symptom_count);
void get_all_symptoms(char result[][MAX_SYMPTOM_LEN], int *count);

#endif