#include <stdlib.h>
#include <stdio.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>	   // Close sockets
#include <sys/types.h>
#include <dirent.h>

#define _BUFFER_SIZE 2000
#define ECHO_PORT 5675

int list_files(char buff[_BUFFER_SIZE]);
int search_file(char buff[_BUFFER_SIZE], int socket, char command[_BUFFER_SIZE]);
void send_file(char file_name[_BUFFER_SIZE], int socket);
int send_file_size(char file_name[_BUFFER_SIZE], int socket);
int write_file(int socket, char file_name[_BUFFER_SIZE], char buffer[_BUFFER_SIZE]);
int delete_file(char file_name[_BUFFER_SIZE]);
int recv_ACK(int socket);
void send_ACK(int socket);
void error(const char *s);
int md5Hash(char file_name[_BUFFER_SIZE], char hash[_BUFFER_SIZE]);

int main(int argc, char *argv[])
{
	int socket_desc, accepted_socket;
	struct sockaddr_in server_addr; // Direccion del servidor
	char buffer[_BUFFER_SIZE];

	printf("Initializing file server\n");

	//Create socket
	socket_desc = socket(AF_INET, SOCK_STREAM, 0); // Sock stream --> SOCKET TCP
	if (socket_desc < 0)
	{
		printf("Could not create socket");
		return 0;
	}

	printf("Socket created");

	char *address = "0.0.0.0"; // Accept connections from the whole Internet
	if (argc > 1)
		address = argv[1];
	server_addr.sin_addr.s_addr = inet_addr(address);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(ECHO_PORT);

	bind(socket_desc, (struct sockaddr *)&server_addr, sizeof(server_addr));

	listen(socket_desc, 10);

	int exit = 0;

	while (!exit)
	{
		printf("File server. Waiting for incoming connections from clients.\n");
		accepted_socket = accept(socket_desc, (struct sockaddr *)NULL, NULL);
		printf("Accepted a connection from client\n");

		// First wait for the message of the client
		ssize_t len = recv(accepted_socket, buffer, _BUFFER_SIZE, 0);
		if (len < 0)
		{
			printf("recv failed\n");
		}
		else
		{
			printf("Received data from client: %s\n", buffer); // TODO: If the message is too long --> may print garbage

			char command[_BUFFER_SIZE], file_name[_BUFFER_SIZE], file_rename[_BUFFER_SIZE];
			sscanf(buffer, "%s %s %s", command, file_name, file_rename);

			if (strcmp(buffer, "CLOSE") == 0)
			{
				printf("Received CLOSE message --> stopping server\n");
				exit = 1;
			}
			else if (strcmp(buffer, "LIST_FILES") == 0) // Comprobacion del comando LIST_FILES
			{
				char buff[_BUFFER_SIZE];
				len = list_files(buff);

				if (send(accepted_socket, buff, len, 0) < 0) // Envia contenido del directorio
				{
					printf("Send failed\n"); // Si no se envia muestra el error
					return 1;
				}
				printf("Sent: %s\n", buff);
			}
			else if (strcmp(command, "DOWNLOAD_FILE") == 0) // Comprobacion del comando DOWNLOAD_FILE
			{
				int found = search_file(file_name, accepted_socket, command); // Buscamos si existe el archivo

				if (found == 1)
				{
					send_file_size(file_name, accepted_socket); // Si se encuentra, se envia el tamaño
					
					if(recv(accepted_socket, buffer, _BUFFER_SIZE, 0) > 0){
						if(strcmp(buffer, "DOWNLOAD_ACK") == 0){ // Si se recibe ACK
							send_file(file_name, accepted_socket); // Envia el archivo
							char hash[32];

							md5Hash(file_name, hash);
							send(accepted_socket, hash, 32, 0); // Se envia el hash
						}
						else{
							send(accepted_socket, "ERROR", _BUFFER_SIZE, 0); // Si no se recibe el ACK envia error
						}
					}
				}
				
			}
			else if (strcmp(command, "UPLOAD_FILE") == 0) // Comprobacion del comando UPLOAD_FILE
			{
				send_ACK(accepted_socket); // Enviamos ACK si tamaño recibido

				if(recv(accepted_socket, buffer, _BUFFER_SIZE, 0) > 0){
					send_ACK(accepted_socket); // Enviamos ACK si archivo recibido
					if(recv(accepted_socket, buffer, _BUFFER_SIZE, 0) > 0){
						int w = write_file(accepted_socket, file_name, buffer); // Si se recibe archivo, se escribe
						
						if(w != -1){
							send(accepted_socket, "SUCCES", _BUFFER_SIZE, 0); // Si se escribio correctamente, envia SUCCESS
						}
						else{
							send(accepted_socket, "ERROR", _BUFFER_SIZE, 0); // Si no, envia ERROR
						}

						char currenthash[32]; // Recibimos hash del archivo original

						recv(accepted_socket, buffer, 32, 0);

						md5Hash(file_name, currenthash); // Creamos hash del archivo escrito
				
						if(strncmp(currenthash, buffer, 31) == 0){ // Comparamos ambos hashes
							printf("DOWNLOAD WITHOUT ERRORS\n");
						}
						else{
							printf("DOWNLOAD WITH ERRORS\n");
						}

					}
				}
			}
			else if (strcmp(command, "DELETE_FILE") == 0) // Comprobacion del comando DELETE_FILE
			{
				int found = search_file(file_name, accepted_socket, command); // Buscamos el archivo

				if (found == 1)
				{
					int rm = delete_file(file_name); // Si se encuentra, lo intenta borrar

					if(rm != -1){
						send(accepted_socket, "SUCCESS", _BUFFER_SIZE, 0); // Si se borro correctamente, se envia SUCCESS
					}
					else{
						send(accepted_socket, "ERROR", _BUFFER_SIZE, 0); // Si no ERROR			 							
					}
				}
				else{
					send(accepted_socket, "ERROR", _BUFFER_SIZE, 0); // Si no se encuentra, envia ERROR
				}
			}
			else if (strcmp(command, "MOVE_FILE") == 0) // Comprobacion del comando MOVE_FILE
			{
				if (search_file(file_name, accepted_socket, file_rename) == 1) // Busca el archivo
				{
					rename(file_name, file_rename); // Si encuentra el archivo, lo renombra
					send(accepted_socket, "SUCCES", _BUFFER_SIZE, 0);
				}
				else{
					send(accepted_socket, "ERROR", _BUFFER_SIZE, 0);
				}
			}
			else // Si el comando no ha sido leido antes, no es valido
			{
				char error[_BUFFER_SIZE] = "INVALID COMMAND";

				if (send(accepted_socket, error, _BUFFER_SIZE, 0) < 0)
				{
					printf("Send failed\n");
					return 1;
				}
				printf("Sent: %s\n", error);
			}
		}

		close(accepted_socket);
		printf("Accepted connection closed.\n");
		sleep(1);
	}

	printf("Closing binded socket\n");
	close(socket_desc);

	return 0;
}

int list_files(char buff[_BUFFER_SIZE]) // Abrimos directorio, leemos el directorio y guardamos en un buffer
{
	int caracteres = 0;

	DIR *dir;

	struct dirent *entry;

	dir = opendir(".");

	if (dir == NULL)
	{
		error("Couldn't open directory");
	}

	while ((entry = readdir(dir)) != NULL)
	{

		if (strcmp(entry->d_name, ".") != 0)
		{
			caracteres += sprintf(buff + caracteres, "%s\n", entry->d_name);
		}
	}
	closedir(dir);

	return caracteres;
}
void error(const char *s)
{
	perror(s);
	exit(EXIT_FAILURE);
}

int search_file(char buff[_BUFFER_SIZE], int socket, char command[_BUFFER_SIZE]) // Abrimos el directorio y buscamos el nombre del archivo
{
	int found = 0;

	DIR *dir;

	struct dirent *entry;

	dir = opendir(".");

	if (dir == NULL)
		error("Couldn't open directory");

	while ((entry = readdir(dir)) != NULL && found != 1)
	{

		if ((strcmp(entry->d_name, ".") != 0))
		{
			if (strcmp(buff, entry->d_name) == 0)
			{
				found = 1;
			}
		}
	}
	closedir(dir);

	return found;
}

void send_file(char file_name[_BUFFER_SIZE], int socket) // Abrimos el fichero, leemos el contenido y lo enviamos por partes.
{
	FILE *fp;
	fp = fopen(file_name, "r");
	char data[1] = {0};

	fseek(fp, 0L, SEEK_END); 
	int length = ftell(fp);
	fclose(fp);

	fp = fopen(file_name, "r");

	for(int i = 1; i <= length; i++){
		fread(data, sizeof(char), 1, fp);
		if (send(socket, data, 1, 0) == -1)
		{	
			perror("Error sending file.");
			exit(1);
		}
	}
	fclose(fp);
}

int send_file_size(char file_name[_BUFFER_SIZE], int socket) // Abrimos el fichero, desplazamos el puntero al final y devolvemos longitud
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

int recv_ACK(int socket) // Comprueba si el ACK recibido es correcto
{
	int received;
	char buff[_BUFFER_SIZE];

	recv(socket, buff, _BUFFER_SIZE, 0);

	if (strcmp(buff, "DOWNLOAD_ACK") == 0)
	{
		received = 1;
	}

	return received;
}

void send_ACK(int socket) // Envia ACK
{
	send(socket, "UPLOAD_ACK", _BUFFER_SIZE, 0);
}

int delete_file(char file_name[_BUFFER_SIZE]) // Borra el archivo
{
	int rm = remove(file_name);
	printf("%s has been removed\n", file_name);
	return rm;
}

int write_file(int socket, char file_name[_BUFFER_SIZE], char buffer[_BUFFER_SIZE]) // Creamos un archivo con el nombre original y copiamos el contenido del original
{
	int w;
	FILE *fp;

	fp = fopen(strcat(file_name, "_"), "w");
	if(fp != NULL){
		fprintf(fp, "%s", buffer);
		w = 1;
	}
	

	fclose(fp);
}

int md5Hash(char file_name[_BUFFER_SIZE], char hash[_BUFFER_SIZE]){ // Hacemos el hash del contenido del fichero

	char command[_BUFFER_SIZE] = {"md5sum "};

	strcat(command, file_name);
	strcat(command, " > hash_.txt");
	
	system(command);

	FILE *fp = fopen("hash.txt", "r");

	fgets(hash, 32, fp);

}