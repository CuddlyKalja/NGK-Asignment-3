#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define PORT 9000           // Port number for UDP server
#define BUFFER_SIZE 256     // Buffer size for requests and responses

// Prints an error message and exits the program
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Handles client requests by reading system information from /proc files
void handle_request(char request, char *response, size_t size) {
    FILE *file;

    // Determine which system file to read based on request type
    if (request == 'U') {
        file = fopen("/proc/uptime", "r");  // Get system uptime
    } else if (request == 'L') {
        file = fopen("/proc/loadavg", "r");  // Get system load average
    } else {
        snprintf(response, size, "Invalid request\n");
        return;
    }

    // Handle file read errors
    if (file == NULL) {
        snprintf(response, size, "Error reading system file\n");
        return;
    }

    fgets(response, size, file);  // Read file content into response buffer
    fclose(file);  // Close the file after reading
}

int main() {
    printf("Starting UDP server on port %d...\n", PORT);

    int sock;
    struct sockaddr_in server, client;
    char buf[BUFFER_SIZE];
    socklen_t client_len = sizeof(client);

    // Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) error("ERROR opening socket");

    // Zero out the server address structure and set it up
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;  // Accept connections from any IP
    server.sin_port = htons(PORT);  // Convert port to network byte order

    // Bind the socket to the specified port
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
        error("ERROR on binding");

    while (1) {
        printf("Waiting for request...\n");

        // Receive data from a client
        int n = recvfrom(sock, buf, BUFFER_SIZE - 1, 0, (struct sockaddr*)&client, &client_len);
        if (n < 0) error("ERROR on recvfrom");

        buf[n] = '\0';  // Null-terminate received data
        char request = toupper(buf[0]);  // Convert request to uppercase
        printf("Received request: %c\n", request);

        // Process request and prepare response
        char response[BUFFER_SIZE];
        handle_request(request, response, BUFFER_SIZE);

        // Send response back to client
        n = sendto(sock, response, strlen(response), 0, (struct sockaddr*)&client, client_len);
        if (n < 0) error("ERROR on sendto");
    }

    close(sock);  // Close socket (never actually reached due to infinite loop)
    return 0;
}
