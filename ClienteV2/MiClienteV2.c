/* RemoteClient.c
   Se introducen las primitivas necesarias para establecer una conexión simple
   dentro del lenguaje C utilizando sockets.
*/
/* Cabeceras de Sockets */
#include <sys/types.h>
#include <sys/socket.h>
/* Cabecera de direcciones por red */
#include <netdb.h>
/**********/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/*
  El archivo describe un sencillo cliente que se conecta al servidor establecido
  en el archivo RemoteServer.c. Se utiliza de la siguiente manera:
  $cliente IP port
 */


 int clienteFD = 0;

void *escribir() { ///Homologo a enviarMensage
  char buf[1024];
  while(1) {
    scanf(" %[^\n]", buf);
    if(!strcmp(buf, "/exit"))
      raise(SIGINT);
    send(clienteFD , buf, sizeof(buf), 0);
    bzero(buf,1024);
  }
}

int main (int argc, char **argv){
//Analizamos la cantidad de argumentos que hay
  if(argc != 3){
    printf("La cantidad de argumentos es erroneo, recuerde que toma Direccion y Puerto\n");
    return EXIT_FAILURE;
  }
//Guardo los argumentos
  char *ip = argv[1];
  int puerto = atoi(argv[2]);
//Creamos el socket de comunicacion
  struct sockaddr_in sv;
//Configuramos el Socket
  clienteFD = socket(AF_INET, SOCK_STREAM, 0);
  sv.sin_family = AF_INET;
  sv.sin_addr.s_addr = inet_addr(ip);
  sv.sin_port = htons(puerto);
// Intentamos alcanzar al server
  if (connect(clienteFD, (struct sockaddr *)&sv, sizeof(sv)) != 0) {
    printf("No se pudo conectar al servidor\n");
    close(clienteFD);
    return EXIT_FAILURE;

  }
//Creamos hilo para mandar mensajes. En el actual vamos a escuchar
  pthread_t send_msg_thread;
  if(pthread_create(&send_msg_thread, NULL, escribir, NULL) != 0) {
    printf("No se pudo crear el canal de escritura\n");
    return EXIT_FAILURE;
  }
  char mensaje[1024];
  int bandera = 0;
  while(bandera == 0){ //Aca se escucha
    recv(clienteFD, mensaje, sizeof(mensaje), 0);
    mensaje[1023] = '\0';
    if(strcmp(mensaje, "\033xit") == 0){
      bandera = 1;
      printf("Cerrando conexion del cliente...\n");
    } else {
      printf("%s\n", mensaje);
    }
bzero(mensaje,1024);
  }
 close(clienteFD);
 void* ret;
 pthread_join(send_msg_thread,&ret);
//Se cerraron todos los caminos
  printf("Cerrada la conexion full duplex. Ya no puede participar\n");
//Para que no quede mem leak de los pthread
  pthread_exit(0);
  return EXIT_SUCCESS;
}

