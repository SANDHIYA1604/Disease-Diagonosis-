#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "disease_engine.h"
#include "csv_parser.h"

// Global array to store symptom names from header
char symptom_headers[MAX_SYMPTOMS][MAX_SYMPTOM_LEN];
int header_count = 0;

void trim_whitespace(char *str) {
    char *start = str;
    char *end;
    
    while(isspace((unsigned char)*start)) start++;
    
    if(*start == 0) {
        str[0] = '\0';
        return;
    }
    
    end = start + strlen(start) - 1;
    while(end > start && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    memmove(str, start, end - start + 2);
}

int load_disease_database(DiseaseDatabase *db, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return -1;
    }
    
    char line[MAX_LINE_LEN];
    db->count = 0;
    
    // Read header line to get symptom names
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return -1;
    }
    
    char fields[MAX_CSV_FIELDS][MAX_SYMPTOM_LEN];
    int field_count = parse_csv_line(line, fields, MAX_CSV_FIELDS);
    
    // Store symptom names (skip first field which is "Disease")
    header_count = 0;
    for (int i = 1; i < field_count && header_count < MAX_SYMPTOMS; i++) {
        strncpy(symptom_headers[header_count], fields[i], MAX_SYMPTOM_LEN - 1);
        symptom_headers[header_count][MAX_SYMPTOM_LEN - 1] = '\0';
        trim_whitespace(symptom_headers[header_count]);
        header_count++;
    }
    
    printf("Loaded %d symptom types from header\n", header_count);
    
    // Read disease data
    while (fgets(line, sizeof(line), file) && db->count < MAX_DISEASES) {
        field_count = parse_csv_line(line, fields, MAX_CSV_FIELDS);
        
        if (field_count >= 2) {
            Disease *disease = &db->diseases[db->count];
            
            // First field is disease name
            strncpy(disease->name, fields[0], MAX_NAME_LEN - 1);
            disease->name[MAX_NAME_LEN - 1] = '\0';
            
            // Parse 1s and 0s to get symptoms
            disease->symptom_count = 0;
            for (int i = 1; i < field_count && (i-1) < header_count; i++) {
                trim_whitespace(fields[i]);
                if (strcmp(fields[i], "1") == 0) {
                    // This symptom is present
                    strncpy(disease->symptoms[disease->symptom_count], 
                           symptom_headers[i-1], MAX_SYMPTOM_LEN - 1);
                    disease->symptoms[disease->symptom_count][MAX_SYMPTOM_LEN - 1] = '\0';
                    disease->symptom_count++;
                }
            }
            
            if (disease->symptom_count > 0) {
                db->count++;
            }
        }
    }
    
    fclose(file);
    printf("Loaded %d diseases from database\n", db->count);
    return db->count;
}

int calculate_match_score(Disease *disease, char symptoms[][MAX_SYMPTOM_LEN], 
                         int symptom_count) {
    int score = 0;
    
    for (int i = 0; i < symptom_count; i++) {
        for (int j = 0; j < disease->symptom_count; j++) {
            if (strcasecmp(symptoms[i], disease->symptoms[j]) == 0) {
                score++;
                break;
            }
        }
    }
    
    return score;
}

void get_all_symptoms(char result[][MAX_SYMPTOM_LEN], int *count) {
    *count = header_count;
    for (int i = 0; i < header_count; i++) {
        strncpy(result[i], symptom_headers[i], MAX_SYMPTOM_LEN - 1);
        result[i][MAX_SYMPTOM_LEN - 1] = '\0';
    }
}

void diagnose_disease(DiseaseDatabase *db, char symptoms[][MAX_SYMPTOM_LEN], 
                     int symptom_count, char *result, int result_size) {
    typedef struct {
        char name[MAX_NAME_LEN];
        int score;
        int total_symptoms;
        int index;
    } DiseaseMatch;
    
    DiseaseMatch matches[MAX_DISEASES];
    int match_count = 0;
    
    // Calculate scores for all diseases
    for (int i = 0; i < db->count; i++) {
        int score = calculate_match_score(&db->diseases[i], symptoms, symptom_count);
        
        if (score > 0) {
            // Check if this disease name already exists
            int found = -1;
            for (int j = 0; j < match_count; j++) {
                if (strcmp(matches[j].name, db->diseases[i].name) == 0) {
                    found = j;
                    break;
                }
            }
            
            // If disease already exists, keep the one with better score
            if (found >= 0) {
                if (score > matches[found].score) {
                    matches[found].score = score;
                    matches[found].total_symptoms = db->diseases[i].symptom_count;
                    matches[found].index = i;
                }
            } else {
                // Add new disease
                strncpy(matches[match_count].name, db->diseases[i].name, MAX_NAME_LEN - 1);
                matches[match_count].name[MAX_NAME_LEN - 1] = '\0';
                matches[match_count].score = score;
                matches[match_count].total_symptoms = db->diseases[i].symptom_count;
                matches[match_count].index = i;
                match_count++;
            }
        }
    }
    
    // Sort matches by score (descending)
    for (int i = 0; i < match_count - 1; i++) {
        for (int j = i + 1; j < match_count; j++) {
            if (matches[j].score > matches[i].score) {
                DiseaseMatch temp = matches[i];
                matches[i] = matches[j];
                matches[j] = temp;
            }
        }
    }
    
    // Build result JSON
    if (match_count == 0) {
        snprintf(result, result_size, 
                "{\"status\":\"no_match\",\"message\":\"No matching diseases found\",\"matches\":[]}");
    } else {
        snprintf(result, result_size, 
                "{\"status\":\"success\",\"matches\":[")
        
        int limit = match_count < 10 ? match_count : 10; // Show top 10 results
        for (int i = 0; i < limit; i++) {
            char temp[512];
            snprintf(temp, sizeof(temp), 
                    "%s{\"disease\":\"%s\",\"match_score\":%d,\"total_symptoms\":%d}",
                    i > 0 ? "," : "", matches[i].name, matches[i].score, matches[i].total_symptoms);
            strncat(result, temp, result_size - strlen(result) - 1);
        }
        
        strncat(result, "]}", result_size - strlen(result) - 1);
    }
}