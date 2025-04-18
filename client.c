#include "common.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 1024
#define MSGSZ 1024
#define STR_MIN 8
#define ID_HOLD 9999
char ack = '1';

//inicializa 
int eq_id = ID_HOLD;
int clients_id[30];

void usage(){
    printf("Chamada correta: ./client <server IP> <port>\n");
    exit(EXIT_FAILURE);
}

struct sdata{
	struct sockaddr *saddr;
	int ssock;
	socklen_t saddrlen;
};

void process_command(char *comando, char *msg_out){  
    char buf[BUFSZ];
    //strcat(comando, " x");
    //printf("%s\n", comando);
	memset(buf, 0, BUFSZ);
	char *comando1 = strtok(comando, "\n");
    char *token = strtok(comando1, " ");
    //printf("%s\n", token);
	if(!strcmp(token, "kill")){
		//char *str_id = malloc(STR_MIN);
        char str_id[STR_MIN];
        sprintf(str_id, "%02d", eq_id); //parse int->string
		strcpy(buf, "REQ_DC(");
		strcat(buf, str_id);
        strcat(buf, ")");
		strcpy(msg_out, buf); //coloca o comando em msg_out
	}
	
	
    else if(!strcmp(token, "show")){
        token = strtok(NULL, " ");
        //printf("%s\n", token);
        if(!strcmp(token, "localmaxsensor")){
            strcpy(msg_out, "REQ_LS");
        }
        else if(!strcmp(token, "externalmaxsensor")){
            strcpy(msg_out, "REQ_ES");
         }
        else if(!strcmp(token, "localpotency")){
            strcpy(msg_out, "REQ_LP");
        }
        else if(!strcmp(token, "externalpotency")){
            strcpy(msg_out, "REQ_EP");
        }
        else if(!strcmp(token, "globalmaxsensor")){
            strcpy(msg_out, "REQ_MS");
        }
        else if(!strcmp(token, "globalmaxnetwork")){
            strcpy(msg_out, "REQ_MN");
        }
    }
}

void *get_command(void *data){
	struct sdata *dados = (struct sdata*) data;
	
	char comando[BUFSZ];
	char msg_out[BUFSZ];

	while(fgets(comando, BUFSZ, stdin)){
		//Processa comando e salva mensagem resultante em msg_out
		process_command(comando, (char *)msg_out);

		//Envia msg_out ao servidor
		if(strcmp(msg_out, "controle")){
			ssize_t count = send(dados->ssock, msg_out, strlen(msg_out) + 1, 0);
			if (count != strlen(msg_out) + 1) {
				logexit("erro ao enviar mensagem para o servidor");
			}
		}
	}
    pthread_exit(NULL);
}

int main(int argc, char **argv){
    if(argc < 3){
        usage();
    }

    //inicializa o vetor de dispositivos, colocando ID_HOLD em todos , o que significa que nao esta na base dados
	for(int i = 0; i < 30; i++){
		clients_id[i] = ID_HOLD;
	}
    
    // - CONEXAO COM SERVIDOR - //

    // Recebe versao e porta na variavel storage
    struct sockaddr_storage storage;
    if(0 != addrparse(argv[1], argv[2], &storage)){
        usage();
    }

    // inicializa o socket
    int s = socket(storage.ss_family, SOCK_STREAM, 0);
    if(s == -1){
        logexit("erro ao incializar socket");
    }

    //Salva o endereco de storage em addr, que eh do tipo correto (addr_storage nao eh suportado)
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    socklen_t addr_len = sizeof(storage);

    //Abre uma conexao no socket em um determinado endereco
    if(connect(s, addr, sizeof(storage)) != 0){
        logexit("erro ao conectar com servidor");
    }

    //Transforma addr de volta pra string e printa
    char addrstr[BUFSZ];
	addrtostr(addr, addrstr, BUFSZ);

    printf("conectado a %s\n", addrstr);

    // - TROCA DE MENSAGENS - //

    char *buf = malloc(BUFSZ);
    // memset(buf, 0, BUFSZ - 1);
    
    //Mandar REQ_ADD ao inicializar o equipamento
	strcpy(buf, "REQ_ADD()");
	ssize_t count = send(s, buf, strlen(buf) + 1, 0);
	if (count != strlen(buf) + 1) {
		logexit("erro ao enviar mensagem");
	}

	struct sdata *send_data = malloc(sizeof(send_data)); //passa socket do servidor e dados do endereco
	send_data->ssock = s;
	send_data->saddr = addr;
	send_data->saddrlen = addr_len;

    //Cria thread com loop para pegar comando do teclado e enviar mensagem resultante ao servidor 
    pthread_t thread_command;
	if(0 != pthread_create(&thread_command, NULL, get_command, send_data)){
	    logexit("erro ao criar thread");
    }
    // Entra em loop para receber mensagens
    while(1){
        // Recebe mensagem de resposta
        buf = malloc(BUFSZ);
        count = recv(s, buf, BUFSZ - 1, 0);
        
        if(count < 0){
            logexit("erro ao receber mensagem do servidor");
        }

        //Trata mensagem de resposta e imprime aviso na tela
        char *token = strtok(buf, "("); //token = type
        unsigned type = parse_msg_type(token);
        switch (type){
            case ERROR:
                token = strtok(NULL, ")"); //token = codigo do erro
                unsigned code = atoi(token);

                switch (code){
                    case 1:
                        printf("Client limit exceeded\n");
                    break;

                    case 2:
                        printf("Peer limit exceeded\n");
                    break;

                    case 3:
                        printf("Peer not found\n");
                    break;

                    case 4:
                        printf("Client not found\n");
                    break;


                    default:
                        
                    break;
                }
            break;

            case OK:
                token = strtok(NULL, ")"); //token = codigo do erro
                code = atoi(token);

                switch (code){

                    case 1:
                        printf("Successful disconnect\n");
                        //Fecha o socket
                        close(s);
                        exit(EXIT_SUCCESS);
                    break;

                    default:
                        
                    break;
                }
            break;

            case RES_ADD:
                token = strtok(NULL, ")"); //token = client Id
                if(eq_id == ID_HOLD){
                    //se o id nao tiver sido inicializado
                    eq_id = atoi(token);
                    printf("New ID: %s\n", token);
                    char *str_id = malloc(STR_MIN);
                    sprintf(str_id, "%02d", 1); //parse int->string
		            strcpy(buf, str_id);
                    //envia ack
                    ssize_t count = send(s, buf, strlen(buf) + 1, 0);
                    if (count != strlen(buf) + 1) {
		                logexit("erro ao enviar mensagem");
	                }
                } 
                else{
                    //se o id ja tiver sido inicializado
                    for(int i = 0; i < 30; i++) {
                        if(clients_id[i] == ID_HOLD) {
                            clients_id[i] = atoi(token);
                            break;
                        }
                    }
                    //printf("Client %s added\n", token);
                }
                break;            

            case RES_LS:
                
                token = strtok(NULL, ")");
                printf("%s)\n", token);
                
            break;

            case RES_LP:
                
                token = strtok(NULL, ")");
                printf("%s\n", token);
                
            break;

            case RES_ES:

                token = strtok(NULL, ")");
                printf("%s)\n", token);

            break;

            case RES_EP:

                token = strtok(NULL, ")");
                printf("%s\n", token);

            break;

            case RES_MS:

                token = strtok(NULL, ")");
                printf("%s)\n", token);

            break;

            case RES_MN:

                token = strtok(NULL, ")");
                printf("%s\n", token);

            break;

            case RES_LIST:
                //recebe RES_LIST(<id1> <id2> ...  e adiciona todos os ids no vetor int clients_id
                //token = eq_id
                while((token = strtok(NULL, " ")) != NULL){
                    //adiciona todos os ids listados nas primeiras posicoes do vetor
                    for(int i = 0; i < 30; i++) {
                        if(clients_id[i] == ID_HOLD) {
                            clients_id[i] = atoi(token);
                            break;
                        }
                    }
                }      
            break;

            case REQ_INF:
                token = strtok(NULL, ", "); //token = eq_id que pediu info
                int src_id = atoi(token);
                token = strtok(NULL, ")"); //token = eq_id desse equipamento
                int dest_id = atoi(token);
                printf("client %02u requested information \n", src_id);

                srand(time(NULL));
				float random = ((float)rand()/(float)RAND_MAX) * 10.0; ; //gera um valor aleatorio com duas casas entre 0 e 10

                memset(buf, 0, BUFSZ);
				strcpy(buf, "RES_INF(");
				char *str_id = malloc(STR_MIN);
				
				sprintf(str_id, "%02d,", dest_id); 
				strcat(buf, str_id); //id_dest
				sprintf(str_id, " %02d,", src_id); 
				strcat(buf, str_id); //id_src;
				sprintf(str_id, " %.2f)", random); 
				strcat(buf, str_id); //value
				
				count = send(s, buf, strlen(buf) + 1, 0);
				if (count != strlen(buf) + 1) {
					logexit("erro ao enviar mensagem com sendto");
				}

            break;

            case REQ_DC:
                token = strtok(NULL, ")"); //token = eq_id a ser deletado
                int del_id = atoi(token);
                for(int i = 0; i < 30; i++) {
                    if(clients_id[i] == del_id) {
                        clients_id[i] = ID_HOLD;
                        break;
                    }
                }
                //printf("Client %s removed\n", token);
            break;

            case RES_INF:
                token = strtok(NULL, ", "); //id do eq que enviou a resposta
                dest_id = atoi(token);
                token = strtok(NULL, ", "); //id do equipamento que requisitou a info (esse)
                src_id = atoi(token);
                token = strtok(NULL, ")"); //valor da resposta
                float value = atof(token);

                printf("Value from %02d: %.2f \n",dest_id, value);
            break;

            default:
            break;
        }
        free(buf);
        buf = NULL;
        token = NULL;
    }
    //Ao sair do loop, fecha o socket e finaliza a conexao
    close(s);

    //Termina o programa
    exit(EXIT_SUCCESS);
}


