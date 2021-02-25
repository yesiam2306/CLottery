#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>

char* SESSION_ID = "NULL"; /variabile che viene sempre passata al server come inizio di ogni messaggio/


/-------FUNZIONI PER LA STAMPA DI PARAMETRI------------/

void stampa_int(int n){
	printf("%d\n",n);
}

void stampa_str(char* s){
	printf("%s\n",s);
}



/-------FUNZIONI CHE SVOLGONO LA ROUTINE DELLO SCAMBIO DEI MESSAGGI-------/

/*PARAMETRI:
-sd: socket descriptor del client a cui si vuole inviare un messaggio.
-buf: puntatore al buffer contenente il messaggio che si desidera inviare. Per ogni messaggio ne viene prima inviata la dimensione in un messaggio preliminare.
-SESSION_ID: stringa in cui e` memorizzato il SESSION_ID
*/
int invia_risposta(int sd, char buf, char SESSION_ID) {

	char* msg;
	int len = strlen(SESSION_ID) + strlen(buf) + 2;
	uint16_t dim = ntohs(len);
	int ret = send(sd, (void*)&dim, sizeof(uint16_t), 0);

	if (ret < 0){
		printf("Errore in fase di risposta al client"); 
		/exit(1);/
	}
	msg = (char*) malloc(len * sizeof(char));
	strcat(msg, SESSION_ID);
	strcat(msg, " ");
	strcat(msg, buf);
	ret = send(sd, (void*) msg, len, 0);
	if (ret < 0) {
		printf("Errore in fase di risposta al client"); 
		/exit(1);/
	}
	return ret;
}


/*PARAMETRI:
-sd: socket descriptor del client a cui si vuole inviare un messaggio.
-buf: puntatore al buffer in cui verra` memorizzato il messaggio che si riceve. Per ogni messaggio ne viene prima ricevuta la dimensione in un messaggio preliminare.
*/
int ricevi_dati(int sd_client, char* buf) {

	int len = strlen(buf)+1;
	uint16_t dim = ntohs(len);
	/si riceve la dimensione del messaggio contenente il comando/
	int ret = recv(sd_client, (void*)&dim, sizeof(uint16_t), 0);
	if (ret <= 0) {
		printf("Errore in fase di ricezione della dimensione\n");
		return ret;
	}
	
	/si riceve il comando/
	len = ntohs(dim);
	ret = recv(sd_client, (void*)buf, len, 0);
	if (ret <= 0){
		printf("Errore in fase di ricezione del messaggio\n");
		return ret;
	}
	return ret;
}



int invia_input(int sd_server, char* buf) {
	int ret;
	fgets(buf, 60, stdin);
	printf("5\n");
	ret = invia_risposta(sd_server, buf, SESSION_ID);
	return ret;
}

/---------------------------------------------------------------------------/

int main(int argc, char* argv[]) {
	
	int porta, timer;
	
	int ret, sd_server, len, i;
	struct sockaddr_in server_addr;
	char* indirizzo_server;
	char buffer[1024];	
	char* parametri[10];
	uint16_t dim;
	
	if(argc<2 || argc>3) {
		porta = 4242;
		indirizzo_server = "127.0.0.1";
	}
	else {
	 	inet_aton(argv[1], &server_addr.sin_addr);
		indirizzo_server = inet_ntoa(server_addr.sin_addr);
		porta = atoi(argv[2]);
	}

	sd_server = socket(AF_INET, SOCK_STREAM, 0);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(porta);
	inet_pton(AF_INET, indirizzo_server, &server_addr.sin_addr);

	ret = connect(sd_server, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(ret < 0)
		perror("Errore durante l'esecuzione della connect\n");
	else {
		printf("***********GIOCO DEL LOTTO***********\n");
		printf("Sono disponibili i seguenti comandi:\n\n");
		printf("1) !help <comando> --> mostra i dettagli di un comando\n");
		printf("2) !signup <username> <password> --> crea un nuovo utente\n");
		printf("3) !login <username> <password> --> autentica un utente\n");
		printf("4) !invia_giocata g --> invia una giocata g al server\n");
		printf("5) !vedi_giocate tipo --> visualizza le giocate precedenti dove tipo = {0,1} e permette di visualizzare le giocate passate ‘0’ oppure le giocate attive ‘1’ (ancora non estratte)\n");
		printf("6) !vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni sulla ruota specificata\n");
		printf("7) !vedi_vincite --> mostra un dettaglio delle vincite dell'utente\n");
		printf("8) !esci --> termina il client\n");
		printf("***************************\n\n");
		
				
		while(1){
			
			printf("\nInserisci un comando: \n");
			fgets(buffer, 1024, stdin);
			printf("\n");
			buffer[strlen(buffer)-1] = '\0';

			ret = invia_risposta(sd_server, buffer, SESSION_ID);
			if(ret<=0)stampa_str("Errore nell'invio del messaggio");
			while (1){
				char* token;

				/si attende la risposta/
				ret = ricevi_dati(sd_server, buffer);
				
				/ci sono 4 messaggi 'speciali' che possono essere ricevuti dal server/

		/wait_for_input fa iniziare una routine in cui il client si trova in fase di input/
				if(strcmp(buffer,"wait_for_input")==0) {
					fgets(buffer, 1024, stdin);
					printf("\n");
					buffer[strlen(buffer)-1] = '\0';

					ret = invia_risposta(sd_server, buffer, SESSION_ID);
					continue;
				}

		/next_command fa uscire da questo while per far tornare il client ad inserire un nuovo comando/
				if(strcmp(buffer,"next_command")==0) break;

		/exit fa chiudere connessione e programma lato client/
				if(strcmp(buffer,"exit")==0){
					close(sd_server);
					exit(1);
		  		}
		
		/SESSION_ID fa memorizzare il session_id nella variabile globale/
				token = strtok(buffer, "~");
				if(strcmp(token, "SESSION_ID") == 0){
					token = strtok(NULL, "~"); 
					SESSION_ID = (char*) malloc((strlen(token) + 1) * sizeof(char));
					strcpy(SESSION_ID, token);
					continue;
				}
					
				stampa_str(buffer);
			}
		}
		
	}
}
