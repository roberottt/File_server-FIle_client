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

void download_file(int socket, char buffer[_BUFFER_SIZE], char file_name[_BUFFER_SIZE], int size);
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

	char *address = "192.168.1.104";
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
	if (argc > 2)
		message = argv[2]; // Si existe, el segundo argumento es el mensaje

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

		if (strcmp(command, "DOWNLOAD_FILE") == 0) // Comprobacion del comando DOWNLOAD_FILE
		{
			printf("DOWNLOAD SIZE: %s\n", server_reply);

			int size;
			sscanf(server_reply, "%d", &size);

			send(socket_desc, "DOWNLOAD_ACK", _BUFFER_SIZE, 0);

			download_file(socket_desc, server_reply, file_name, size);

			char currenthash[32];

			recv(socket_desc, server_reply, 32, 0);

			md5Hash(file_name, currenthash);

			if (strncmp(currenthash, server_reply, 32) == 0)
			{
				printf("DOWNLOAD WITHOUT ERRORS\n");
			}
			else
			{
				printf("DOWNLOAD WITH ERRORS\n");
			}

			printf("[+]Download complete.\n");
		}
		else if (strcmp(command, "DELETE_FILE") == 0) // Comprobacion del comando DELETE_FILE
		{
			recv(socket_desc, server_reply, _BUFFER_SIZE, 0);
			printf("%s", server_reply);
		}
		else if (strcmp(command, "UPLOAD_FILE") == 0) // Comprobacion del comando UPLOAD_FILE
		{

			send_file_size(file_name, socket_desc);

			if (recv(socket_desc, server_reply, _BUFFER_SIZE, 0) > 0)
			{
				send_file(file_name, socket_desc);

				if (recv(socket_desc, server_reply, _BUFFER_SIZE, 0) > 0)
				{
					printf("%s", server_reply);
				}

				char hash[32];
				md5Hash(file_name, hash);
				send(socket_desc, hash, 32, 0);
			}
		}
		else if (strcmp(command, "MOVE_FILE") == 0) // Comprobacion del comando MOVE_FILE
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

void download_file(int socket, char buffer[_BUFFER_SIZE], char file_name[_BUFFER_SIZE], int size) // Abrimos el fichero y recibimos caracter a caracter
{

	FILE *fp;
	fp = fopen(strcat(file_name, "_"), "w");
	char caracter[1];
	int cont = 0;

	while (cont < size)
	{
		recv(socket, caracter, 1, 0);
		fwrite(caracter, sizeof(char), 1, fp);
		cont++;
	}
	fclose(fp);

}

int send_file_size(char file_name[_BUFFER_SIZE], int socket) // Abrimos el fichero, desplazamos el puntero al final del fichero, guardamos el tamaño y lo enviamos
{

	FILE *fp;
	fp = fopen(file_name, "r");
	
	fseek(fp, 0L, SEEK_END); 
	int length = ftell(fp);
	
	// Para pasar el int a char
	char size[length];
	sprintf(size, "%d", length);

	send(socket, size, length, 0);

	return length;
}

void send_file(char file_name[_BUFFER_SIZE], int socket) // Abrimos el fichero y enviamos el archivo
{
	FILE *fp;
	fp = fopen(file_name, "r");
	char data[_BUFFER_SIZE] = {0};

	while (fgets(data, _BUFFER_SIZE, fp) != NULL)
	{
		if (send(socket, data, sizeof(data), 0) == -1)
		{
			perror("Error sending file.");
			exit(1);
		}
		
	}
}

int recv_ACK(int socket) // Comprobamos el ACK recibido
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

int md5Hash(char file_name[_BUFFER_SIZE], char hash[_BUFFER_SIZE]) // Hacemos el hash 
{

	char command[_BUFFER_SIZE] = {"md5sum "};

	strcat(command, file_name);
	strcat(command, " >> hash.txt");

	system(command);

	FILE *fp = fopen("hash.txt", "r");

	fgets(hash, 32, fp);
}