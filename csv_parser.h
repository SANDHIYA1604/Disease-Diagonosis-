#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#define MAX_CSV_FIELDS 100

// Function to parse a CSV line
int parse_csv_line(char *line, char fields[][MAX_SYMPTOM_LEN], int max_fields);

#endif