#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>

#define ECHO_PORT 5675
#define _BUFFER_SIZE 2000

int download_file(int socket, char buffer[_BUFFER_SIZE], char file_name[_BUFFER_SIZE], int size);
int send_file_size(char file_name[_BUFFER_SIZE], int socket);
void send_file(char file_name[_BUFFER_SIZE], int socket);
int recv_ACK(int socket);
int md5Hash(char file_name[_BUFFER_SIZE], char hash[_BUFFER_SIZE]);

int main(int argc, char *argv[])
{
	int socket_desc;
	struct sockaddr_in server;
	char *message, server_reply[_BUFFER_SIZE];

	printf("Initializing socket\n");

	//Create socket
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
	}

	char *address = "192.168.1.107";
	if (argc > 1)
		address = argv[1]; // Si existe, el primer argumento es la IP
	server.sin_addr.s_addr = inet_addr(address);
	server.sin_family = AF_INET;
	server.sin_port = htons(ECHO_PORT);

	printf("Trying to connect to address: %s\n", address);

	//Connect to remote server
	if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		printf("Connect error\n");
		return 1;
	}

	printf("Connected Successfully\n");

	//Send some data
/*	if (argc > 2)
		message = argv[2]; // Si existe, el segundo argumento es el mensaje
*/
message = "DOWNLOAD_FILE f1.txt";
	if (send(socket_desc, message, strlen(message) + 1, 0) < 0) // Importante el +1 para enviar el finalizador de cadena!!
	{
		printf("Send failed\n");
		return 1;
	}
	printf("Message sent. Content: %s\n", message);

	if (recv(socket_desc, server_reply, _BUFFER_SIZE, 0) < 0)
	{
		printf("Recv failed.\n");
	}
	else
	{
		char command[_BUFFER_SIZE], file_name[_BUFFER_SIZE];
		sscanf(message, "%s %s", command, file_name);

		if (strcmp(command, "DOWNLOAD_FILE") == 0)
		{
			printf("DOWNLOAD SIZE: %s\n", server_reply);

			int size;
			sscanf(server_reply, "%d", &size);

			send(socket_desc, "DOWNLOAD_ACK", _BUFFER_SIZE, 0);

			int confirmation = download_file(socket_desc, server_reply, file_name, size);

			if(confirmation == 0){
				send(socket_desc, "DOWNLOAD_COMPLETED", _BUFFER_SIZE, 0);
			}
			char currenthash[32];

			if(recv(socket_desc, server_reply, 33, 0) > 0){
				md5Hash(file_name, currenthash);

				if (strncmp(currenthash, server_reply, 33) == 0)
				{
					printf("DOWNLOAD WITHOUT ERRORS\n");
				}
				else
				{
					printf("DOWNLOAD WITH ERRORS\n");
				}

				printf("[+]Download complete.\n");
				
			}

			
		}
		else if (strcmp(command, "DELETE_FILE") == 0)
		{
			recv(socket_desc, server_reply, _BUFFER_SIZE, 0);
			printf("%s", server_reply);
		}
		else if (strcmp(command, "UPLOAD_FILE") == 0)
		{

			send_file_size(file_name, socket_desc);

			if (recv(socket_desc, server_reply, _BUFFER_SIZE, 0) > 0)
			{
				send_file(file_name, socket_desc);

				if (recv(socket_desc, server_reply, _BUFFER_SIZE, 0) > 0)
				{
					printf("%s", server_reply);
				}

				char hash[33];
				md5Hash(file_name, hash);
				send(socket_desc, hash, 33, 0);
			}
		}
		else if (strcmp(command, "MOVE_FILE") == 0)
		{
			recv(socket_desc, server_reply, _BUFFER_SIZE, 0);
			printf("%s", server_reply);
		}
		else
		{
			printf("[-]%s ", server_reply);
		}
	}

	close(socket_desc);

	return 0;
}

int download_file(int socket, char buffer[_BUFFER_SIZE], char file_name[_BUFFER_SIZE], int size)
{

	FILE *fp;
	fp = fopen(file_name, "wb");
	int confirmation = 0;

	ssize_t bytes_recibidos = recv(socket, buffer, _BUFFER_SIZE*sizeof(char), 0);
	if(fp == NULL){
		confirmation = -1;
	}
	while (bytes_recibidos > 0)
	{
		fwrite(buffer, 1, bytes_recibidos, fp);
		if(size > _BUFFER_SIZE){
			bytes_recibidos = recv(socket, buffer, _BUFFER_SIZE*sizeof(char), 0);
		}
		else {
			bytes_recibidos = -1;
		}
	}

	fclose(fp);

	return confirmation;
}

int send_file_size(char file_name[_BUFFER_SIZE], int socket)
{

	FILE *fp;
	fp = fopen(file_name, "r");
	char data[_BUFFER_SIZE] = {0};

	fgets(data, _BUFFER_SIZE, fp);

	send(socket, data, sizeof(data), 0);

	fclose(fp);

	return sizeof(data);
}

void send_file(char file_name[_BUFFER_SIZE], int socket)
{
	FILE *fp;
	fp = fopen(file_name, "rb");
	char data[1] = {0};

	fseek(fp, 0L, SEEK_END); 
	int length = ftell(fp);
	fclose(fp);

	fp = fopen(file_name, "rb");

	for(int i = 1; i <= length; i++){
		fread(data, sizeof(char), _BUFFER_SIZE, fp);
		if (send(socket, data, _BUFFER_SIZE*sizeof(char), 0) == -1)
		{	
			perror("Error sending file.");
			exit(1);
		}
	}
	fclose(fp);
	/*FILE *fp;
	fp = fopen(file_name, "rb");
	char data[_BUFFER_SIZE] = {0};

	while (fgets(data, _BUFFER_SIZE, fp) != NULL)
	{
		if (send(socket, data, sizeof(data), 0) == -1)
		{
			perror("Error sending file.");
			exit(1);
		}
		bzero(data, _BUFFER_SIZE);
	}*/
}

int recv_ACK(int socket)
{
	int received;
	char buff[_BUFFER_SIZE];

	recv(socket, buff, _BUFFER_SIZE, 0);

	if (strcmp(buff, "UPLOAD_ACK") == 0)
	{
		received = 1;
	}

	return received;
}

int md5Hash(char file_name[_BUFFER_SIZE], char hash[_BUFFER_SIZE])
{

	char command[_BUFFER_SIZE] = {"md5sum "};

	strcat(command, file_name);
	strcat(command, " > hash.txt");

	system(command);

	FILE *fp = fopen("hash.txt", "r");

	fgets(hash, 33, fp);
}