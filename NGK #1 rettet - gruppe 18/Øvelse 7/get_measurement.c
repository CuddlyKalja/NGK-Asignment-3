#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define PORT 9000          // Port number for communication
#define BUFFER_SIZE 256    // Buffer size for sending/receiving messages

// Error handling function that prints an error message and exits the program
void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    printf("Starting UDP client...\n");

    // Ensure correct command-line arguments
    if (argc != 3) {
        error("USAGE: ./client_udp <server_ip> <u|l>");
    }

    char *server_ip = argv[1];
    char request = toupper(argv[2][0]);  // Convert request type to uppercase

    // Validate request type ('U' for uppercase, 'L' for lowercase)
    if (request != 'U' && request != 'L') {
        error("Invalid request. Use 'u' or 'l'.");
    }

    int sock, n;
    struct sockaddr_in server;
    struct hostent *hp;
    char buf[BUFFER_SIZE];
    socklen_t serverlength = sizeof(server);

    // Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) error("ERROR opening socket");

    server.sin_family = AF_INET;

    // Resolve hostname to IP address
    hp = gethostbyname(server_ip);
    if (hp == NULL) error("ERROR unknown host");

    memcpy(&server.sin_addr, hp->h_addr_list[0], hp->h_length);
    server.sin_port = htons(PORT);  // Convert port to network byte order

    // Prepare the request message
    buf[0] = request;
    buf[1] = '\0';

    printf("Sending request: %c\n", request);

    // Send request to the server
    n = sendto(sock, buf, strlen(buf), 0, (struct sockaddr *)&server, serverlength);
    if (n < 0) error("ERROR sending request");

    // Receive response from the server
    n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&server, &serverlength);
    if (n < 0) error("ERROR receiving response");

    buf[n] = '\0';  // Null-terminate received data
    printf("Server response: %s\n", buf);

    close(sock);  // Close socket
    return 0;
}
