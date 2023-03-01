
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_CHUNKS 1024
#define MAX_FINGERPRINTS 1024
#define SERVER_PORT 8080

// Function to parse the file recipe
void parse_file_recipe(char *recipe_str, char *filename, int *num_chunks, int *num_fingerprints, int *fingerprints) {
    char *ptr = strtok(recipe_str, ":");
    strcpy(filename, ptr);

    ptr = strtok(NULL, ":");
    *num_chunks = atoi(ptr);

    ptr = strtok(NULL, ":");
    char *fingerprint_str = strtok(ptr, ",");
    while (fingerprint_str != NULL) {
        fingerprints[*num_fingerprints] = atoi(fingerprint_str);
        (*num_fingerprints)++;
        fingerprint_str = strtok(NULL, ",");
    }
}

// Function to select the fingerprints with frequency higher than 3
void select_fingerprints(int *fingerprints, int num_fingerprints, int *selected_fingerprints, int *num_selected_fingerprints) {
    int freq[MAX_FINGERPRINTS];
    memset(freq, 0, sizeof(freq));

    for (int i = 0; i < num_fingerprints; i++) {
        freq[fingerprints[i]]++;
    }

    for (int i = 0; i < num_fingerprints; i++) {
        if (freq[fingerprints[i]] > 3) {
            bool found = false;
            for (int j = 0; j < *num_selected_fingerprints; j++) {
                if (selected_fingerprints[j] == fingerprints[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                selected_fingerprints[*num_selected_fingerprints] = fingerprints[i];
                (*num_selected_fingerprints)++;
            }
        }
    }
}

// Function to handle client requests
void handle_request(int client_sockfd) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    read(client_sockfd, buffer, sizeof(buffer));

    char filename[256];
    int num_chunks;
    int num_fingerprints;
    int fingerprints[MAX_FINGERPRINTS];
    memset(fingerprints, 0, sizeof(fingerprints));
    parse_file_recipe(buffer, filename, &num_chunks, &num_fingerprints, fingerprints);

    int selected_fingerprints[MAX_FINGERPRINTS];
    int num_selected_fingerprints = 0;
    select_fingerprints(fingerprints, num_fingerprints, selected_fingerprints, &num_selected_fingerprints);

    char response[1024];
    memset(response, 0, sizeof(response));
    sprintf(response, "Selected fingerprints for file %s: ", filename);
    for (int i = 0; i < num_selected_fingerprints; i++) {
        char fingerprint_str[16];
        sprintf(fingerprint_str, "%d", selected_fingerprints[i]);
        strcat(response, fingerprint_str);
        strcat(response, ",");
    }
    response[strlen(response) - 1] = '\0';

    write(client_sockfd, response, strlen(response));
    close(client_sockfd);
}

// Main function
int main() {
    int server_sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    // Create socket
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

// Bind socket to port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

// Listen for incoming connections
    if (listen(server_sockfd, 5) < 0) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    while (1) {
        // Accept incoming connection
        int client_addrlen = sizeof(client_addr);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addrlen);
        if (client_sockfd < 0) {
            perror("Error accepting connection");
            exit(EXIT_FAILURE);
        }

        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));

        // Handle client request
        handle_request(client_sockfd);
    }

    return 0;
}

