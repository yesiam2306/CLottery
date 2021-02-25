#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>

#define MAX_LISTENED 	10
#define NUMERI 		90
#define RUOTE 		11

char buffer[1024];
int ultima_estrazione = 0;
char* array_ruote[RUOTE] = {"Bari", "Cagliari", "Firenze", "Genova", "Milano", "Napoli", "Palermo", "Roma", "Torino", "Venezia", "Nazionale"}; 

typedef struct {
	int sd;
	char* session_id;
	char* username;
} sessione; /usata per tenere uniti in un'unica struttura il socket descriptor di un client, l'username usato dall'utente e l'id della sessione in corso./

typedef struct {
	int tipo;
	char* ruote[RUOTE];
	int numeri[10];
	double quote[5];
	char* utente;
	time_t curtime;
} giocata;

sessione array_sessioni[MAX_LISTENED];
int quante_sessioni = 0;



/-------FUNZIONI PER LA STAMPA DI PARAMETRI------------/

void stampa_int(int n){
	printf("%d\n",n);
}

void stampa_str(char* s){
	printf("%s\n",s);
}

void stampa_double(double n){
	printf("%.2f\n",n);
}

/----------------------------------------------------/


/-----FUNZIONI PER IL CALCOLO DELLE COMBINAZIONI-----/
/necessario nel calcolo delle vincite nella funzione controlla_vincite/

int fact(int n){
	int i;
	int res = 1;
	if (n == 0 || n == 1) return 1;	
	for(i=n; i>1; i--)
		res *= i;
	return res;
}

int combinazioni(int n, int k){
	return fact(n) / (fact(n - k) * fact(k));
}


/-----FUNZIONI DI CONTROLLO DEL FORMATO----------/

bool controlla_int(char* str) {

	int i;
	for (i = 0; i<strlen(str); i++)
		if (!isdigit(str[i])) return false;
	return true;
}

bool controlla_double(char* str) {
	char *foo;

	double d = strtod(str, &foo);
	if (foo == str) {
		return false;
	}
	else if (foo[strspn(foo, " \t\r\n")] != '\0') {
		return false;
	}
	else {
		return true;
	}
}



/*FUNZIONE CHE GENERA UNA STRINGA CASUALE ALFANUMERICA
utilizzata per generare il SESSION_ID*/

char* random_str(){   
	int i; 
	struct timeval time;
	gettimeofday(&time, NULL);
	srand((time.tv_sec * 100) + (time.tv_usec / 100));
	
	char* alpha = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	size_t alpha_len = strlen(alpha);        
	char* ret = NULL;
	int length = 10;

	ret = malloc(sizeof(char) * (length +1));
	if (ret) {
		for (i = 0; i < length; i++) {          		
			int key = rand() % alpha_len;    
			ret[i] = alpha[key];
		}
		ret[length] = '\0';
	}
	else {
		printf("Memoria insufficiente\n");
		exit(1);
	}
	return ret;
}



/FUNZIONE UTILIZZATA PER CONTROLLARE SE UN UTENTE HA GIA` UNA SESSIONE APERTA/

bool in_session(char* str) {
	int i;
	if(strcmp(str, "NULL")==0)
		return true;

	for (i = 0; i<MAX_LISTENED; i++){
		if(array_sessioni[i].session_id && strcmp(array_sessioni[i].session_id, str)==0)
			return true;
	}

	return false;
}



/-------FUNZIONI CHE SVOLGONO LA ROUTINE DELLO SCAMBIO DEI MESSAGGI-------/
/in piu` la funzione esci() utilizzata sia per effettuare il logout, sia per far uscire forzatamente un client./


/*PARAMETRI:
-sd: socket descriptor del client a cui si vuole inviare un messaggio.
-buf: puntatore al buffer contenente il messaggio che si desidera inviare. Per ogni messaggio ne viene prima inviata la dimensione in un messaggio preliminare.
*/
int invia_risposta(int sd, char *buf) {

	int len = strlen(buf)+1;
	uint16_t dim = ntohs(len);
	int ret = send(sd, (void*)&dim, sizeof(uint16_t), 0);
	if (ret < 0){
		printf("Errore in fase di risposta al client"); 
		/exit(1);/
	}
	ret = send(sd, (void*) buf, len, 0);
	if (ret < 0) {
		printf("Errore in fase di risposta al client"); 
		/exit(1);/
	}
	return ret;
}

/*PARAMETRI:
-sd_client: socket descriptor del client in questione.
-SESSION_ID: utilizzato per individuare e liberare, all'interno dell'array_sessioni, la sessione occupata dal client.
*/
void esci(int sd_client, char* SESSION_ID){
	
	int i;
	char* risposta = "Arrivederci.";
	int ret = invia_risposta(sd_client, risposta);
	risposta = "exit";
	ret = invia_risposta(sd_client, risposta);
	close(sd_client);

	for(i=0; i<MAX_LISTENED; i++) {

		if(SESSION_ID && strcmp(SESSION_ID, array_sessioni[i].session_id)){
			free(array_sessioni[i].session_id);
			free(array_sessioni[i].username);
			array_sessioni[i].sd = 0;
		}
	}

	if(quante_sessioni > 0)
		quante_sessioni--;

	exit(1);
	
}


/*PARAMETRI:
-sd: socket descriptor del client da cui si vuole ricevere un messaggio.
-buf: puntatore al buffer in cui memorizzare il messaggio che si desidera ricevere. Per ogni messaggio ne viene prima ricevuta la dimensione in un messaggio preliminare.
*/
int ricevi_dati(int sd_client, char* buf) {
	
	int len = strlen(buf)+1;
	uint16_t dim = ntohs(len);
	char* token; /variabile usata per dividere il messaggio ricevuto in singole parole./
	char* new_buf; /buffer usato fondamentalmente per poter escludere dal messaggio ricevuto,il SESSION_ID/

	/si riceve la dimensione del messaggio contenente il comando/
	int ret = recv(sd_client, (void*)&dim, sizeof(uint16_t), 0);
	if (ret <= 0) {
		printf("Errore in fase di ricezione della dimensione\n");
		return ret;
	}
	printf("Ricevuto messaggio dal client %d : %d\n", sd_client, dim);
	
	/si riceve il comando/
	len = ntohs(dim);
	ret = recv(sd_client, (void*)buf, len, 0);
	if (ret <= 0){
		printf("Errore in fase di ricezione del messaggio\n");
		return ret;
	}
	printf("Ricevuto messaggio dal client %d : %s\n", sd_client, buf);

	token = strtok(buf, " ");
	if(!in_session(token)) {
		invia_risposta(sd_client, "ERRORE DI SICUREZZA: Non risulti autenticato");
		esci(sd_client, NULL);
	}

	new_buf = (char*) malloc((len - strlen(token) -1) * sizeof(char));
	strcpy(new_buf, "");
	
	token = strtok(NULL, " ");

	while(token != NULL){
		strcat(new_buf, token);
		strcat(new_buf, " ");
		token = strtok(NULL, " ");
	}

	strcpy(buf, new_buf);

	return ret;
}



/*la seguente funzione viene utilizzata quando il client necessita di ridigitare un input ad esempio a causa di un errore di autenticazione e il serve deve dunque porsi in attesa. 
PARAMETRI: 
-sd_client: descriptor necessario per poter utilizzare la invia_risposta e la ricevi_dati
-argc: intero che indica quanti parametri ci si aspetta di ricevere in input
-argv: vettore di stringhe in cui vengono memorizzati i parametri
*/
void wait_for_input(int sd_client, int argc, char** argv){
	
	int i = 0;
	char* risposta = "wait_for_input"; /viene comunicato al client che il server e` pronto a ricevere/
	int ret = invia_risposta(sd_client, risposta);

	while (1) {
		ret = ricevi_dati(sd_client, buffer);
		if (ret>0) {
			buffer[strlen(buffer)-1] = '\0'; /*tolgo \n	*/		
			
			char* token = strtok(buffer, " ");
			for(i=0; token != NULL && i < argc; i++) {
				argv[i] = token;
				token = strtok(NULL, " ");
			}

/se i<argc vuol dire che non sono stati ricevuti tutti i parametri, viceversa se token!=NULL e` segno che ne sono stati ricevuti piu` del dovuto./
			if(i<argc || token) {
				risposta = "Errore nel numero di parametri.";
				invia_risposta(sd_client, risposta);
				risposta = "Riprova:";
				invia_risposta(sd_client, risposta);
				wait_for_input(sd_client, argc, argv);
				return;
			}
			return;
		}
	}
}

/-------------------------------------------------------------------------------/



void help(int sd_client, char* comando){
	char* risposta;
	if(!comando) {
		risposta = "***********GIOCO DEL LOTTO**********\nSono disponibili i seguenti comandi:\n\n1) !help <comando> --> mostra i dettagli di un comando\n2) !signup <username> <password> --> crea un nuovo utente\n3) !login <username> <password> --> autentica un utente\n4) !invia_giocata g --> invia una giocata g al server\n5) !vedi_giocate tipo --> visualizza le giocate precedenti dove tipo = {0,1} e permette di visualizzare le giocate passate ‘0’ oppure le giocate attive ‘1’ (ancora non estratte)\n6) !vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni sulla ruota specificata\n7) !vedi_vincite --> mostra un dettaglio delle vincite dell'utente\n8) !esci --> termina il client\n**************************\n";
		invia_risposta(sd_client, risposta);
		risposta = "!help mostra i dettagli del comando specificato come parametro. Se il comando non è specificato, !help restituisce una breve descrizione di tutti i comandi.\nVanno passati esclusivamente nomi di comandi, preceduti da un ! e senza aggiungere parametri.\n\nEsempio:\n!help !signup\n";
		invia_risposta(sd_client, risposta);
	}
	else if(strcmp(comando, "!signup")==0){
		risposta = "!signup <username> <password> - registra un nuovo utente caratterizzato da username e password ricevuti come parametri. Se è già presente un altro utente con lo stesso username, verrà generato un errore di conflitto e la procedura dovrà essere ripetuta dall'utente con un nuovo username. <username> e <password> devono essere stringhe alfanumeriche, non contenenti spazi, di lunghezza massima pari a 25 caratteri.\n\nEsempio:\n!signup u32uten30tea9ds pas49sdw341o450rfd9\n";	
		invia_risposta(sd_client, risposta);
	}
	else if (strcmp(comando, "!help")==0){
		risposta = "!help mostra i dettagli del comando specificato come parametro. Se il comando non è specificato, !help restituisce una breve descrizione di tutti i comandi.\nVanno passati esclusivamente nomi di comandi, preceduti da un ! e senza aggiungere parametri.\n\nEsempio:\n!help !signup\n";
		invia_risposta(sd_client, risposta);
	}
	else if (strcmp(comando, "!login")==0){
		risposta = "!login <username> <password> - invia al server le credenziali di autenticazione. <username> e <password> devono essere stringhe alfanumeriche di lunghezza massima pari a 25 caratteri. Nel caso in cui fossero inviate credenziali non valide, ad esempio l'<username> inserito non corrispondesse ad alcun utente registrato, o la password inserita fosse errata, si genera un errore di autenticazione. Dopo 3 errori di autenticazione verra' bloccato l'accesso all'utente.\n\nEsempio:\n!login u32uten30tea9ds pas49sdw341o450rfd9\n";
		invia_risposta(sd_client, risposta);
	}
	else if (strcmp(comando, "!invia_giocata")==0){
		risposta ="!invia_giocata <schedina> - invia al server una giocata. La giocata è descritta dalla schedina, la quale contiene le ruote scelte, i numeri giocati e gli importi per ogni tipologia di giocata.La schedina è formata da: elenco ruote, numeri giocati, importi per tipo giocata. \nLa schedina si specifica come segue: -r [ruote] -n [numeri] -i [importi] \n\n\t[ruote] indica l'insieme di ruote sulle quali si vuole scommettere, tra le possibili {Bari, Cagliari, Firenze, Genova, Milano, Napoli, Palermo, Roma, Torino, Venezia, Nazionale, Tutte}. Tutte è utilizzato per scommettere su tutte le ruote.\n\t[numeri] indica l'insieme di numeri che si vuole giocare. Bisogna specificare da 1 a 6 numeri tra 1 e 90.\n\t[importi] indica l'insieme di importi selezionati per ogni tipologia. Le tipologie, sempre presenti e calcolate, sono, in ordine: estratto, ambo, terno, quaterna, cinquina. Devono essere numeri con al massimo 2 cifre dopo il punto. Inserire uno 0 se non si vuole puntare su una determinata tipologia.\n\n";
		invia_risposta(sd_client, risposta);
		risposta = "Esempio nel quale vengono giocati i numeri 15, 19 e 33 sulle ruote di Milano e Roma, puntando 10 € sul terno e 5 € sull’ambo:\n!invia_giocata –r Roma Milano –n 15 19 33 –i 0 5 10\n\n";
		invia_risposta(sd_client, risposta);
	}
	else if (strcmp(comando, "!vedi_giocate")==0){
		risposta ="!vedi_giocate <tipo> - richiede al server le giocate effettuate. Se il parametro tipo vale 0, verranno ricevute le proprie giocate relative a estrazioni già effettuate; se il parametro tipo vale 1, verranno ricevute le proprie giocate attive, cioè quelle in attesa della prossima estrazione.\n\nEsempio:\n!vedi_giocate 0\n";
		invia_risposta(sd_client, risposta);
	}
	else if (strcmp(comando, "!vedi_estrazione")==0){
		risposta ="!vedi_estrazione n <ruota> - richiede al server i numeri estratti nelle ultime n estrazioni, sulla ruota specificata come parametro. Se la ruota non è specificata, il server invia i numeri estratti su tutte le ruote. Puo` essere specificata una sola ruota alla volta. Si ricorda che gli input validi per le ruote sono {Bari, Cagliari, Firenze, Genova, Milano, Napoli, Palermo, Roma, Torino, Venezia, Nazionale, Tutte}. Se il numero specificato dovesse superare il numero di estrazioni fatte, verranno comunque riportate tutte quelle fatte.\n\nEsempio:\n!vedi_estrazione 10 Nazionale\n";
		invia_risposta(sd_client, risposta);
	}
	else if (strcmp(comando, "!vedi_vincite")==0){
		risposta ="!vedi_vincite - richiede al server tutte le vincite conseguite dall'utente, l’estrazione in cui sono state realizzate e un consuntivo per tipologia di giocata. Non sono richiesti parametri.\n\nEsempio:\n!vedi_vincite\n";
		invia_risposta(sd_client, risposta);
	}
	else if (strcmp(comando, "!esci")==0){
		risposta ="!esci - esegue il logout.\n";
		invia_risposta(sd_client, risposta);
	}
	else{
		risposta ="Comando non trovato.\nSono disponibili i seguenti comandi:\n\n1) !help <comando> --> mostra i dettagli di un comando\n2) !signup <username> <password> --> crea un nuovo utente\n3) !login <username> <password> --> autentica un utente\n4) !invia_giocata g --> invia una giocata g al server\n5) !vedi_giocate tipo --> visualizza le giocate precedenti dove tipo = {0,1} e permette di visualizzare le giocate passate ‘0’ oppure le giocate attive ‘1’ (ancora non estratte)\n6) !vedi_estrazione <n> <ruota> --> mostra i numeri delle ultime n estrazioni sulla ruota specificata\n7) !esci --> termina il client\n***********\n\n";
		invia_risposta(sd_client, risposta);
	}
	risposta = "next_command";
	invia_risposta(sd_client,risposta);
}



/*la seguente funzione viene utilizzata per consentire ad un utente di registrarsi. Se la registrazione va a buon fine utente e password vengono memorizzati nel file utenti.txt
PARAMETRI:
-sd_client: socket descriptor del client in questione. Necessario per poter utilizzare la invia_risposta e la ricevi_dati.
-username, password: quelli scelti dall'utente. 
*/
void signup(int sd_client, char* username, char* password){

	char* risposta;
	int ret;
	FILE* fd;
	char target_user[25], target_password[25];

	fd = fopen("./files/utenti.txt", "a+");
	if(fd == NULL) {
		risposta = "Errore esterno registrazione non riuscita.\n";
		ret = invia_risposta(sd_client, risposta);
		exit(1);
	}

/viene ricercato all'interno del file utenti.txt se e` gia` presente un utente con un username coincidente con quello che si sta tentando di scrivere./
	while (fscanf(fd, "%s\t%s", &target_user, &target_password) != EOF){
		while (strcmp(target_user,username)==0) {
			int quanti = 2;
			char* parametri[quanti];
			risposta = "username gia' esistente. Riprova.";
			ret = invia_risposta(sd_client, risposta);
			risposta = "Inserisci username e password:";
			ret = invia_risposta(sd_client, risposta);
			wait_for_input(sd_client, quanti, parametri);
			username = parametri[0];
			
		}
	}
	ret = fprintf(fd, "%s\t%s\n", username, password);
	fclose(fd);

	char file_name[35];
	strcpy(file_name,"./files/utenti/");
	strcat(file_name, username);
	strcat(file_name, ".txt");
	fd = fopen(file_name, "w");
	ret = fprintf(fd, "%s", password);
	fclose(fd);

	risposta = "Registrazione eseguita con successo";
	ret = invia_risposta(sd_client, risposta);
	risposta = "next_command";
	ret = invia_risposta(sd_client, risposta);
}



/*la seguente funzione viene utilizzata per consentire ad un utente di effettuare il login. In assenza di errori viene assegnato un SESSION_ID all'utente e memorizzato nell'array_sessioni. Nel caso in cui un utente inserisse per 3 volte una password errata il suo indirizzo IP viene memorizzato in un file Blackist.txt, assieme al timestamp dell'ultimo tentativo.
PARAMETRI:
-sd_client: socket descriptor del client in questione. Necessario per poter utilizzare la invia_risposta e la ricevi_dati.
-username, password: quelli inseriti dall'utente.
-client_addr: indirizzo IP dell'utente. Utilizzato per gestire la Blacklist. 
-SESSION_ID: session id che viene assegnato al client. Utilizzato anche dalla funzione esci nei casi in cui si vuole forzare l'uscita del client.
*/
bool login(int sd_client, char* username, char* password, const struct sockaddr_in* client_addr, char* SESSION_ID){
	
	int i;
	char* risposta;
	int ret;
	FILE* fd;
	char file_name[35];
	char* temp = random_str(); /utilizzato all'invio del session id/

	strcpy(file_name,"./files/utenti/");
	strcat(file_name, username);
	strcat(file_name, ".txt");

	fd = fopen("./files/Blacklist.txt", "r");
	if(fd == NULL) {
		stampa_str("Errore in fase di apertura del file Blacklist.txt.");
	}

	else {
		while(fgets(buffer, 1024, fd)) {
			char* token = strtok(buffer, " ");
			char* ip_address = (char*) malloc((strlen(token)+1) * sizeof(char));
			strcpy(ip_address, token);
			if(strcmp(ip_address, inet_ntoa(client_addr->sin_addr)) == 0){
				time_t t = time(NULL);
				struct tm* now = localtime(&t);
				struct tm to_verify;

				token = strtok(NULL, " ");
				to_verify.tm_mday = atoi(token);
				token = strtok(NULL, " ");
				to_verify.tm_mon = atoi(token) - 1;
				token = strtok(NULL, " ");
				to_verify.tm_year = atoi(token) - 1900;
				token = strtok(NULL, " ");
				to_verify.tm_hour = atoi(token); 
				token = strtok(NULL, " ");
				to_verify.tm_min = atoi(token);
				token = strtok(NULL, " ");
				to_verify.tm_sec = atoi(token);

				if(difftime(mktime(now), mktime(&to_verify)) < 30){
					risposta = "Il tuo indirizzo IP e` stato bloccato.";
					ret = invia_risposta(sd_client, risposta);
					fclose(fd);
					esci(sd_client, SESSION_ID);	
					return false;
				}
			}
		}
		fclose(fd);
	}
	
	fd = fopen(file_name, "r");
	if(fd == NULL) {
		risposta = "Username errato o utente non registrato.\n";
		ret = invia_risposta(sd_client, risposta);
		risposta = "next_command";
		ret = invia_risposta(sd_client, risposta);
		return false;
	}

	/controllo se la password e' quella inserita/
	while (fgets(buffer, 1024, fd)){
		int tentativi = 2;
		char* target_password = (char*) malloc((strlen(buffer)+1) * sizeof(char));
		strcpy(target_password, buffer);
		while (strcmp(target_password, password)!=0) {
			char tentativi_char[65]; 
			int quanti = 1;
			char* parametri[quanti];
			sprintf(tentativi_char, "Errore di autenticazione: password errata. Tentativi rimasti: %d\n", tentativi);
			risposta = tentativi_char;
			ret = invia_risposta(sd_client, risposta);
			risposta = "Inserisci di nuovo la password.";
			ret = invia_risposta(sd_client, risposta);
			
			wait_for_input(sd_client, quanti, parametri);
			password = parametri[0];
			
			tentativi--;
			if(tentativi == 0){
				time_t t = time(NULL);
				struct tm* timestamp = localtime(&t);

				fclose(fd);		
				fd = fopen("./files/Blacklist.txt", "a");
				if(fd == NULL) {
					stampa_str("Errore nella creazione del file Blacklist.txt.");
					return false;
				}

				fprintf(fd, "%s %d %d %d %d %d %d\n",inet_ntoa(client_addr->sin_addr), timestamp->tm_mday, timestamp->tm_mon + 1, 1900 + timestamp->tm_year, timestamp->tm_hour, timestamp->tm_min, timestamp->tm_sec);

				fclose(fd);
				esci(sd_client, SESSION_ID);	
				return false;
			}
		}
	}
	fclose(fd);
	
	/invio del session_id/
	free(SESSION_ID);
	SESSION_ID = (char*) malloc((strlen(SESSION_ID)+1) * sizeof(char));
	strcpy(SESSION_ID, temp);
	sprintf(buffer, "SESSION_ID~%s", SESSION_ID);
	risposta = (char*) malloc((strlen(buffer)+1) * sizeof(char));
	strcpy(risposta, buffer);
	ret = invia_risposta(sd_client, risposta);
	sprintf(buffer, "Ciao, %s, il tuo SESSION ID è %s", username, SESSION_ID);
	risposta = (char*) malloc((strlen(buffer)+1) * sizeof(char));
	strcpy(risposta, buffer);
	ret = invia_risposta(sd_client, risposta);

	for(i = 0; i<MAX_LISTENED; i++){
		if(array_sessioni[i].session_id == NULL)	{	
			array_sessioni[i].session_id = (char*) malloc((strlen(SESSION_ID)+1) * sizeof(char));
			strcpy(array_sessioni[i].session_id, SESSION_ID);
			array_sessioni[i].username = (char*) malloc((strlen(username)+1) * sizeof(char));
			strcpy(array_sessioni[i].username, username);
			array_sessioni[i].sd = quante_sessioni;
			if(quante_sessioni < MAX_LISTENED)
				quante_sessioni++;
			break;
		}
	}
 
	risposta = "next_command";
	ret = invia_risposta(sd_client, risposta);
	return true;
}



/*la seguente funzione viene utilizzata per consentire ad un utente di inviare una giocata. In assenza di errori la giocata viene memorizzata nel file giocate1.txt. In particolare vengono memorizzati in ordine i seguenti campi:
-lunghezza dell'username
-username
-numero di ruote giocate
-ruote giocate
-numero di numeri giocati
-numeri giocati
-numero di puntate fatte
-puntate fatte

PARAMETRI:
-sd_client: socket descriptor del client in questione. Necessario per poter utilizzare la invia_risposta e la ricevi_dati.
-quante_ruote: numero di ruote inserite dall'utente.
-ruote: array di stringhe in cui vengono memorizzare le ruote.
-quanti_numeri: numero di numeri inseriti dall'utente.
-numeri: array di interi in cui vengono memorizzare i numeri.
-quante_quote: numero di quote inserite dall'utente.
-quote: array di double in cui vengono memorizzare le quote. 
-SESSION_ID: session id associato al client. Utilizzato per ricavare l'username dall'array_sessioni.
*/
void invia_giocata(int sd_client, int quante_ruote, char** ruote, int quanti_numeri, int numeri[], int quante_quote, double quote[], char* SESSION_ID){
	
	int i;
	char* risposta;
	char* username;	
	
	FILE* fd;
	fd = fopen("./files/giocate1.txt", "a");
	if(fd == NULL) {
		risposta = "Errore esterno.\n";
		invia_risposta(sd_client, risposta);
		stampa_str("Errore nella creazione del file giocate1.txt");
		exit(1);
	}
	
	/mi ricavo l'username dell'utente dall'array_sessioni/
	for(i = 0; i<MAX_LISTENED; i++) {
		if(array_sessioni[i].session_id && strcmp(array_sessioni[i].session_id, SESSION_ID) == 0) {
			username = (char*) malloc((strlen(array_sessioni[i].username)+1) * sizeof(char));
			strcpy(username, array_sessioni[i].username);
			break;
		}
	}

	fprintf(fd, "%d %s ", strlen(username), username);
	
	fprintf(fd, "%d ", quante_ruote);
	for(i = 0; ruote[i] != NULL; i++){
		fprintf(fd, "%s ", ruote[i]);
	}
		
	fprintf(fd, "%d ", quanti_numeri);
	for(i = 0; i<quanti_numeri; i++){
		fprintf(fd, "%d ", numeri[i]);
	}
	
	fprintf(fd, "%d ", quante_quote);
	for(i = 0; i<quante_quote; i++){
		fprintf(fd, "%.2f ", quote[i]);
	}

	fprintf(fd, "\n");
	fclose(fd);
	
	risposta = "Giocata inviata con successo.";
	invia_risposta(sd_client, risposta);

	risposta = "next_command";
	invia_risposta(sd_client, risposta);
}



/*la seguente funzione viene utilizzata per consentire ad un utente di vedere le giocate che ha effettuato.

PARAMETRI:
-sd_client: socket descriptor del client in questione. Necessario per poter utilizzare la invia_risposta e la ricevi_dati.
-tipo: intero che indica il tipo delle giocate che si vogliono vedere. 
-SESSION_ID: session id associato al client. Utilizzato per ricavare l'username dall'array_sessioni.
*/
void vedi_giocate(int sd_client, int tipo, char* SESSION_ID){

	FILE* fd;
	char file_name[35];
	char* risposta;

	sprintf(file_name, "./files/giocate%d.txt", tipo);
	fd = fopen(file_name, "r");
	if(fd == NULL) {
		stampa_str("Errore nell'apertura del file giocate");
		return;
	}
	
	while(fgets(buffer, 1024, fd)){
		int i, len;
		char* user;
		char* session_user;
		int quante_ruote, quanti_giocati, quante_quote;
		char** ruote_giocate;		
		int* numeri;
		double* quote_giocate;
		char* vittorie[5] = {"estratto", "ambo", "terno", "quaterna", "cinquina"};
		char* token = strtok(buffer, " ");
		
		len = atoi(token);

		user = (char*) malloc(len * sizeof(char));
		token = strtok(NULL, " ");
		strcpy(user, token);

		for(i = 0; i<MAX_LISTENED; i++) {
			if(array_sessioni[i].session_id && strcmp(array_sessioni[i].session_id, SESSION_ID) == 0) {
				session_user = (char*) malloc((strlen(array_sessioni[i].username)+1) * sizeof(char));
				strcpy(session_user, array_sessioni[i].username);
				break;
			}
		}

		if(strcmp(session_user, user)!=0)
			continue;

		token = strtok(NULL, " ");
		len = atoi(token);	/legge quante_ruote/
		quante_ruote = len;
		ruote_giocate = (char*) malloc(len * sizeof(char));
		for(i=0; i<len; i++){
			token = strtok(NULL, " ");
			ruote_giocate[i] = (char*) malloc((strlen(token)+1) * sizeof(char));
			strcpy(ruote_giocate[i], token);
		}		

		token = strtok(NULL, " ");
		quanti_giocati = atoi(token);	/legge quanti_giocati/
		numeri = (int*) malloc(5 * sizeof(int));
		for(i=0; i<quanti_giocati; i++){
			token = strtok(NULL, " ");
			numeri[i] = atoi(token);
		}

		token = strtok(NULL, " ");
		quante_quote = atoi(token);	/legge quante_quote/
		quote_giocate = (double*) malloc(quante_quote * sizeof(double));
		for(i=0; i<quante_quote; i++){
			token = strtok(NULL, " ");
			quote_giocate[i] = atof(token);
		}
		
		sprintf(buffer, "");

		for(i=0; i<quante_ruote; i++)
			sprintf(buffer+strlen(buffer), "%s ", ruote_giocate[i]);
		
		for(i=0; i<quanti_giocati; i++)
			sprintf(buffer+strlen(buffer), "%d ", numeri[i]);

		for(i=0; i<quante_quote; i++)
			sprintf(buffer+strlen(buffer), "* %.2f %s ", quote_giocate[i], vittorie[i]);
		
		risposta = (char*) malloc((strlen(buffer)+1) * sizeof(char));
		strcpy(risposta, buffer);
		invia_risposta(sd_client, risposta);
	}
	fclose(fd);
	risposta = "next_command";
	invia_risposta(sd_client, risposta);
}



/la seguente funzione viene utilizzata per verificare se una stringa corrisponde a una ruota valida/
bool is_ruota(char* str) {
	
	int i;
	if(strcmp(str, "Tutte") == 0) return true;	
	for(i=0; i<RUOTE; i++){
		if(strcmp(str, array_ruote[i]) == 0)
			return true;
	}
	return false;
}



/*la seguente funzione viene utilizzata per consentire ad un utente di vedere le ultime n estrazioni.

PARAMETRI:
-sd_client: socket descriptor del client in questione. Necessario per poter utilizzare la invia_risposta e la ricevi_dati.
-n: numero di estrazioni che devono essere riportate.
-ruota: specifica di quale ruota si vogliono vedere le estrazioni.
*/
void vedi_estrazione(int sd_client, int n, char* ruota){
	int partenza = 0;
	int ret;
	FILE* fd;
	char file_name[35];
	char* risposta;
	int ultima = 0;

	if(ruota && !is_ruota(ruota)){
		risposta = "Ruota inserita non valida";
		invia_risposta(sd_client, risposta);
		help(sd_client, "!vedi_estrazione");
		return;
	}

	while(fd != NULL || partenza == 0) {
		if(fd) fclose(fd);
		ultima++;
		sprintf(file_name, "./files/estrazioni/%d.txt", ++partenza);
		fd = fopen(file_name, "r");
	}
	ultima--;
	partenza = ultima - n;
	
	while(partenza < ultima){
		if(partenza < 0) partenza = 0;
		sprintf(file_name, "./files/estrazioni/%d.txt", ++partenza);
		fd = fopen(file_name, "r");
		if(fd == NULL) {
			stampa_str("Errore nell'apertura del file estrazioni");
			return;
		}
		while (fgets(buffer, 1024, fd)){
			risposta = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
			strcpy(risposta,buffer);
			risposta[strlen(risposta) - 1] = '\0';
		
			if(ruota == NULL || (strcmp(ruota, "Tutte") == 0)){
				ret = invia_risposta(sd_client, risposta);
			}
			else {
				char* token = strtok(buffer, " "); 
				if(strcmp(token, ruota)==0){
					ret = invia_risposta(sd_client, risposta);
					break;
				}
			}
		}
	risposta = "--------------------------------------------------";
	ret = invia_risposta(sd_client, risposta);
	
	}
	fclose(fd);
	risposta = "next_command";
	ret = invia_risposta(sd_client, risposta);
}



/*la seguente funzione viene utilizzata per consentire ad un utente di visualizzare un resoconto delle sue vincite. 

PARAMETRI:
-sd_client: socket descriptor del client in questione. Necessario per poter utilizzare la invia_risposta e la ricevi_dati.
-SESSION_ID: session id associato al client. Utilizzato per ricavare l'username dall'array_sessioni.
*/
void vedi_vincite(int sd_client, char* SESSION_ID){
	char* risposta;
	int i, len;

	int tipo_vincita;
	char* user;
	char* ruota;
	int* numeri_giocati;
	int quanti_indovinati;
	int* numeri;
	double vincita;
	double* vincite_recap = (double*) malloc(5 * sizeof(double));
	char risultato[80];	

	FILE* fd;

	fd = fopen("./files/vincite.txt", "r");
	if(fd == NULL) {
		stampa_str("Errore nell'apertura del file vincite.txt");
		risposta = "Vincite su ESTRATTO: 0.00\nVincite su AMBO: 0.00\nVincite su TERNO: 0.00\nVincite su QUATERNA: 0.00\nVincite su CINQUINA: 0.00\n";
		invia_risposta(sd_client, risposta);
		risposta = "next_command";
		invia_risposta(sd_client, risposta);
		return;
	}

	/mi ricavo l'username/
	for(i = 0; i<MAX_LISTENED; i++) {
		if(array_sessioni[i].session_id && strcmp(array_sessioni[i].session_id, SESSION_ID) == 0) {
			user = (char*) malloc((strlen(array_sessioni[i].username)+1) * sizeof(char));
			strcpy(user, array_sessioni[i].username);
			break;
		}
	}
	
	while(fgets(buffer, 1024, fd)){	
		char* data_estrazione;
		char* token = strtok(buffer, " ");
		
		if(strcmp(token, user)!=0) continue;

		token = strtok(NULL, " "); 				/legge la data/
		len = atoi(token);
		data_estrazione = (char*) malloc((len+1) * sizeof(char));
		token = strtok(NULL, " "); 
		strcpy(data_estrazione, token);
		while(strlen(data_estrazione) < len){
			token = strtok(NULL, " "); 
			sprintf(data_estrazione + strlen(data_estrazione), " %s", token);
			stampa_str(token);
			stampa_str(data_estrazione);
		}

		token = strtok(NULL, " "); /legge al ruota/
		ruota = (char*) malloc((strlen(token)+1) * sizeof(char));
		strcpy(ruota, token);

		token = strtok(NULL, " ");
		quanti_indovinati = atoi(token);	/*legge quanti_indovinati */
		
		numeri = (int*) malloc(quanti_indovinati * sizeof(int));
		for(i=0; i<quanti_indovinati; i++){
			token = strtok(NULL, " ");	/*legge i numeri indovinati */
			numeri[i] = atoi(token);
		}

		token = strtok(NULL, " "); /legge ambo,terno ecc/
		tipo_vincita = atoi(token);

		token = strtok(NULL, " "); /legge la vincita/
		vincita = atof(token);

		sprintf(risultato, "%s\n%s\t", data_estrazione, ruota);
		for(i=0; i<quanti_indovinati; i++)
			sprintf(risultato + strlen(risultato), "%d ", numeri[i]);
		sprintf(risultato + strlen(risultato), "\t>>\t");
		switch(tipo_vincita) {
			case 5: sprintf(risultato + strlen(risultato), " Cinquina"); break;
			case 4: sprintf(risultato + strlen(risultato), " Quaterna"); break;
			case 3: sprintf(risultato + strlen(risultato), " Terno"); break;
			case 2: sprintf(risultato + strlen(risultato), " Ambo"); break;
			case 1: sprintf(risultato + strlen(risultato), " Estratto"); break;
			default: sprintf(risultato + strlen(risultato), " Errore"); break;
		}

		sprintf(risultato + strlen(risultato), " %.2f\n", vincita);

		vincite_recap[tipo_vincita - 1] += vincita;

		risposta = (char*) malloc((strlen(risultato)+1) * sizeof(char));
		strcpy(risposta, risultato);
		invia_risposta(sd_client, risposta);
		free(risposta);
	}
	fclose(fd);

	sprintf(risultato, "Vincite su ESTRATTO: %.2f\nVincite su AMBO: %.2f\nVincite su TERNO: %.2f\nVincite su QUATERNA: %.2f\nVincite su CINQUINA: %.2f\n", vincite_recap[0], vincite_recap[1], vincite_recap[2], vincite_recap[3], vincite_recap[4]);
	risposta = (char*) malloc((strlen(risultato)+1) * sizeof(char));
	strcpy(risposta, risultato);
	invia_risposta(sd_client, risposta);
	free(risposta);

	risposta = "next_command";
	invia_risposta(sd_client, risposta);
}



/*la seguente funzione viene utilizzata per comunicare all'utente che il comando inserito non esiste o e` scritto in modo errato. 

PARAMETRI:
-sd_client: socket descriptor del client in questione. Necessario per poter utilizzare la invia_risposta e la ricevi_dati.
*/
void command_not_found(int sd_client){
	char* risposta = "Command not found";
	invia_risposta(sd_client, risposta);
	risposta = "next_command";
	invia_risposta(sd_client, risposta);
}



/la seguente funzione viene utilizzata per estarre casualmente 5 numeri/
void estrai_numeri(int array[]){

	int i, j;
	struct timeval time;
	gettimeofday(&time, NULL);
	srand((time.tv_sec * 1) + (time.tv_usec / 1));
	for(i = 0; i<5; i++){
		array[i] = rand() % 90 +1;
	}
	for(i=0; i<5; i++){
		for(j=i+1; j<5; j++)
			if(array[i]==array[j]){
				estrai_numeri(array);
				return;
			}
	}
}



/*la seguente funzione viene utilizzata, al momento di ogni estrazione, per verificare se per caso qualche utente ha vinto. Se cio` avviene viene notificato al server e la vincita viene memorizzata nel file vincite.txt. In particolare vengono memorizzati in ordine i seguenti campi:
-username
-lunghezza di una stringa contenente il timestamp
-stringa di cui sopra
-ruota su cui si ha vinto
-quanti e quali sono i numeri indovinati
-totale della vincita

PARAMETRI:
-estratti: i numeri estratti
-ruota: su che ruota
*/
void controlla_vincite(int estratti[5], char* ruota) {
	int i, j;
	char* user;
	int quante_ruote;
	char** ruote_giocate;
	int len;
	int* numeri_giocati;
	int quanti_indovinati, quanti_giocati;
	int* numeri_indovinati;
	double* quote_giocate;
	int k; /* per le combinazioni*/
	double vincita;
	int tipo_vincita;
	double premi[5] = {11.23, 250, 4500, 120000, 6000000};
	int* temp;
	FILE* fd;
	/*estratti[0] = 10;
	estratti[1] = 12;
	estratti[2] = 18;			/*DEBUG  
	estratti[3] = 52;
	estratti[4] = 36;*/
	fd = fopen("./files/giocate1.txt", "r");
	if(fd == NULL) {
		stampa_str("Errore nell'apertura del file giocate");
		return;
	}
	
	while(fgets(buffer, 1024, fd)){
		bool match = false;   /usato per verificare se un utente ha giocato sulla ruota in questione/
		char* token = strtok(buffer, " ");
		quanti_indovinati = 0;
		len = atoi(token);
		user = (char*) malloc(len * sizeof(char));
		token = strtok(NULL, " ");
		strcpy(user, token);

		token = strtok(NULL, " ");
		len = atoi(token);	/legge quante_ruote/
		quante_ruote = len;
		ruote_giocate = (char*) malloc(len * sizeof(char));
		for(i=0; i<len; i++){
			token = strtok(NULL, " ");
			ruote_giocate[i] = (char*) malloc((strlen(token)+1) * sizeof(char));
			strcpy(ruote_giocate[i], token);
			if(strcmp(ruote_giocate[i], ruota) == 0) {
				match = true;
			}
		}
		if(match == false) {   /vuol dire che l'utente in questione non ha giocato sulla ruota/
			continue;  		/presa in esame, quindi avanti il prossimo/
		}		

		token = strtok(NULL, " ");
		quanti_giocati = atoi(token);	/legge quanti_giocati/
		numeri_indovinati = (int*) malloc(5 * sizeof(int));
		for(i=0; i<quanti_giocati; i++){
			token = strtok(NULL, " ");
			for(j=0; j<5; j++) {
				if(estratti[j] == atoi(token)) {
					numeri_indovinati[quanti_indovinati] = atoi(token);
					quanti_indovinati++;
					break;				
				}			
			}
		}

		token = strtok(NULL, " ");
		len = atoi(token);	/legge quante_quote/
		quote_giocate = (double*) malloc(len * sizeof(double));
		for(i=0; i<len; i++){
			token = strtok(NULL, " ");
			quote_giocate[i] = atof(token);
		}
		
		if(numeri_indovinati[0] != 0) {
			time_t t = time(NULL);
			struct tm* timestamp = localtime(&t);
			char data_estrazione[30];

			FILE* fd_vincite;

			k = combinazioni(quanti_giocati, quanti_indovinati);
			vincita = quote_giocate[quanti_indovinati -1] * premi[quanti_indovinati -1] / (k*quante_ruote);

			printf("%s ha vinto %.2fE!\n", user, vincita);

			fd_vincite = fopen("./files/vincite.txt", "a");
			if(fd_vincite == NULL) {
				stampa_str("Errore nella creazione del file vincite");
				return;
			}
			
			sprintf(data_estrazione, "Estrazione del %d-%d-%d ore %d:%d", timestamp->tm_mday, timestamp->tm_mon + 1, 1900 + timestamp->tm_year, timestamp->tm_hour, timestamp->tm_min);	
	
			fprintf(fd_vincite, "%s %d %s %s %d", user, strlen(data_estrazione), data_estrazione, ruota, quanti_indovinati);
			for(i = 0; i<quanti_indovinati; i++) 
				fprintf(fd_vincite, "% d", numeri_indovinati[i]);
			tipo_vincita = quanti_indovinati;
			while(quote_giocate[tipo_vincita - 1]==0){
				tipo_vincita--;
			}
			
			fprintf(fd_vincite, " %d", tipo_vincita);
			
			fprintf(fd_vincite, " %.2f\n", vincita);
			fclose(fd_vincite);

		}
	}
	fclose(fd);
}



/*la seguente funzione viene utilizzata per generare estrazioni. Ogni estrazione viene memorizzata in un nuovo file nella cartella Estrazioni. Ad ogni ruota estratta viene controllato se qualche utente ha vinto.
Una volta estratte tutte le ruote il file giocate1.txt viene copiato in append nel file giocate0.txt e il file giocate1.txt viene svuotato.
*/
void nuova_estrazione() {

	int i, j;
	int ret;
	FILE* fd, *fd_giocate1, *fd_giocate0;
	char file_name[35];

	sprintf(file_name, "./files/estrazioni/%d.txt", ++ultima_estrazione);
	fd = fopen(file_name, "w");
	if(fd == NULL) {
		stampa_str("Errore nella creazione del file estrazioni");
		return;
	}
	
	for (i=0; i<RUOTE; i++){
		int estratti[5];
		fprintf(fd, "%s    \t", array_ruote[i]);
		estrai_numeri(estratti);
		for(j=0; j<5; j++){
			fprintf(fd, "%d\t", estratti[j]);
		}
		fprintf(fd, "\n");
		controlla_vincite(estratti, array_ruote[i]);
	}
	
	fclose(fd);
	stampa_str("Nuova Estrazione");

	fd_giocate1 = fopen("./files/giocate1.txt", "r");
	fd_giocate0 = fopen("./files/giocate0.txt", "a");
	
	while(fgets(buffer, 1024, fd_giocate1)){
		fprintf(fd_giocate0, "%s", buffer);
	}
	fclose(fd_giocate0);
	fclose(fd_giocate1);

	fd_giocate1 = fopen("./files/giocate1.txt", "w");
	fprintf(fd_giocate1, "");
	fclose(fd_giocate1);
}



/*la seguente funzione viene utilizzata per controllare la correttezza del formato del comando !invia_giocata e dei suoi parametri. In assenza di errori viene chiamata automaticamente la funzione invia_giocata.

PARAMETRI:
-sd_client: socket descriptor del client in questione. Necessario per poter utilizzare la invia_risposta e la ricevi_dati.
-len: numero di parametri inseriti. La forma corretta sono 4 parametri: comando, ruote, numeri, quote.
-parametri: array di stringhe in cui sono memorizzati i parametri inseriti.
-SESSION_ID: session id associato al client. Utilizzato per ricavare l'username dall'array_sessioni.
*/
void controlla_parametri_invia_g(int sd_client, int len, char** parametri, char* SESSION_ID){
	
	int i, j;
	char* risposta;
	char* ruote[RUOTE];
	int quante_ruote = 0;
	int quanti_numeri = 0;
	int quante_quote = 0;
	int numeri[10];
	double quote[5];
	char* token; 

	if(strcmp(parametri[0], "!invia_giocata") != 0){
		command_not_found(sd_client);
		return;
	}
	if(len != 4 || parametri[1][0] != 'r' || parametri[2][0] != 'n' || parametri[3][0] != 'i' || (parametri[1][0] == 'r' && parametri[1][1] != ' ') || (parametri[2][0] == 'n' && parametri[2][1] != ' ') || (parametri[3][0] == 'i' && parametri[3][1] != ' ')){
		help(sd_client, "!invia_giocata");
		return;
	}

	token = strtok(parametri[1], " ");

	token = strtok(NULL, " ");
	for(i=1; token != NULL; i++) {
		if(quante_ruote > RUOTE){
			risposta = "Inserite piu' ruote del possibile.\n";
			invia_risposta(sd_client, risposta);				
			help(sd_client, "!invia_giocata");
			return;
		}

		ruote[i-1] = "nessuna";
		if(strcmp(token, "Tutte")!=0){	
			for(j = 0; j<RUOTE; j++){
				if(strcmp(token, array_ruote[j])==0){
					ruote[i-1] = token;
					break;
				}
			}
		}
		quante_ruote++;
		token = strtok(NULL, " ");
	}
	
	for(i=0; i<RUOTE; i++){
		for(j=i+1; j<quante_ruote; j++)
			if(strcmp(ruote[i],ruote[j])==0 || strcmp(ruote[j],"nessuna")==0){
				risposta = "Inserite 2 ruote uguali o una ruota non valida\n";
				invia_risposta(sd_client, risposta);				
				help(sd_client, "!invia_giocata");
				return;
			}
	}

	token = strtok(parametri[2], " "); 
	token = strtok(NULL, " ");
	for(i=1; token != NULL; i++) {
		if(!controlla_int(token) || atoi(token) >90 || atoi(token)<0) {
			risposta= "Errore nel formato dei numeri";
			invia_risposta(sd_client, risposta);
			help(sd_client, "!invia_giocata");
			return;
		}
		numeri[i-1] = atoi(token);
		quanti_numeri++;
		token = strtok(NULL, " ");
	}

	for(i=0; i<10; i++){
		for(j=i+1; j<quanti_numeri; j++)
			if(numeri[i] == numeri[j]){
				risposta = "Inseriti 2 numeri uguali\n";
				invia_risposta(sd_client, risposta);				
				help(sd_client, "!invia_giocata");
				return;
			}
	}	
	
	token = strtok(parametri[3], " "); 
	token = strtok(NULL, " ");
	for(i=1; token != NULL; i++) {
		if(!controlla_double(token)) {
			risposta= "Errore nel formato dell' importo";
			invia_risposta(sd_client, risposta);
			help(sd_client, "!invia_giocata");
			return;
		}
		quote[i-1] = atof(token);
		quante_quote++;
		token = strtok(NULL, " ");
	}

	if(quante_quote > quanti_numeri){
		risposta= "Impossibile inserire piu' importi che numeri";
		invia_risposta(sd_client, risposta);
		help(sd_client, "!invia_giocata");
		return;
	}

	invia_giocata(sd_client, quante_ruote, ruote, quanti_numeri, numeri, quante_quote, quote, SESSION_ID);
}



int main(int argc, char* argv[]) {
	
	int porta, timer;
	
	int ret, sd, sd_client, len, i;
	pid_t pid, pid2;
	struct sockaddr_in my_addr, client_addr;
	uint16_t dim;
	char* parametri[36];
	char* SESSION_ID;
	
	for(i=0; i<36; i++)
		parametri[i] = NULL;

	if(argc<2 || argc>3){
		porta = 4242;
		timer = 300;
	}
	else {
		porta = atoi(argv[1]);
		if (argc == 3)
			timer = atoi(argv[2]); /controllo sul formato/
		else timer = 300; /secondi/
	}

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd<0)
		perror("Errore in fase di creazione del socket\n");
	else
		printf("Socket creato con successo\n");

	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(porta);
	my_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if(ret < 0)
		perror("Errore in fase della bind\n");
	else
		printf("Socket collegato con successo.\nIndirizzo: %s\nPorta: %d\n", inet_ntoa(my_addr.sin_addr), porta);

	ret = listen(sd, MAX_LISTENED);
	if(ret < 0)
		perror("Errore durante l'esecuzione della listen\n"); 
	else
		printf("Socket in ascolto.\nNumero massimo di richieste di connessione: %d\n", MAX_LISTENED);
	
	int volte = 0;

	pid2 = fork();

	if (pid2 < 0) 
		printf("Errore nella creazione del processo figlio per le estrazioni");
	
	else if (pid2 == 0) {
		while(1){
			sleep(timer);
			nuova_estrazione();
		}
	}

	while(1){
		len = sizeof(client_addr);
		sd_client = accept(sd, (struct sockaddr*) &client_addr, &len);
		/*if(sd_client < 0) {
			printf("Errore nella accept\n");
			exit(1);
		} */	
		if(sd_client>0) printf("Stabilita connessione col client %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port)); 
		SESSION_ID = (char*) malloc((strlen(inet_ntoa(client_addr.sin_addr))+1) * sizeof(char));
		strcpy(SESSION_ID, inet_ntoa(client_addr.sin_addr));
		
		pid = fork();
		
		if (pid < 0) 
			printf("Errore nella creazione del processo figlio");
		
		else if (pid == 0) {
			
			bool log = false;	
	
			close(sd);
			
			while(1) {			
				
				/si riceve il messaggio contenente il comando/
				ret = ricevi_dati(sd_client, buffer);
				
				char* token = strtok(buffer, "-");
				int quanti_parametri = 0;
				for(i=0; token != NULL; i++) {
					parametri[i] = (char*) malloc((strlen(token)+1) * sizeof(char));
					strcpy(parametri[i], token);
					parametri[i][strlen(parametri[i])-1] = '\0';
					quanti_parametri++;
					token = strtok(NULL, "-");
				}


				if(quanti_parametri > 1 && log){
					controlla_parametri_invia_g(sd_client, quanti_parametri, parametri, SESSION_ID);
					continue;
				}


				/*divido il comando in comando effettivo, salvato in parametri[0], e parametri. */
				quanti_parametri = 0;
				token = strtok(parametri[0], " "); 
				for(i=0; token != NULL; i++) {
					parametri[i] = token;
					quanti_parametri++;
					token = strtok(NULL, " ");
				}

				if(!log){
					if(parametri[0] && strcmp(parametri[0],"!signup")==0){
						if(!parametri[1] || strlen(parametri[1]) >25 || !parametri[2] || strlen(parametri[2]) >25 || parametri[3]){
							help(sd_client, "!signup");
							parametri[3] = NULL;
						}
						else
							signup(sd_client, parametri[1], parametri[2]);
					}
					else if (parametri[0] && strcmp(parametri[0],"!login")==0) { 
						if(!parametri[1] || strlen(parametri[1]) >25 || !parametri[2] || strlen(parametri[2]) >25 || parametri[3]){
							help(sd_client, "!login");
							parametri[3] = NULL;
						}
						else {
							log = login(sd_client, parametri[1], parametri[2], &client_addr, SESSION_ID);
						}
					}
					else if (parametri[0] && strcmp(parametri[0], "!help")==0) {
						if(quanti_parametri > 2){
							help(sd_client, "!help");
						}
						else
							help(sd_client, parametri[1]);
					}
					else {
						invia_risposta(sd_client, "Per prima cosa registrati, o esegui il login");
						invia_risposta(sd_client, "next_command");
					}

				for(i = 0; parametri[i]!=NULL; i++)
					parametri[i] = NULL;
				continue;
				}
				
				if (parametri[0] && strcmp(parametri[0], "!help")==0) {
					if(quanti_parametri > 2){
						help(sd_client, "!help");
						parametri[2] = NULL;
					}
					else
						help(sd_client, parametri[1]);
				}


				else if (parametri[0] && strcmp(parametri[0],"!signup")==0) { 
					if(!parametri[1] || strlen(parametri[1]) >25 || !parametri[2] || strlen(parametri[2]) >25 || parametri[3]){
						help(sd_client, "!signup");
						parametri[3] = NULL;
					}
					else	{
						invia_risposta(sd_client, "Hai gia` effettuato il log in. Usa il comando !esci per uscire");
						help(sd_client, "!esci");
					}
				}


				else if (parametri[0] && strcmp(parametri[0],"!login")==0) { 
					if(!parametri[1] || strlen(parametri[1]) >25 || !parametri[2] || strlen(parametri[2]) >25 || parametri[3]){
						help(sd_client, "!login");
						parametri[3] = NULL;
					}
					else {
						invia_risposta(sd_client, "Hai gia` effettuato il log in. Usa il comando !esci per uscire");
						help(sd_client, "!esci");
					}
				}


				else if (parametri[0] && strcmp(parametri[0],"!invia_giocata")==0) { 
					help(sd_client, "!invia_giocata");
				}


				else if (parametri[0] && strcmp(parametri[0],"!vedi_giocate")==0) { 
					if(quanti_parametri != 2 || !controlla_int(parametri[1]) || atoi(parametri[1]) > 1 || atoi(parametri[1]) < 0) 
						help(sd_client, "!vedi_giocate");
					else
						vedi_giocate(sd_client, atoi(parametri[1]), SESSION_ID);
				}


				else if (parametri[0] && strcmp(parametri[0],"!vedi_estrazione")==0) {
					if(!parametri[1] || !controlla_int(parametri[1]) || atoi(parametri[1]) < 1 || parametri[3]){				
						help(sd_client, "!vedi_estrazione");
						parametri[3] = NULL;
					}
					else vedi_estrazione(sd_client, atoi(parametri[1]), parametri[2]);
				}


				else if (parametri[0] && strcmp(parametri[0],"!vedi_vincite")==0) {
					if(quanti_parametri != 1) 
						help(sd_client, "!vedi_vincite");
					else
						vedi_vincite(sd_client, SESSION_ID);
				}


				else if (parametri[0] && strcmp(parametri[0],"!esci")==0) { 
					if(quanti_parametri != 1) 
						help(sd_client, "!esci");
					else
						esci(sd_client, SESSION_ID);
				}


				else { 
					command_not_found(sd_client);
				}

			
				for(i = 0; parametri[i]!=NULL; i++)
					parametri[i] = NULL;

			}
			ret = close(sd_client);
			stampa_int(ret);
		}
		else {
			ret = close(sd_client);
		}
	}
	printf("porta %d, timer %d\n",porta,timer);


}
