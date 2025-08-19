/* A simple client in the internet domain using TCP
The ip adresse and port number on server is passed as arguments 
Based on example: https://www.linuxhowtos.org/C_C++/socket.htm 

Modified: Michael Alrøe
Extended to support file client!
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h> 
#include "iknlib.h"

#define STRBUFSIZE 256 //Buffer size for sending commands

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

void receiveFile(int serverSocket, const char* fileName, long fileSize)
{
	printf("Receiving: '%s', size: %li\n", fileName, fileSize);

	//Opens file in binary write mode:
	FILE *file = fopen(fileName, "wb");
	if(file==NULL)
	{
		perror("ERROR: Could not create file");
		return;
	}

	char buffer[1000]; //Buffer for file data
	long totalBytesReceived = 0;
	ssize_t bytesReceived;

	//Receives file data in chunks:
	while(totalBytesReceived<fileSize)
	{
		bzero(buffer, sizeof(buffer));
		bytesReceived = read(serverSocket, buffer, sizeof(buffer));

		if(bytesReceived < 0)
		{
			perror ("ERROR: Recieving file data");
			fclose(file);
			return;
		}
		if(bytesReceived == 0)
		{
			perror ("ERROR: Connection closed by server befor transfer completion");
			break;
		}

		//Writes received data to file:
		fwrite(buffer, 1, bytesReceived, file);
		totalBytesReceived += bytesReceived;

		printf("Recieved %ld/%ld bytes...\n", totalBytesReceived, fileSize);
	}
	fclose(file);

	//Verifies if the complete file was received:
	if(totalBytesReceived==fileSize)
	{
		printf("File transfer was succesful: %s\n", fileName);
	}
	else
	{
		printf("File transfer was unsuccesful. Received %ld of %ld bytes.\n", totalBytesReceived, fileSize);
	}

}

int main(int argc, char *argv[])
{
	printf("Starting client...\n");

	int sockfd, portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	uint8_t buffer[STRBUFSIZE];
    
	//Ensures number of arguments:
	if (argc < 3)
	{
		error( "ERROR usage: ""hostname""");
	}
	    
	portno = 9000; //Port number

	//Creates a TCP socket:
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
	{
		error("ERROR opening socket");
	}
	    
	// Resolves server hostname to IP address:
	server = gethostbyname(argv[1]);
	if (server == NULL) 
	{
		error("ERROR no such host");
	}
	    
	printf("Connecting to server...\n");

	//Sets up server address struct:
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	//Connects to server:
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
	{
		error("ERROR connecting");
	}
	
	//Sends file request to the server:
	strncpy((char*)buffer, argv[2], sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';  //0 termination
    writeTextTCP(sockfd, (char*)buffer);

	//Receives file size from server:
    long fileSize = readFileSizeTCP(sockfd);

    if (fileSize <= 0)
    {
        printf("Server error: File not found or invalid size \n");
        close(sockfd);
        return 1;
    }

    printf("File size received: %ld bytes\n", fileSize);

	//Receives and saves the requested file:
    receiveFile(sockfd, argv[2], fileSize);

    printf("Closing client...\n");
    close(sockfd);
    return 0;
}
