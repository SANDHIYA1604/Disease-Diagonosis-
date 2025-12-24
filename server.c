#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

#include "disease_engine.h"

#define PORT 8080
#define BUFFER_SIZE 4096

DiseaseDatabase db;

void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && 
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

void parse_symptoms(char *query, char symptoms[][MAX_SYMPTOM_LEN], int *count) {
    *count = 0;
    char decoded[BUFFER_SIZE];
    url_decode(decoded, query);
    
    char *token = strtok(decoded, "&");
    while (token != NULL && *count < MAX_SYMPTOMS) {
        if (strncmp(token, "symptoms=", 9) == 0) {
            char *symptom = token + 9;
            
            // Split by comma
            char *comma = strchr(symptom, ',');
            if (comma) {
                char temp[MAX_SYMPTOM_LEN];
                strncpy(temp, symptom, sizeof(temp) - 1);
                temp[sizeof(temp) - 1] = '\0';
                
                char *s = strtok(temp, ",");
                while (s != NULL && *count < MAX_SYMPTOMS) {
                    trim_whitespace(s);
                    if (strlen(s) > 0) {
                        strncpy(symptoms[*count], s, MAX_SYMPTOM_LEN - 1);
                        symptoms[*count][MAX_SYMPTOM_LEN - 1] = '\0';
                        (*count)++;
                    }
                    s = strtok(NULL, ",");
                }
            } else {
                trim_whitespace(symptom);
                if (strlen(symptom) > 0) {
                    strncpy(symptoms[*count], symptom, MAX_SYMPTOM_LEN - 1);
                    symptoms[*count][MAX_SYMPTOM_LEN - 1] = '\0';
                    (*count)++;
                }
            }
        }
        token = strtok(NULL, "&");
    }
}

void handle_request(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE * 2];
    
    recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    buffer[BUFFER_SIZE - 1] = '\0';
    
    // Parse HTTP request
    char method[16], path[256];
    sscanf(buffer, "%s %s", method, path);
    
    printf("Request: %s %s\n", method, path);
    
    if (strcmp(method, "GET") == 0 && strncmp(path, "/symptoms", 9) == 0) {
        // Get all symptoms from header
        char all_symptoms[MAX_SYMPTOMS][MAX_SYMPTOM_LEN];
        int symptom_count;
        
        get_all_symptoms(all_symptoms, &symptom_count);
        
        // Build JSON array
        char symptoms_json[BUFFER_SIZE * 4];
        strcpy(symptoms_json, "{\"symptoms\":[");
        for (int i = 0; i < symptom_count; i++) {
            char temp[256];
            snprintf(temp, sizeof(temp), "%s\"%s\"", i > 0 ? "," : "", all_symptoms[i]);
            strncat(symptoms_json, temp, sizeof(symptoms_json) - strlen(symptoms_json) - 1);
        }
        strncat(symptoms_json, "]}", sizeof(symptoms_json) - strlen(symptoms_json) - 1);
        
        snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: %d\r\n"
                "\r\n%s",
                (int)strlen(symptoms_json), symptoms_json);
    } else if (strcmp(method, "POST") == 0 && strncmp(path, "/diagnose", 9) == 0) {
        // Find the body of POST request
        char *body = strstr(buffer, "\r\n\r\n");
        if (body) {
            body += 4;
            
            char symptoms[MAX_SYMPTOMS][MAX_SYMPTOM_LEN];
            int symptom_count;
            parse_symptoms(body, symptoms, &symptom_count);
            
            printf("Received %d symptoms\n", symptom_count);
            
            char result[BUFFER_SIZE];
            diagnose_disease(&db, symptoms, symptom_count, result, sizeof(result));
            
            snprintf(response, sizeof(response),
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n%s",
                    (int)strlen(result), result);
        } else {
            snprintf(response, sizeof(response),
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: application/json\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "\r\n{\"error\":\"Invalid request\"}");
        }
    } else if (strcmp(method, "OPTIONS") == 0) {
        snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n"
                "\r\n");
    } else {
        snprintf(response, sizeof(response),
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "\r\n{\"error\":\"Endpoint not found\"}");
    }
    
    send(client_socket, response, (int)strlen(response), 0);
    close(client_socket);
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }
#endif

    // Load disease database
    if (load_disease_database(&db, "../data/data.csv") < 0) {
        fprintf(stderr, "Failed to load disease database\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    
    SOCKET server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        perror("Socket failed");
#ifdef _WIN32
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
#ifdef _WIN32
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        perror("Bind failed");
#ifdef _WIN32
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 10) == SOCKET_ERROR) {
        perror("Listen failed");
#ifdef _WIN32
        WSACleanup();
#endif
        exit(EXIT_FAILURE);
    }
    
    printf("Server running on http://localhost:%d\n", PORT);
    printf("Waiting for connections...\n");
    
    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, 
                                   &addrlen)) == INVALID_SOCKET) {
            perror("Accept failed");
            continue;
        }
        
        handle_request(client_socket);
    }
    
#ifdef _WIN32
    closesocket(server_fd);
    WSACleanup();
#endif
    
    return 0;
}