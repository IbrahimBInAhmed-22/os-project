#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096
#define PORT 8080

/**
 * Simple test client for the Dropbox server
 * Usage: ./client
 */

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    // Convert IP address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to server!\n");
    
    // Receive authentication prompt
    memset(buffer, 0, BUFFER_SIZE);
    recv(sock, buffer, BUFFER_SIZE - 1, 0);
    printf("%s", buffer);
    
    // Authenticate
    printf("Enter command (SIGNUP user pass or LOGIN user pass): ");
    fgets(buffer, BUFFER_SIZE, stdin);
    send(sock, buffer, strlen(buffer), 0);
    
    // Receive authentication response
    memset(buffer, 0, BUFFER_SIZE);
    recv(sock, buffer, BUFFER_SIZE - 1, 0);
    printf("%s", buffer);
    
    if (strncmp(buffer, "ERROR", 5) == 0) {
        close(sock);
        return 1;
    }
    
    // Command loop
    printf("\nAvailable commands:\n");
    printf("  UPLOAD <filename>   - Upload a file\n");
    printf("  DOWNLOAD <filename> - Download a file\n");
    printf("  DELETE <filename>   - Delete a file\n");
    printf("  LIST                - List all files\n");
    printf("  QUIT                - Disconnect\n\n");
    
    while (1) {
        printf("> ");
        fgets(buffer, BUFFER_SIZE, stdin);
        
        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strlen(buffer) == 0) continue;
        
        // Check for QUIT
        if (strncmp(buffer, "QUIT", 4) == 0) {
            send(sock, "QUIT\n", 5, 0);
            memset(buffer, 0, BUFFER_SIZE);
            recv(sock, buffer, BUFFER_SIZE - 1, 0);
            printf("%s", buffer);
            break;
        }
        
        // Parse command
        char cmd[20], filename[256];
        int parsed = sscanf(buffer, "%19s %255s", cmd, filename);
        
        if (parsed < 1) {
            printf("Invalid command\n");
            continue;
        }
        
        // Handle UPLOAD
        if (strcmp(cmd, "UPLOAD") == 0 && parsed == 2) {
            // Open local file
            FILE *file = fopen(filename, "rb");
            if (!file) {
                printf("Error: Cannot open file '%s'\n", filename);
                continue;
            }
            
            // Get file size
            fseek(file, 0, SEEK_END);
            long file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            
            // Send command
            char cmd_buffer[300];
            snprintf(cmd_buffer, sizeof(cmd_buffer), "UPLOAD %s\n", filename);
            send(sock, cmd_buffer, strlen(cmd_buffer), 0);
            
            // Send file size
            snprintf(cmd_buffer, sizeof(cmd_buffer), "%ld", file_size);
            send(sock, cmd_buffer, strlen(cmd_buffer), 0);
            usleep(10000);  // Small delay
            
            // Send file data
            char *file_data = malloc(file_size);
            fread(file_data, 1, file_size, file);
            send(sock, file_data, file_size, 0);
            free(file_data);
            fclose(file);
            
            // Receive response
            memset(buffer, 0, BUFFER_SIZE);
            recv(sock, buffer, BUFFER_SIZE - 1, 0);
            printf("%s", buffer);
            
        }
        // Handle DOWNLOAD
        else if (strcmp(cmd, "DOWNLOAD") == 0 && parsed == 2) {
            // Send command
            strcat(buffer, "\n");
            send(sock, buffer, strlen(buffer), 0);
            
            // Receive response with file size
            memset(buffer, 0, BUFFER_SIZE);
            recv(sock, buffer, BUFFER_SIZE - 1, 0);
            
            if (strncmp(buffer, "ERROR", 5) == 0) {
                printf("%s", buffer);
                continue;
            }
            
            // Parse file size
            long file_size;
            sscanf(buffer, "OK: %ld", &file_size);
            printf("Receiving %ld bytes...\n", file_size);
            
            // Receive file data
            FILE *file = fopen(filename, "wb");
            if (!file) {
                printf("Error: Cannot create file '%s'\n", filename);
                continue;
            }
            
            long received = 0;
            while (received < file_size) {
                ssize_t bytes = recv(sock, buffer, 
                                    (file_size - received > BUFFER_SIZE) ? BUFFER_SIZE : file_size - received, 
                                    0);
                if (bytes <= 0) break;
                fwrite(buffer, 1, bytes, file);
                received += bytes;
            }
            
            fclose(file);
            printf("OK: File downloaded successfully\n");
            
        }
        // Handle DELETE and LIST
        else {
            strcat(buffer, "\n");
            send(sock, buffer, strlen(buffer), 0);
            
            // Receive response
            memset(buffer, 0, BUFFER_SIZE);
            recv(sock, buffer, BUFFER_SIZE - 1, 0);
            printf("%s", buffer);
        }
    }
    
    close(sock);
    printf("Disconnected from server\n");
    
    return 0;
}