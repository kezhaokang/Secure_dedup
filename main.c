#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define CHUNK_SIZE 4096
#define MAX_CHUNKS 1024
#define MAX_FINGERPRINTS 1024
#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 8080

// Function to generate fingerprints from chunks
void generate_fingerprints(char **chunks, int num_chunks, int *fingerprints, int *num_fingerprints) {
    for (int i = 0; i < num_chunks; i++) {
        int fingerprint = 0;
        for (int j = 0; j < CHUNK_SIZE; j++) {
            fingerprint += chunks[i][j];
        }
        bool found = false;
        for (int j = 0; j < *num_fingerprints; j++) {
            if (fingerprints[j] == fingerprint) {
                found = true;
                break;
            }
        }
        if (!found) {
            fingerprints[*num_fingerprints] = fingerprint;
            (*num_fingerprints)++;
        }
    }
}

// Function to send file recipe to the server
void send_file_recipe(char *filename, int *fingerprints, int num_fingerprints) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error connecting to server");
        exit(1);
    }

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    char buffer[CHUNK_SIZE];
    int chunk_num = 0;
    int num_chunks = 0;
    char *chunks[MAX_CHUNKS];

    while (fread(buffer, 1, CHUNK_SIZE, fp) > 0) {
        char *chunk = (char *) malloc(CHUNK_SIZE);
        memcpy(chunk, buffer, CHUNK_SIZE);
        chunks[chunk_num] = chunk;
        chunk_num++;
        num_chunks++;

        if (chunk_num == MAX_CHUNKS) {
            int file_recipe[MAX_FINGERPRINTS];
            int num_file_recipe = 0;
            generate_fingerprints(chunks, chunk_num, file_recipe, &num_file_recipe);
            char recipe_str[1024];
            sprintf(recipe_str, "%s:%d:%d:", filename, num_chunks, num_file_recipe);
            for (int i = 0; i < num_file_recipe; i++) {
                char fingerprint_str[16];
                sprintf(fingerprint_str, "%d", file_recipe[i]);
                strcat(recipe_str, fingerprint_str);
                strcat(recipe_str, ",");
            }
            recipe_str[strlen(recipe_str) - 1] = '\0';
            write(sockfd, recipe_str, strlen(recipe_str));
            for (int i = 0; i < chunk_num; i++) {
                free(chunks[i]);
            }
            chunk_num = 0;
        }
    }

    if (chunk_num > 0) {
        int file_recipe[MAX_FINGERPRINTS];
        int num_file_recipe = 0;
        generate_fingerprints(chunks, chunk_num, file_recipe, &num_file_recipe);
        char recipe_str[1024];
        sprintf(recipe_str, "%s:%d:%d:", filename, num_chunks, num_file_recipe);
        for (int i = 0; i < num_file_recipe; i++) {
            char fingerprint_str[16];
            sprintf(fingerprint_str, "%d", file_recipe[i]);
            strcat(recipe_str, fingerprint_str);
            strcat(recipe_str, ",");
        }
        recipe_str[strlen(recipe_str) - 1] = '\0';
        write(sockfd, recipe_str, strlen(recipe_str));
        for (int i = 0; i < chunk_num; i++) {
            free(chunks[i]);
        }
    }// Receive the selected fingerprints from the server
    int selected_fingerprints[MAX_FINGERPRINTS];
    int num_selected_fingerprints = 0;
    char selected_fingerprints_str[1024];
    int num_bytes = read(sockfd, selected_fingerprints_str, 1024);
    if (num_bytes < 0) {
        perror("Error receiving selected fingerprints from server");
        exit(1);
    }
    selected_fingerprints_str[num_bytes] = '\0';
    char *fingerprint_str = strtok(selected_fingerprints_str, ",");
    while (fingerprint_str != NULL) {
        selected_fingerprints[num_selected_fingerprints] = atoi(fingerprint_str);
        num_selected_fingerprints++;
        fingerprint_str = strtok(NULL, ",");
    }

// Upload the chunks that match the selected fingerprints
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }
    chunk_num = 0;
    num_chunks = 0;
    int chunk_indexes[MAX_CHUNKS];
    while (fread(buffer, 1, CHUNK_SIZE, fp) > 0) {
        char *chunk = (char *) malloc(CHUNK_SIZE);
        memcpy(chunk, buffer, CHUNK_SIZE);
        int fingerprint = 0;
        for (int j = 0; j < CHUNK_SIZE; j++) {
            fingerprint += chunk[j];
        }
        for (int j = 0; j < num_selected_fingerprints; j++) {
            if (selected_fingerprints[j] == fingerprint) {
                // Found a matching fingerprint, upload the chunk
                char upload_str[1024];
                sprintf(upload_str, "%d:%d:", chunk_num, fingerprint);
                write(sockfd, upload_str, strlen(upload_str));
                write(sockfd, chunk, CHUNK_SIZE);
                chunk_indexes[num_chunks] = chunk_num;
                num_chunks++;
                break;
            }
        }
        chunk_num++;
        free(chunk);
    }

// Send the indexes of the uploaded chunks to the server
    char index_str[1024];
    sprintf(index_str, "%s:%d:", filename, num_chunks);
    for (int i = 0; i < num_chunks; i++) {
        char chunk_index_str[16];
        sprintf(chunk_index_str, "%d", chunk_indexes[i]);
        strcat(index_str, chunk_index_str);
        strcat(index_str, ",");
    }
    index_str[strlen(index_str) - 1] = '\0';
    write(sockfd, index_str, strlen(index_str));

    fclose(fp);
    close(sockfd);

}
// Function to download file chunks from the server
void download_file_chunks(char *filename, int *fingerprints, int num_fingerprints) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(1);
    }
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error connecting to server");
        exit(1);
    }

// Send the filename and fingerprints to the server
    char request_str[1024];
    sprintf(request_str, "%s:", filename);
    for (int i = 0; i < num_fingerprints; i++) {
        char fingerprint_str[16];
        sprintf(fingerprint_str, "%d", fingerprints[i]);
        strcat(request_str, fingerprint_str);
        strcat(request_str, ",");
    }
    request_str[strlen(request_str) - 1] = '\0';
    write(sockfd, request_str, strlen(request_str));

// Receive the file chunks from the server and write them to disk
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

    char buffer[CHUNK_SIZE];
    int num_chunks = 0;
    while (true) {
        int bytes_read = read(sockfd, buffer, CHUNK_SIZE);
        if (bytes_read <= 0) {
            break;
        }
        fwrite(buffer, 1, bytes_read, fp);
        num_chunks++;
    }

    fclose(fp);
    close(sockfd);

    printf("File downloaded successfully: %s (%d chunks)\n", filename, num_chunks);
}


// Main function
    int main() {
// Read the filename from user input
        char filename[256];
        printf("Enter filename: ");
        scanf("%s", filename);
        // Send the file recipe to the server
        int fingerprints[MAX_FINGERPRINTS];
        int num_fingerprints = 0;
        send_file_recipe(filename, fingerprints, &num_fingerprints);

// Download the file chunks from the server
        download_file_chunks(filename, fingerprints, num_fingerprints);

        return 0;
    }


