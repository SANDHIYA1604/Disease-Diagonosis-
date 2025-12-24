#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "disease_engine.h"
#include "csv_parser.h"

int parse_csv_line(char *line, char fields[][MAX_SYMPTOM_LEN], int max_fields) {
    int field_count = 0;
    int in_quotes = 0;
    int field_pos = 0;
    
    for (int i = 0; line[i] != '\0' && field_count < max_fields; i++) {
        if (line[i] == '"') {
            in_quotes = !in_quotes;
        } else if (line[i] == ',' && !in_quotes) {
            fields[field_count][field_pos] = '\0';
            trim_whitespace(fields[field_count]);
            field_count++;
            field_pos = 0;
        } else if (line[i] != '\r' && line[i] != '\n') {
            if (field_pos < MAX_SYMPTOM_LEN - 1) {
                fields[field_count][field_pos++] = line[i];
            }
        }
    }
    
    // Add the last field
    if (field_pos > 0 || field_count > 0) {
        fields[field_count][field_pos] = '\0';
        trim_whitespace(fields[field_count]);
        field_count++;
    }
    
    return field_count;
}