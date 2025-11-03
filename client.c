#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 4096

/* Helper to receive a line from server */
int recv_line(int sock, char *buffer, size_t size) {
    memset(buffer, 0, size);
    int n = recv(sock, buffer, size - 1, 0);
    return n;
}

/* Upload a local file to the server */
int handle_upload(int sock, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("ERROR: Cannot open local file '%s'\n", filename);
        return -1;
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    printf("Uploading '%s' (%ld bytes)...\n", filename, file_size);
    
    /* Send UPLOAD command */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "UPLOAD %s\n", filename);
    send(sock, cmd, strlen(cmd), 0);
    
    /* Receive READY response */
    char buffer[BUFFER_SIZE];
    int n = recv_line(sock, buffer, sizeof(buffer));
    if (n <= 0) {
        fclose(fp);
        return -1;
    }
    
    printf("Server: %s", buffer);
    
    if (strncmp(buffer, "READY:", 6) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Send SIZE */
    snprintf(cmd, sizeof(cmd), "SIZE %ld\n", file_size);
    send(sock, cmd, strlen(cmd), 0);
    
    /* Receive OK response */
    n = recv_line(sock, buffer, sizeof(buffer));
    if (n <= 0 || strncmp(buffer, "OK:", 3) != 0) {
        printf("Server: %s", buffer);
        fclose(fp);
        return -1;
    }
    
    printf("Server: %s", buffer);
    
    /* Send file data */
    char chunk[4096];
    size_t bytes;
    long sent = 0;
    
    while ((bytes = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        send(sock, chunk, bytes, 0);
        sent += bytes;
        printf("\rProgress: %ld / %ld bytes (%.1f%%)", 
               sent, file_size, (sent * 100.0) / file_size);
        fflush(stdout);
    }
    
    printf("\n");
    fclose(fp);
    
    /* Receive final response */
    n = recv_line(sock, buffer, sizeof(buffer));
    if (n > 0) {
        printf("Server: %s", buffer);
    }
    
    return 0;
}

/* Download a file from the server */
int handle_download(int sock, const char *filename) {
    /* Send DOWNLOAD command */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "DOWNLOAD %s\n", filename);
    send(sock, cmd, strlen(cmd), 0);
    
    /* Receive SIZE response */
    char buffer[BUFFER_SIZE];
    int n = recv_line(sock, buffer, sizeof(buffer));
    if (n <= 0) return -1;
    
    printf("Server: %s", buffer);
    
    long file_size;
    if (sscanf(buffer, "SIZE: %ld", &file_size) != 1) {
        return -1;
    }
    
    /* Create local file */
    char local_filename[256];
    snprintf(local_filename, sizeof(local_filename), "downloaded_%s", filename);
    
    FILE *fp = fopen(local_filename, "wb");
    if (!fp) {
        printf("ERROR: Cannot create local file\n");
        return -1;
    }
    
    printf("Downloading to '%s' (%ld bytes)...\n", local_filename, file_size);
    
    /* Receive file data */
    long received = 0;
    while (received < file_size) {
        long to_recv = file_size - received;
        if (to_recv > sizeof(buffer)) to_recv = sizeof(buffer);
        
        int bytes = recv(sock, buffer, to_recv, 0);
        if (bytes <= 0) break;
        
        fwrite(buffer, 1, bytes, fp);
        received += bytes;
        
        printf("\rProgress: %ld / %ld bytes (%.1f%%)",
               received, file_size, (received * 100.0) / file_size);
        fflush(stdout);
    }
    
    printf("\n");
    fclose(fp);
    
    if (received == file_size) {
        printf("SUCCESS: Download complete\n");
        return 0;
    } else {
        printf("ERROR: Incomplete download\n");
        return -1;
    }
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char input[BUFFER_SIZE];
    int port = 8080;
    const char *host = "127.0.0.1";
    
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    
    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    /* Connect to server */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }
    
    printf("Connecting to %s:%d...\n", host, port);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    
    printf("Connected!\n\n");
    
    /* Receive welcome message */
    memset(buffer, 0, sizeof(buffer));
    int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        printf("%s\n", buffer);
    }
    
    /* Interactive command loop */
    printf("Commands:\n");
    printf("  REGISTER <user> <pass>\n");
    printf("  LOGIN <user> <pass>\n");
    printf("  UPLOAD <local_file>\n");
    printf("  DOWNLOAD <remote_file>\n");
    printf("  DELETE <file>\n");
    printf("  LIST\n");
    printf("  QUIT\n\n");
    
    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        /* Remove newline */
        input[strcspn(input, "\r\n")] = 0;
        
        /* Parse command */
        char cmd[16], arg[256];
        memset(cmd, 0, sizeof(cmd));
        memset(arg, 0, sizeof(arg));
        sscanf(input, "%s %s", cmd, arg);
        
        /* Check for quit */
        if (strcmp(cmd, "QUIT") == 0) {
            send(sock, "QUIT\n", 5, 0);
            break;
        }
        
        /* Handle special commands */
        if (strcmp(cmd, "UPLOAD") == 0) {
            if (strlen(arg) == 0) {
                printf("Usage: UPLOAD <local_filename>\n");
                continue;
            }
            handle_upload(sock, arg);
            continue;
        }
        
        if (strcmp(cmd, "DOWNLOAD") == 0) {
            if (strlen(arg) == 0) {
                printf("Usage: DOWNLOAD <remote_filename>\n");
                continue;
            }
            handle_download(sock, arg);
            continue;
        }
        
        /* Send regular command to server */
        strcat(input, "\n");
        send(sock, input, strlen(input), 0);
        
        /* Receive response */
        memset(buffer, 0, sizeof(buffer));
        n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            printf("Server disconnected\n");
            break;
        }
        
        printf("%s\n", buffer);
    }
    
    close(sock);
    printf("Disconnected.\n");
    return 0;
}