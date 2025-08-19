/* A simple server in the internet domain using TCP
The port number is passed as an argument 
Based on example: https://www.linuxhowtos.org/C_C++/socket.htm 

Modified: Michael Alrøe
Extended to support file server!
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "iknlib.h"

#define STRBUFSIZE 256 // Buffer size
#define CHUNKSIZE 1000 // File transfer chunk size
#define PORT 9000 //Port number

void error(const char *msg) 
{
	perror(msg);
	exit(1);
}

void sendFile(int clientSocket, const char* fileName, long fileSize)
{
	printf("Sending: %s, size: %li\n", fileName, fileSize);

	// Opens the requested file in binary read mode:
	FILE *file = fopen(fileName, "rb");
	if(file==NULL)
	{
		perror("ERROR: File not found");
		return;
	}

	// Sends file size to client:
	char fileSizeStr[20];
	snprintf(fileSizeStr, sizeof(fileSizeStr), "%ld\n", fileSize);
	writeTextTCP(clientSocket,fileSizeStr);

	char buffer[CHUNKSIZE]; //buffer for file data
	size_t bytesRead;
	long totalBytesSent = 0;

	// Reads the file in chunks and sends to the client:
	while((bytesRead=fread(buffer, 1, CHUNKSIZE, file))>0)
	{
		ssize_t bytesSent = 0;

		while(bytesSent < bytesRead)
		{
			ssize_t result = write(clientSocket, buffer + bytesSent, bytesRead - bytesSent);
			if(result < 0)
			{
				perror("ERROR: Failed to send file");
				fclose(file);
				return;
			}
			bytesSent += result;
		}
		totalBytesSent += bytesSent;
		printf("Sent %ld/%ld bytes\n", totalBytesSent, fileSize);
	}
	
	printf("File transfer complete: %s (%ld bytes sent)\n", fileName, totalBytesSent);
	fclose(file);
}


int main(int argc, char *argv[])
{
	printf("Starting server...\n");

	int sockfd, newsockfd, portno;

	socklen_t clilen;
	uint8_t bufferRx[STRBUFSIZE];
	uint8_t bufferTx[STRBUFSIZE]; //buffer for receiving data from client
	struct sockaddr_in serv_addr, cli_addr;
	
	//Creates a TCP socket:
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		{
			error("ERROR opening socket");
		} 
		
	printf("Binding...\n");
	bzero((char *) &serv_addr, sizeof(serv_addr)); //Clears server address struct
	portno = PORT;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY; //Accepts connection from any IP
	serv_addr.sin_port = htons(portno); //Convert to network byte order

	//Binds the socket to the specified port
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
	{
		error("ERROR on binding");
	}
		
	printf("Listening for incoming connections...\n");
	listen(sockfd,5); //Allows up to 5 connections
	
	clilen = sizeof(cli_addr);

	//The main server loop - Accepts and handles client connections:
	for (;;)
	{
		printf("Waiting for client to connect...\n");
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

		if (newsockfd < 0) 
		{
			error("ERROR on accept");
		}
		else 
		{
			printf("Client connected\n");
		}
		
		//Receives file name from client
		bzero(bufferRx,sizeof(bufferRx));
		readTextTCP(newsockfd,(char*)bufferRx,sizeof(bufferRx));

		//Gets the file size:
		long fileSize = getFilesize((char*)bufferRx);

		//Sends the requested file to client:
		sendFile(newsockfd,(char*)bufferRx,fileSize);
		
		//Closes clients connections after transfer
		close(newsockfd);
	}
	close(sockfd);
	return 0; 
}