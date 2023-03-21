#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 1024
#define NOM_MAX 40

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

//Cliente
typedef struct {
  struct sockaddr_in address;
  int sockfd;
  int uid;
  char name[NOM_MAX];
} client_t;

client_t *clients[MAX_CLIENTS];

//Lock para los clientes, porque varios clientes(hilos) van a querer acceder a la cola de clientes, ya sea para cambiarse el nombre
//que requiere un control ya que se puede solapar, el mandar mensajes para que ocurra en orden, etc
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; //Inicializo el mutex

//Como los saltos de carro para mostrar el mensaje en la terminal del sv
void como_salto_de_carro(char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) {
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

//Añado una estructura cliente a la cola. **INTENTAR SACAR LOCK
void anadir_cliente_cola(client_t *cl) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (!clients[i]) {
      clients[i] = cl;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

//Sacamos un cliente de la cola mediante su identificador
void sacar_cliente_cola(int uid) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i]) {
      if (clients[i]->uid == uid) {
        clients[i] = NULL;
        break;
      }
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

//Mando mensaje privado punto a punto, aplicacion de /msg
void mensaje_privado_p2p(char *msg, char* nickDestino) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i<MAX_CLIENTS; ++i) {
    if (clients[i]) {
      if (strcmp(clients[i]->name, nickDestino) == 0) {
        if (write(clients[i]->sockfd, msg, strlen(msg)) < 0) {
          perror("No se pudo mandar msg privado a destino\n");
          break;
        }
      }
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

//Difusion de un mensaje a todos los clientes en pantalla, broadcast
void broadcast(char *s, int uid) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i<MAX_CLIENTS; ++i) {
    if (clients[i]) {
      if (clients[i]->uid != uid) {
        if (write(clients[i]->sockfd, s, strlen(s)) < 0) {
        perror("No se pudo difundir el mensaje\n");
        break;
        }
      }
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

//Estructura para parsear los mensajes entrantes al sv, para después darles un correcto proceso
typedef struct {
	int comando;
	char nickname[NOM_MAX];
	char mensaje [BUFFER_SZ];
} parseoResultado;
//Si comando es :
// 0 -> normal mensaje
// 1 -> exit
// 2 -> mensaje privado p2p
// 3 -> nickname cambio swap

parseoResultado* parseoTotal2 (char* mensaje){
    parseoResultado* parseo = (parseoResultado*) malloc(sizeof(parseoResultado));
    char nombre[NOM_MAX];
    char mensajePrivado[BUFFER_SZ];
    if(strcmp(mensaje, "/exit") == 0){
        parseo->comando = 1;
        return parseo;
    }
    if(sscanf(mensaje,"/nickname %s", nombre) == 1){
        strcpy(parseo->nickname, nombre);
        parseo->comando = 3;
        return parseo;
    }
    if(sscanf(mensaje,"/msg %s %s",nombre,mensajePrivado) == 2){
        parseo->comando = 2;
        strcpy(parseo->mensaje, mensajePrivado);
        strcpy(parseo->nickname, nombre);
        return parseo;
    }
    parseo->comando = 0;
    return parseo;

}

//Verifico si un nombre es repetido en la cola de clientes o no, con mutex
int controlarNick( char* nuevoNick){
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i<MAX_CLIENTS; ++i) {
    if (clients[i]) {
      if (strcmp(clients[i]->name,nuevoNick) == 0) {
        pthread_mutex_unlock(&clients_mutex);
        return 1;
      }
    }
  }
  pthread_mutex_unlock(&clients_mutex);
  return 0;
}

void mandar_cadenas_sv(char *s, int uid) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i<MAX_CLIENTS; ++i) {
    if (clients[i]) {
      if (clients[i]->uid == uid) {
        if (write(clients[i]->sockfd, s, strlen(s)) < 0) {
          perror("No se puede mandar la cadena de sv\n");
          break;
        }
      }
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

//Funcion rutina de cada hilo( cliente), en ella primero orientamos al mismo a que nos introduzca un nombre y luego estamos a la espera
// de sus peticiones o mensajes
void *rutina_cliente(void *arg) {
  char peticion[BUFFER_SZ];
  int vidaHilo = 0;
//Traemos la informacion del cliente con arg
  cli_count++;
  client_t *cli = (client_t *)arg;
//Resolvemos el ingreso del nombre del cliente al sv
  char str[112],str2[150];
  sprintf(str,"Misterioso %d",cli->uid);
  strcpy(cli->name,str);
  sprintf(str2,"Bienvenido %s a la sala",str);
  broadcast(str2,cli->uid);
  mandar_cadenas_sv(str2,cli->uid);

//Inicia Ciclo de vida del cliente del lado del sv
  while (vidaHilo == 0) {
//Estamos a la espera de un mensaje de algún
    int receive = recv(cli->sockfd, peticion, sizeof(peticion), 0);
    char cabeceraSola[NOM_MAX + 2];
    strcpy(cabeceraSola, cli->name);
    strcat(cabeceraSola, ": ");
//Parseo el mensaje
    if ( receive  > 0){
      parseoResultado* resultado = parseoTotal2(peticion);
//Catalogo el mensaje
      switch (resultado->comando){
        case 0:
        //MENSAKE NORMAL ANDA FINAL
         printf("Mensaje Normal\n");
          char msg[BUFFER_SZ + NOM_MAX + 1];
          sprintf(msg, "%s: %s", cli->name, peticion);
          broadcast(msg, cli->uid);
          break;
        case 1:
        //SALIR ANDA
          sprintf(peticion, "%s se fue del chat\n", cli->name);
          printf("%s", peticion);
          broadcast(peticion, cli->uid);
          char* exitConfirmacion = "/exit";
          mandar_cadenas_sv(exitConfirmacion, cli->uid);
          vidaHilo = 1;
          break;
        case 2:
        //MENSAJE P2P ANDA FINAL
          printf("MENSAJE P2P\n");
          printf("TO: -%s-, MSG: -%s-\n",resultado->nickname,resultado->mensaje);
          strcat(cabeceraSola,resultado->mensaje);
          strcat(cabeceraSola, "[Private]");
          mensaje_privado_p2p(cabeceraSola,resultado->nickname);
          como_salto_de_carro(peticion, strlen(peticion));
          break;
        case 3:
        //CAMBIAR NICK ANDA
          printf("CAMBIAR NICK\n");
          printf("NUEVO NICK: -%s-\n", resultado->nickname);
          int nn = controlarNick(resultado->nickname);
          if (nn == 0){
            char nickConfirmacion[NOM_MAX + 24 ];
            sprintf(nickConfirmacion,  "Su nombre fue aceptado, %s", resultado->nickname);
            mandar_cadenas_sv(nickConfirmacion, cli->uid);
            strcpy(cli->name,resultado->nickname);
          }
          else{
            char* nickNegacion = "Su nombre no fue aceptado.";
            mandar_cadenas_sv(nickNegacion, cli->uid);
          }
          break;
      }
      free(resultado);
    }
    else if (receive == 0) {
      sprintf(peticion, "%s se fue del chat\n", cli->name);
      printf("%s", peticion);
      broadcast(peticion, cli->uid);
      vidaHilo = 1;
      }
      else {
        printf("ERROR: -1\n");
        vidaHilo = 1;
      }
    bzero(peticion, BUFFER_SZ);
  }
//Sacamos al cliente de la cola del servidor, lo eliminamos y cerramos su socket
  close(cli->sockfd);
  sacar_cliente_cola(cli->uid);
  free(cli);
  cli_count--;
  pthread_detach(pthread_self());
  return NULL;
}

//DE ACA PARA ABAJO DEJO
int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Cantidad de argumentos inválidos\n");
    return EXIT_FAILURE;
  }
  char *ip = "127.0.0.1";
  int puerto = atoi(argv[1]);
  int option = 1;
  int servidorFD = 0, clienteFD = 0;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;
  pthread_t tid;
//Configuramos el socker, le asignamos una dir
  servidorFD = socket(AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ip);
  serv_addr.sin_port = htons(puerto);
//Cuando un cliente se intenta conectar al server y este está abajo, eso nos devuelve una SIGPIPE, que usualmente termina con el
//programa para que esto no pase las ignoramos
  signal(SIGPIPE, SIG_IGN);
//Configuramos el socket para que este pueda reutilizar los puertos y direcciones
  if (setsockopt(servidorFD, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option)) < 0) {
    perror("No se pudo configurar el socket\n");
    return EXIT_FAILURE;
  }
//Le asignamos un FD al socket del sv
  if (bind(servidorFD, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("No se pudo asignar el FD al socket");
    return EXIT_FAILURE;
  }
//Ponemos al socket en modo pasivo, para que esté pendiente de nuevas conexiones con una cola máxima de 10 clientes en espera
  if (listen(servidorFD, 10) < 0) {
    perror("No se puede esperar las conexiones\n");
    return EXIT_FAILURE;
  }
  printf("PANEL DEL SV.... CHAT INICIADO\n");
//Hilo de vida del sv
  while (1) {
//Como accept es bloqueante siempre vamos a estar esperando una conexion. Cuando hay un conexion creamos un hilo para esa conexion con
//una rutina, si no hay a la espera conexiones no se hace nada
    socklen_t clilen = sizeof(cli_addr);
    clienteFD = accept(servidorFD, (struct sockaddr*)&cli_addr, &clilen);
//Limite de 100 en la sala
    if ((cli_count + 1) == MAX_CLIENTS) {
      printf("Limite de sala de char alcanzado, fuiste rechazado :/\n");
      close(clienteFD);
      continue;
    }
//Configuramos la estructura cliente
    client_t *cli = (client_t *)malloc(sizeof(client_t));
    cli->address = cli_addr;
    cli->sockfd = clienteFD;
    cli->uid = uid++;
//Añadimos el cliente a la cola y le asignamos un hilo
    anadir_cliente_cola(cli);
    pthread_create(&tid, NULL, &rutina_cliente, (void*)cli);
    sleep(1);
  }
  return EXIT_SUCCESS;
}
