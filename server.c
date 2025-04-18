#include "common.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define BUFSZ 1024
#define MSGSZ 1024
#define STR_MIN 8

int aux_verify = 9999;
int clients[30][3];
int aux_peer = 0; //auxiliar que verifica se o servidor tem um peer
int num_eq = 0; //numero de clients no cluster
int id_eq = 0; //id do cliente
int peer_id = 0; // id do peer
int sockcl = 0; // socket do cliente
int peer_socket = 0; //socket do peer
int my_id = 0; //id do cluster
int psv = 0; // variavel de controle para saber se o servidor eh o primeiro a entrar e portanto se torna um servidor passivo
int aux = 0;

void usage(int argc, char **argv){
    printf("Chamada correta: ./server %s <port> <port>\n", argv[1]);
    exit(EXIT_FAILURE);
}

struct smartgrid
{
    int nsens;
    int *peer;
    int idsens[10];
    int pot[10];
    int efic[10];
    int util[10];
};


//Funcoes pra construir mensagens de controle ERROR e OK ja em formato de string
void build_error_msg(char *msg_out, unsigned codigo){
    
    //parse int->str
    char *code_aux = malloc(sizeof(STR_MIN));
    sprintf(code_aux, "%02u", codigo);

    strcpy(msg_out, "ERROR(");
    strcat(msg_out, code_aux);
    strcat(msg_out, ")");
    strcat(msg_out, "\n");

    free(code_aux);
}
void build_ok_msg(char *msg_out, unsigned codigo){
    
    //parse int->str
    char *code_aux = malloc(sizeof(STR_MIN));
    sprintf(code_aux, "%02u", codigo);

    strcpy(msg_out, "OK(");
    strcat(msg_out, code_aux);
    strcat(msg_out, ")");
    strcat(msg_out, "\n");

    free(code_aux);
}

//struct com os dados do cliente que sera passado na hora de fazer a troca de mensagens
struct client_data {
    int csock;
    struct smartgrid *mrd;
    struct sockaddr_storage storage;
};

//struct com os dados do servidor que sera passado na hora de fazer a troca de mensagens
struct server_data {
	int ssock;
	struct sockaddr_storage storage;
    struct smartgrid *mrd;
};

struct sdata{
	struct sockaddr *saddr;
	int ssock;
	socklen_t saddrlen;
};


void * server_thread(void *data) {
    struct server_data *sdata = (struct server_data *) data;
    struct sockaddr *saddr = (struct sockaddr *)(&sdata->storage);

    // Transforma o endereco de sockaddr pra string
    char saddrstr[BUFSZ];
    addrtostr(saddr, saddrstr, BUFSZ);
    
    while (1) {
        char req_msg[MSGSZ];
        memset(req_msg, 0, MSGSZ);
        size_t count = recv(sdata->ssock, req_msg, MSGSZ - 1, 0);
        if(count < 0){
            logexit("erro ao receber mensagem do servidor");
        }

        //printf("%s", req_msg);

        //Processa a mensagem recebida e guarda a mensagem de resposta ao peer numa string
        char res_msg[MSGSZ];
        memset(res_msg, 0, MSGSZ);

        char preset[MSGSZ];
        strcpy(preset, req_msg);

        char *token = strtok(req_msg, "("); //token = type
        unsigned req_type = parse_msg_type(token);


        switch (req_type) {
            case OK:
                token = strtok(NULL, ")"); //token = codigo do erro
                unsigned code = atoi(token);
                switch (code){
                    case 1:
                        printf("Successful disconnect\n"); 
                        printf("Peer %02d disconnected \n", peer_id);
                        peer_id = 0; //remove o id do peer da base de dados
                        peer_socket = 0; //remove o socket do peer da base de dados
                        aux_peer = 0;
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] != 0 && clients[i][2] == 2){
                                int aux_id = clients[i][0];
                                clients[i][0] = 0;
                                clients[i][1] = 0;
                                clients[i][2] = 0;
                                char *str_id = malloc(STR_MIN);
                                strcpy(res_msg, "REQ_DC(");
                                sprintf(str_id, "%02d)", aux_id); //parse int->string
                                strcat(res_msg, str_id);
                                for(int j = 0; j < 30; j++){
                                    if(clients[j][0] != 0 && clients[j][2] == 1){               
                                        count = send(clients[j][1], res_msg, strlen(res_msg) + 1, 0);
                                        if (count != strlen(res_msg) + 1) {
                                            logexit("erro ao enviar mensagem de resposta");
                                        }                                        
                                    }
                                }
                            }
                        }
                    break;
                }
            break;
            case ERROR:
                token = strtok(NULL, ")"); //token = codigo do erro
                 code = atoi(token);
                switch (code){  
                    case 2:
                        printf("Peer limit exceeded\n");
                    break;
                    case 3:
                        printf("Peer not found\n");
                    break;
                }
                break;
            case REQ_ADDPEER:
                if(aux_peer == 1) {
                    build_error_msg(res_msg, 2);
                    //Envia a mensagem de resposta
                    count = send(sdata->ssock, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                    close(sdata->ssock);
                    pthread_exit(NULL);
                } else {
                    aux_peer = 1;
                    srand(time(NULL));
                    int num = (rand()%100) + 1; // gera um valor aleatorio entre 1 e 100 para ser o id do peer
                    peer_id = num;
                    printf("Peer %02u connected\n", peer_id); //imprime o id do peer conectado
                    char *str_id = malloc(STR_MIN);
                    sprintf(str_id, "%02d", peer_id); //parse int->string
                    strcpy(res_msg, "RES_ADD(");
                    strcat(res_msg, str_id);
                    strcat(res_msg, ")");
                    count = send(sdata->ssock, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                }
                break;
            case RES_ADD:
                token = strtok(NULL, ")"); //token = id do peer
                int rcvdid = atoi(token);
                printf("New Peer ID: %02u \n", rcvdid); //imprime o id do cluster
                my_id = rcvdid;
                //manda RES_LIST para o peer que acabou de ser cadastrado
                memset(res_msg, 0, MSGSZ);
                strcpy(res_msg, "RES_LIST(");
                int aux_id = 0;
                char *str_ids = malloc(STR_MIN);
                for(int i = 0; i < 30; i++){
                    //verifica quais clients estao cadastrados e adicona seu id na mensagem
                    if(clients[i][0] != 0){
                        aux_id = clients[i][0];
                        sprintf(str_ids, "%02d ", aux_id); //parse int->string
                        strcat(res_msg, str_ids);
                    }
                }
                count = send(sdata->ssock, res_msg, strlen(res_msg) + 1, 0);
                if (count != strlen(res_msg) + 1) {
                    logexit("erro ao enviar mensagem de resposta");
                }
                break;     
            case REQ_DCPEER:
                token = strtok(NULL, ")"); //token = id do peer a ser removido
                aux_id = atoi(token);
                
                //verifica o id recebido é igual ao id do peer que esta atualmente conectado
                if(aux_id != peer_id) {
                    build_error_msg(res_msg, 3);
                    //Envia a mensagem de resposta
                    count = send(sdata->ssock, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                } else {
                    build_ok_msg(res_msg, 1);
                    count = send(sdata->ssock, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                    printf("Peer %02d disconnected \n", aux_id);
                    peer_id = 0; //remove o id do peer da base de dados
                    peer_socket = 0; //remove o socket do peer da base de dados
                    aux_peer = 0;
                    for(int i = 0; i < 30; i++){
                        if(clients[i][0] != 0 && clients[i][2] == 2){
                            aux_id = clients[i][0];
                            clients[i][0] = 0;
                            clients[i][1] = 0;
                            clients[i][2] = 0;
                            char *str_id = malloc(STR_MIN);
                            strcpy(res_msg, "REQ_DC(");
                            sprintf(str_id, "%02d)", aux_id); //parse int->string
                            strcat(res_msg, str_id);
                            for(int j = 0; j < 30; j++){
                                if(clients[j][0] != 0 && clients[j][2] == 1){
                                    count = send(clients[j][1], res_msg, strlen(res_msg) + 1, 0);
                                    if (count != strlen(res_msg) + 1) {
                                        logexit("erro ao enviar mensagem de resposta");
                                    }                                                                
                                }
                            }
                        }
                    }

                }
            break;  
            case REQ_ADDC2P:
                token = strtok(NULL, ")"); //token = id do Client que acabou de ser adicionado
                aux_id = atoi(token);
                clients[aux_id - 1][0] = aux_id;
                clients[aux_id - 1][2] = 2;
                printf("Client %02u added \n", aux_id);
                char *str_id = malloc(STR_MIN);
                sprintf(str_id, "%02d", aux_id); //parse int->string
                strcpy(res_msg, "RES_ADD(");
                strcat(res_msg, str_id);
                strcat(res_msg, ")");
                for(int i = 0; i < 30; i++){
                    if(clients[i][0] != 0 && clients[i][2] == 1){
                        count = send(clients[i][1], res_msg, strlen(res_msg) + 1, 0);
                        if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                    }
                } 
            break;
            case REQ_REMCFP:
                token = strtok(NULL, ")"); //token = id do Client que acabou de ser adicionado
                aux_id = atoi(token);
                clients[aux_id - 1][0] = 0;
                clients[aux_id - 1][2] = 0;
                //printf("Client %02u removed \n", aux_id);
                str_id = malloc(STR_MIN);
                sprintf(str_id, "%02d", aux_id); //parse int->string
                strcpy(res_msg, "REQ_DC(");
                strcat(res_msg, str_id);
                strcat(res_msg, ")");
                for(int i = 0; i < 30; i++){
                    if(clients[i][0] != 0 && clients[i][2] == 1){
                        count = send(clients[i][1], res_msg, strlen(res_msg) + 1, 0);
                        if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                    }
                } 
            break;


            case REQ_ES:

            
                aux = 0;

                for(int i=0; i<sdata->mrd->nsens; i++){
                    if(sdata->mrd->util[i] > aux){
                        aux = sdata->mrd->util[i];
                        sprintf(res_msg, "RES_ES(external %d sensor %d: %d (%d %d)", peer_id, sdata->mrd->idsens[i], aux, sdata->mrd->pot[i], sdata->mrd->efic[i]);
                    }
                    //printf("%d\n", cdata->mrd->idsens[i]);
                }
                 count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                printf("REQ_ES\n");
                printf("RES_ES\n");



            break;

            
            case RES_ES:

                token = strtok(NULL, ")");
                printf("%s)\n", token);
                count = send(sockcl, preset, strlen(preset) + 1, 0);
                if (count != strlen(preset) + 1) {
                    logexit("erro ao enviar mensagem de resposta");
                }

            break;

            case REQ_EP:

            
                aux = 0;

                for(int i=0; i<sdata->mrd->nsens; i++){

                        aux += sdata->mrd->util[i];
                    //printf("%d\n", cdata->mrd->idsens[i]);

                }
                sprintf(res_msg, "RES_EP(external %d potency: %d)", peer_id, aux);
                count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                    printf("REQ_EP\n");
                    printf("RES_EP\n");



            break;

            
            case RES_EP:

                token = strtok(NULL, ")");
                printf("%s\n", token);
                count = send(sockcl, preset, strlen(preset) + 1, 0);
                if (count != strlen(preset) + 1) {
                    logexit("erro ao enviar mensagem de resposta");
                }

            break;


            case REQ_MS:

            
                aux = 0;

                for(int i=0; i<sdata->mrd->nsens; i++){
                    if(sdata->mrd->util[i] > aux){
                        aux = sdata->mrd->util[i];
                        sprintf(res_msg, "RES_MS(%d %d %d %d %d)", peer_id, sdata->mrd->idsens[i], aux, sdata->mrd->pot[i], sdata->mrd->efic[i]);
                    }
                    //printf("%d\n", cdata->mrd->idsens[i]);
                }
                 count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                printf("REQ_ES\n");    
                printf("RES_ES\n");



            break;

            
            case RES_MS:

                token = strtok(NULL, ") "); 
                int local = atoi(token);
                token = strtok(NULL, " "); 
                int sensor = atoi(token);
                token = strtok(NULL, " "); 
                int util = atoi(token);
                token = strtok(NULL, " "); 
                int pote = atoi(token);
                token = strtok(NULL, " "); 
                int efici = atoi(token);

                int auxut = 0;
                int auxid = 0;
                int auxpot = 0;
                int auxefic = 0;

                for(int i=0; i<sdata->mrd->nsens; i++){
                    if(sdata->mrd->util[i] > auxut){
                        auxut = sdata->mrd->util[i];
                        auxid = sdata->mrd->idsens[i];
                        auxpot = sdata->mrd->pot[i];
                        auxefic = sdata->mrd->efic[i];
                        }
                    }

                if(auxut > util){
                    sprintf(res_msg, "RES_MS(global %d sensor %d: %d (%d %d)", peer_id, auxid, auxut, auxpot, auxefic);
                }
                else{
                    sprintf(res_msg, "RES_MS(global %d sensor %d: %d (%d %d)", local, sensor, util, pote, efici);
                }

                count = send(sockcl, res_msg, strlen(res_msg) + 1, 0);
                if (count != strlen(res_msg) + 1) {
                    logexit("erro ao enviar mensagem de resposta");
                }

                token = strtok(res_msg, "(");
                token = strtok(NULL, ")");

                printf("%s)\n", token);
                

            break;

            case REQ_MN:

            aux = 0;

                for(int i=0; i<sdata->mrd->nsens; i++){

                    aux += sdata->mrd->util[i];
        
                }

                sprintf(res_msg, "RES_MN(%d %d)", peer_id, aux);
                count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                printf("REQ_EP\n");    
                printf("RES_EP\n");

            break;


            case RES_MN:

            token = strtok(NULL, ") "); 
            local = atoi(token);
            token = strtok(NULL, " "); 
            util = atoi(token);

            auxut = 0;

            for(int i=0; i<sdata->mrd->nsens; i++){
    
                auxut += sdata->mrd->util[i];
                    
            }

            if(auxut > util){
                sprintf(res_msg, "RES_MN(global %d potency: %d", peer_id, auxut);
            }
            else{
                sprintf(res_msg, "RES_MN(global %d potency: %d", local, util);
            }

                count = send(sockcl, res_msg, strlen(res_msg) + 1, 0);
                if (count != strlen(res_msg) + 1) {
                    logexit("erro ao enviar mensagem de resposta");
                }

                token = strtok(res_msg, "(");
                token = strtok(NULL, ")");

                printf("%s\n", token);

            break;


            case REQ_INF:
                token = strtok(NULL, ", "); //token = eq_id que pediu info
                int src_id = atoi(token);
                token = strtok(NULL, ")"); //token = eq_id a ser consultado
                int dest_id = atoi(token);
                //checa a posicao do id de destino no banco de dados
                for(int i = 0; i < 30; i++){
                    if(clients[i][0] == dest_id){
                        aux_verify = i;
                        break;
                    }   
                }
                if(clients[aux_verify][2] == 1) {
                    memset(res_msg, 0, MSGSZ);
                    char *str_id = malloc(STR_MIN);
                    sprintf(str_id, "%02d,", src_id); //parse int->string
                    strcpy(res_msg, "REQ_INF(");
                    strcat(res_msg, str_id);
	                sprintf(str_id, " %02d)", dest_id); //parse int->string
                    strcat(res_msg, str_id);
                    count = send(clients[aux_verify][1], res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                } else {
                    memset(res_msg, 0, MSGSZ);
                    char *str_id = malloc(STR_MIN);
                    sprintf(str_id, "%02d,", src_id); //parse int->string
                    strcpy(res_msg, "REQ_INF(");
                    strcat(res_msg, str_id);
		            sprintf(str_id, " %02d)", dest_id); //parse int->string
                    strcat(res_msg, str_id);
                    count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                }                       
            break;     
            case RES_INF:
                token = strtok(NULL, ", "); //id do eq que enviou a resposta
                dest_id = atoi(token);
                token = strtok(NULL, ", "); //id do equipamento que requisitou a info
                src_id = atoi(token);
                token = strtok(NULL, ")"); //valor da resposta
                float value = atof(token);
            
                for(int i = 0; i < 30; i++){
                    if(clients[i][0] == src_id){
                        aux_verify = i;
                        break;
                    }   
                }
                if(clients[aux_verify][2] == 1) {
                        memset(res_msg, 0, MSGSZ);
                        char *str_id = malloc(STR_MIN);
                        strcpy(res_msg, "RES_INF(");
                        sprintf(str_id, "%02d,", dest_id); //parse int->string
                        strcat(res_msg, str_id);
	                    sprintf(str_id, " %02d,", src_id); //parse int->string
                        strcat(res_msg, str_id);
                        sprintf(str_id, " %.2f)", value); 
				        strcat(res_msg, str_id); //value
                        count = send(clients[aux_verify][1], res_msg, strlen(res_msg) + 1, 0);
                        if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                    } else {
                        memset(res_msg, 0, MSGSZ);
                        char *str_id = malloc(STR_MIN);
                        strcpy(res_msg, "RES_INF(");
                        sprintf(str_id, "%02d,", dest_id); //parse int->string
                        strcat(res_msg, str_id);
		                sprintf(str_id, " %02d,", src_id); //parse int->string
                        strcat(res_msg, str_id);
                        sprintf(str_id, " %.2f)", value); 
				        strcat(res_msg, str_id); //value
                        count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                        if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                    }            
            break;
        }
    }
}

// thread para que o server consiga lidar com mutiplos clientes em paralelo
void * client_thread(void *data) {

    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    //cdata->cn->bst = cdata->mrd;

    // Transforma o endereco de sockaddr pra string
    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);

    while (1) {    
        //Recebe mensagem do cliente
        char req_msg[MSGSZ];
        char ack[STR_MIN];
        memset(ack, 0, MSGSZ);
        memset(req_msg, 0, MSGSZ);
        size_t count = recv(cdata->csock, req_msg, MSGSZ - 1, 0);
        
        sockcl = cdata->csock;

        //Processa a mensagem recebida e guarda a mensagem de resposta ao cliente numa string
        char res_msg[MSGSZ];
        memset(res_msg, 0, MSGSZ);

        char *token = strtok(req_msg, "("); //token = type
        unsigned req_type = parse_msg_type(token);

        switch (req_type){
            case REQ_ADD:
                //verifica se a quantidade de maxima de conexoes foi atingida, se sim retorna erro
                if(num_eq==MAX_EQ_ID){
                    build_error_msg(res_msg, 1);
                    //Envia a mensagem de resposta
                    count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                    close(cdata->csock);
                    pthread_exit(NULL);
                } else {
                    // verifica se o server ja tem um peer
                    if(aux_peer == 0) {
                        num_eq++;
                        //registra o novo equipamento na base de dados na primeira posicao vazia do vetor
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] == 0){
                                id_eq = i + 1;
                                clients[i][0] = id_eq; //primeira posicao recebe o id
                                clients[i][1] = cdata->csock; // segunda posicao recebe o socket
                                clients[i][2] = 1; // terceira posicao recebe 1, indicando que o equipamento pertence a aquele cluster
                                break;
                            }   
                        }
                        printf("Client %02u added\n", id_eq); //imprime o equipamento que foi adicionado no servidor
                        //envia para todos os clients do cluster RES_ADD(eq_id)
                        char *str_id = malloc(STR_MIN);
                        sprintf(str_id, "%02d", id_eq); //parse int->string
                        strcpy(res_msg, "RES_ADD(");
                        strcat(res_msg, str_id);
                        strcat(res_msg, ")");
                        //broadcast
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] != 0){
                                count = send(clients[i][1], res_msg, strlen(res_msg) + 1, 0);
                                if (count != strlen(res_msg) + 1) {
                                    logexit("erro ao enviar mensagem de resposta");
                                }
                            }
                        } 
                        //recebe ack do equipamento
                        size_t count = recv(cdata->csock, ack, STR_MIN + 1, 0);
                        if(count > 0) {
                            //manda RES_LIST para o equipamento que acabou de ser cadastrado
                            memset(res_msg, 0, MSGSZ);
                            strcpy(res_msg, "RES_LIST(");
                            int aux_id = 0;
                            char *str_ids = malloc(STR_MIN);
                            for(int i = 0; i < 30; i++){
                                //verifica quais clients estao cadastrados e adicona seu id na mensagem
                                if(clients[i][0] != 0){
                                    aux_id = clients[i][0];
                                    sprintf(str_ids, "%02d ", aux_id); //parse int->string
                                    strcat(res_msg, str_ids);
                                }
                            }
                            count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);
                            if (count != strlen(res_msg) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        }
                    } else {
                        num_eq++;
                        //registra o novo equipamento na base de dados na primeira posicao vazia do vetor
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] == 0){
                                id_eq = i + 1;
                                clients[i][0] = id_eq; //primeira posicao recebe o id
                                clients[i][1] = cdata->csock; // segunda posicao recebe o socket
                                clients[i][2] = 1; // terceira posicao recebe 1, indicando que o equipamento pertence a aquele cluster
                                break;
                            }   
                        }
                        printf("Client %02u added\n", id_eq); //imprime o equipamento que foi adicionado no servidor
                        //envia para todos os clients do cluster RES_ADD(eq_id)
                        char *str_id = malloc(STR_MIN);
                        sprintf(str_id, "%02d", id_eq); //parse int->string
                        strcpy(res_msg, "RES_ADD(");
                        strcat(res_msg, str_id);
                        strcat(res_msg, ")");
                        //broadcast
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] != 0 && clients[i][2] == 1){
                                count = send(clients[i][1], res_msg, strlen(res_msg) + 1, 0);
                                if (count != strlen(res_msg) + 1) {
                                    logexit("erro ao enviar mensagem de resposta");
                                }
                            }
                        } 
                        //recebe ack do equipamento
                        size_t count = recv(cdata->csock, ack, STR_MIN + 1, 0);
                        if(count > 0) {
                            //manda RES_LIST para o equipamento que acabou de ser cadastrado
                            memset(res_msg, 0, MSGSZ);
                            strcpy(res_msg, "RES_LIST(");
                            int aux_id = 0;
                            char *str_ids = malloc(STR_MIN);
                            for(int i = 0; i < 30; i++){
                                //verifica quais clients estao cadastrados e adicona seu id na mensagem
                                if(clients[i][0] != 0){
                                    aux_id = clients[i][0];
                                    sprintf(str_ids, "%02d ", aux_id); //parse int->string
                                    strcat(res_msg, str_ids);
                                }
                            }
                            count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);
                            if (count != strlen(res_msg) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        }
                        memset(res_msg, 0, MSGSZ);
                        /*strcpy(res_msg, "REQ_ADDC2P(");
                        strcat(res_msg, str_id);
                        strcat(res_msg, ")");
                        count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                        if (count != strlen(res_msg) + 1) {
                             logexit("erro ao enviar mensagem de resposta");
                        }*/
                    }
                }
            break;

            case REQ_DC:
                token = strtok(NULL, ")"); //token = eq_id a ser deletado
                int aux_id = atoi(token);
                //verifica se existe o id eq_id na base de dados
                for(int i = 0; i < 30; i++){
                    if(clients[i][0] == aux_id){
                        aux_verify = i;
                        break;
                    }   
                }
                if(aux_verify == 9999){
                    build_error_msg(res_msg, 4); //se sim retorna ERROR(01)
                    //Envia a mensagem de resposta
                    count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                } else {
                    // verifica se o server ja tem um peer
                    if(aux_peer == 0) { 
                        build_ok_msg(res_msg, 1); //retorna OK(01)
                        count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);//envia para o equipamento a msg
                        if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                        clients[aux_verify][0] = 0;
                        clients[aux_verify][1] = 0;
                        clients[aux_verify][2] = 0;
                        num_eq--;
                        memset(res_msg, 0, MSGSZ);
                        char *str_id = malloc(STR_MIN);
                        sprintf(str_id, "%02d", aux_id); //parse int->string
                        strcpy(res_msg, "REQ_DC(");
                        strcat(res_msg, str_id);
                        strcat(res_msg, ")");
                        printf("Client %s removed\n", str_id);
                        for(int i = 0; i < 30; i++){
                                if(clients[i][0] != 0){
                                    count = send(clients[i][1], res_msg, strlen(res_msg) + 1, 0);
                                    if (count != strlen(res_msg) + 1) {
                                        logexit("erro ao enviar mensagem de resposta");
                                    }
                                }
                            } 
                        close(cdata->csock);
                        pthread_exit(NULL);
                    } else {
                        build_ok_msg(res_msg, 1); //retorna OK(01)
                        count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);//envia para o equipamento a msg
                        if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                        clients[aux_verify][0] = 0;
                        clients[aux_verify][1] = 0;
                        clients[aux_verify][2] = 0;
                        num_eq--;
                        memset(res_msg, 0, MSGSZ);
                        char *str_id = malloc(STR_MIN);
                        sprintf(str_id, "%02d", aux_id); //parse int->string
                        strcpy(res_msg, "REQ_DC(");
                        strcat(res_msg, str_id);
                        strcat(res_msg, ")");
                        printf("Client %s removed\n", str_id);
                        for(int i = 0; i < 30; i++){
                                if(clients[i][0] != 0 && clients[i][2] == 1){
                                    count = send(clients[i][1], res_msg, strlen(res_msg) + 1, 0);
                                    if (count != strlen(res_msg) + 1) {
                                        logexit("erro ao enviar mensagem de resposta");
                                    }
                                }
                            } 
                        memset(res_msg, 0, MSGSZ);
                        /*strcpy(res_msg, "REQ_REMCFP(");
                        strcat(res_msg, str_id);
                        strcat(res_msg, ")");
                        count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                        if (count != strlen(res_msg) + 1) {
                             logexit("erro ao enviar mensagem de resposta");
                        }*/

                        close(cdata->csock);
                        pthread_exit(NULL);
                    }
                }
                break;
            
            case REQ_LS:
                aux = 0;

                for(int i=0; i<cdata->mrd->nsens; i++){
                    if(cdata->mrd->util[i] > aux){
                        aux = cdata->mrd->util[i];
                        sprintf(res_msg, "RES_LS(local %d sensor %d: %d (%d %d)", peer_id, cdata->mrd->idsens[i], aux, cdata->mrd->pot[i], cdata->mrd->efic[i]);
                    }
                    //printf("%d\n", cdata->mrd->idsens[i]);
                }
                count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                    printf("RES_LS\n");

            break;

            case REQ_LP:
                aux = 0;

                for(int i=0; i<cdata->mrd->nsens; i++){

                        aux += cdata->mrd->util[i];
                        
                    //printf("%d\n", cdata->mrd->idsens[i]);

                }
                sprintf(res_msg, "RES_LP(local %d potency: %d)", peer_id, aux);
                count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                    printf("RES_LP\n");

            break;
            
            case REQ_ES:

                strcpy(res_msg, "REQ_ES(");
                count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }

            break;

            case REQ_EP:

                strcpy(res_msg, "REQ_EP(");
                count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }

            break;

            case REQ_MS:

                strcpy(res_msg, "REQ_MS(");
                count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }

            break;

            case REQ_MN:

                strcpy(res_msg, "REQ_MN(");
                count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }

            break;

            case REQ_INF:
                token = strtok(NULL, ", "); //token = eq_id que pediu info
                int src_id = atoi(token);
                token = strtok(NULL, ")"); //token = eq_id a ser consultado
                int dest_id = atoi(token);
                //checa o id de origem esta na base de dados
                for(int i = 0; i < 30; i++){
                    if(clients[i][0] == src_id){
                        aux_verify = i;
                        break;
                    }   
                }
                if(aux_verify == 9999){
                    printf("Client %02d not found\n", src_id);
                    memset(res_msg, 0, MSGSZ);
                    build_error_msg(res_msg, 2); // se nao retorna o ERROR(02)
                    count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);//envia para o equipamento a msg
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                } else {
                    aux_verify = 9999;
                    for(int i = 0; i < 30; i++){
                        if(clients[i][0] == dest_id){
                            aux_verify = i;
                            break;
                        }   
                    }
                    if(aux_verify == 9999){
                        printf("Client %02d not found\n", dest_id);
                        memset(res_msg, 0, MSGSZ);
                        build_error_msg(res_msg, 4); // se nao retorna o ERROR(02)
                        count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);//envia para o equipamento a msg
                        if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                    } else {
                        if(clients[aux_verify][2] == 1) {
                            memset(res_msg, 0, MSGSZ);
                            char *str_id = malloc(STR_MIN);
                            sprintf(str_id, "%02d,", src_id); //parse int->string
                            strcpy(res_msg, "REQ_INF(");
                            strcat(res_msg, str_id);
		                    sprintf(str_id, " %02d)", dest_id); //parse int->string
                            strcat(res_msg, str_id);
                            count = send(clients[aux_verify][1], res_msg, strlen(res_msg) + 1, 0);
                            if (count != strlen(res_msg) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        } else {
                            memset(res_msg, 0, MSGSZ);
                            char *str_id = malloc(STR_MIN);
                            sprintf(str_id, "%02d,", src_id); //parse int->string
                            strcpy(res_msg, "REQ_INF(");
                            strcat(res_msg, str_id);
		                    sprintf(str_id, " %02d)", dest_id); //parse int->string
                            strcat(res_msg, str_id);
                            count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                            if (count != strlen(res_msg) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        }
                    }
                }
            break;     
            case RES_INF:
                token = strtok(NULL, ", "); //id doe eq que enviou a resposta
                dest_id = atoi(token);
                token = strtok(NULL, ", "); //id do equipamento que requisitou a info
                src_id = atoi(token);
                token = strtok(NULL, ")"); //valor da resposta
                float value = atof(token);

                for(int i = 0; i < 30; i++){
                    if(clients[i][0] == dest_id){
                        aux_verify = i;
                        break;
                    }   
                }
                if(aux_verify == 9999){
                    printf("Client %02d not found\n", dest_id);
                    memset(res_msg, 0, MSGSZ);
                    build_error_msg(res_msg, 2); // se nao retorna o ERROR(02)
                    count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);//envia para o equipamento a msg
                    if (count != strlen(res_msg) + 1) {
                        logexit("erro ao enviar mensagem de resposta");
                    }
                } else {
                    aux_verify = 9999;
                    for(int i = 0; i < 30; i++){
                        if(clients[i][0] == src_id){
                            aux_verify = i;
                            break;
                        }   
                    }
                    if(aux_verify == 9999){
                        printf("Client %02d not found\n", src_id);
                        memset(res_msg, 0, MSGSZ);
                        build_error_msg(res_msg, 4); // se nao retorna o ERROR(02)
                        count = send(cdata->csock, res_msg, strlen(res_msg) + 1, 0);//envia para o equipamento a msg
                        if (count != strlen(res_msg) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                    } else {
                        if(clients[aux_verify][2] == 1) {
                            memset(res_msg, 0, MSGSZ);
                            char *str_id = malloc(STR_MIN);
                            strcpy(res_msg, "RES_INF(");
                            sprintf(str_id, "%02d,", dest_id); //parse int->string
                            strcat(res_msg, str_id);
		                    sprintf(str_id, " %02d,", src_id); //parse int->string
                            strcat(res_msg, str_id);
                            sprintf(str_id, " %.2f)", value); 
				            strcat(res_msg, str_id); //value
                            count = send(clients[aux_verify][1], res_msg, strlen(res_msg) + 1, 0);
                            if (count != strlen(res_msg) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        } else {
                            memset(res_msg, 0, MSGSZ);
                            char *str_id = malloc(STR_MIN);
                            strcpy(res_msg, "RES_INF(");
                            sprintf(str_id, "%02d,", dest_id); //parse int->string
                            strcat(res_msg, str_id);
		                    sprintf(str_id, " %02d,", src_id); //parse int->string
                            strcat(res_msg, str_id);
                            sprintf(str_id, " %.2f)", value); 
				            strcat(res_msg, str_id); //value
                            count = send(peer_socket, res_msg, strlen(res_msg) + 1, 0);
                            if (count != strlen(res_msg) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        }
                    }
                }
            break;
            default:
                            
            break;      
        }
    }
}

void process_command(char *comando, char *msg_out){  
    char buf[BUFSZ];
	memset(buf, 0, BUFSZ);
	char *comando1 = strtok(comando, "\n");
    char *token = strtok(comando1, " ");
	if(!strcmp(token, "kill")){
        if(aux_peer == 0) {
            printf("no peer connected to close connection\n");
        } else {
            char *str_id = malloc(STR_MIN);
            sprintf(str_id, "%02d", my_id); //parse int->string
		    strcpy(buf, "REQ_DCPEER(");
		    strcat(buf, str_id);
            strcat(buf, ")");
		    strcpy(msg_out, buf); //coloca o comando em msg_out
        }
	}
	else if(!strcmp(comando, "list")){
		strcpy(buf, "This cluster: ");
		char *str_id = malloc(STR_MIN);

		for(int i = 0; i < 30; i++){
			if(clients[i][0] != 0 && clients[i][2] == 1){
        		sprintf(str_id, "%02d ", clients[i][0]); //parse int->string
				strcat(buf, str_id);
			}
			strcpy(msg_out, "controle");
		}
        if(aux_peer == 1) {
            strcat(buf, " Peer's cluster: ");
            for(int i = 0; i < 30; i++){
                if(clients[i][0] != 0 && clients[i][2] == 2){
                    sprintf(str_id, "%02d ", clients[i][0]); //parse int->string
                    strcat(buf, str_id);
                }
                strcpy(msg_out, "controle");
            }
        }
		printf("%s\n", buf);	
	}
}

void *get_command(void *data){
	struct sdata *dados = (struct sdata*) data;
	
	char comando[BUFSZ];
	char msg_out[BUFSZ];

    while(fgets(comando, BUFSZ, stdin)) {
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
        usage(argc, argv);
    }

    // Obtém o tempo atual como semente para srand
    unsigned int seed = (unsigned int)time(NULL);

    // Inicializa o gerador de números aleatórios com a semente
    srand(seed);
    
    struct smartgrid smart = {
        
        .nsens = 0,
        .peer = &peer_id,
        .idsens = {0},
        .pot = {0},
        .efic = {0},
        .util = {0}
    };
    smart.nsens = rand() % 8 + 3;
    for(int i=0; i<smart.nsens; i++){
        smart.idsens[i] = i;
        smart.pot[i] = rand() % 1501;
        smart.efic[i] = rand() % 101;
        smart.util[i] = smart.pot[i] * smart.efic[i] * 0.01;
        //printf("%d\n", smart.idsens[i]);
    }


    //Todos os clients que se conectam tem seu endereco salvo num vetor, o qual eh inicializado com 0 quando o servidor eh inicializado
    for (int i = 0; i < 30; i++){
        for(int j = 0; i < 3; i++) {
            clients[i][j] = 0;
        }
    }

    struct sockaddr_storage storageS;
    if (0 != addrparse(argv[1], argv[2], &storageS)) {
        usage(argc, argv);
    }
    // inicializa o socket responsavel pela conexao entre servidores
    int ss;
    ss = socket(storageS.ss_family, SOCK_STREAM, 0);
    if (ss == -1) {
        logexit("erro ao inicializar socket");
    }

    struct sockaddr_storage storageC;
    if (0 != addrparse(argv[1], argv[3], &storageC)) {
        usage(argc, argv);
    }

    // inicializa o socket responsavel pela conexao cliente-servidor
    int sc;
    sc = socket(storageC.ss_family, SOCK_STREAM, 0);
    if (sc == -1) {
        logexit("erro ao inicializar socket");
    }

    //reutiliza a mesma porta em duas execucoes consecutivas 
    int enable = 1;
    if (0 != setsockopt(sc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
        logexit("setsockopt");
    }

    struct sockaddr *addrC = (struct sockaddr *)(&storageC);
    // bind atribui o endereço ao socket
    if (0 != bind(sc, addrC, sizeof(storageC))) {
        logexit("erro no bind");
    }
 
    // Espera conexao do equipamento (cliente)
    // Recebe o socket e o maximo de conexoes pendentes possiveis para aquele socket (15)
    if (0 != listen(sc, 15)) {
        logexit("erro no listen");
    }
    char addrstr[BUFSZ];
    addrtostr(addrC, addrstr, BUFSZ);

    // Salva o endereco de storage em addr
    struct sockaddr *addrS = (struct sockaddr *)(&storageS);
    socklen_t addr_lenS = sizeof(storageS);
    
    if(connect(ss, addrS, sizeof(storageS)) == 0){
        //Manda REQ_ADDPEER ao inicializar o server
        char *buf = malloc(BUFSZ);
	    strcpy(buf, "REQ_ADDPEER()");
	    ssize_t count = send(ss, buf, strlen(buf) + 1, 0);
        //peer_socket = ss;
	    if (count != strlen(buf) + 1) {
	        logexit("erro ao enviar mensagem");
        }
    } else {
        psv = 1;
        printf("No peer found, starting to listen..\n");
        if (0 != setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) {
            logexit("setsockopt");
        }
        // bind atribui o endereço ao socket
        if (0 != bind(ss, addrS, sizeof(storageS))) {
            logexit("erro no bind");
        }
        // Espera conexao de outro servidor
        if (0 != listen(ss, 15)) {
            logexit("erro no listen");
        }
        addrtostr(addrS, addrstr, BUFSZ);
    }

    fd_set read_fds;
    int max_sd;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sc, &read_fds);
        FD_SET(ss, &read_fds);

        if(sc > ss) max_sd = sc;
        else max_sd = ss;

        int activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
        if(activity < 0) {
            printf("select error\n");
        }

        //caso haja atividade no socket que faz a conexao com os clientes
        if(FD_ISSET(sc, &read_fds)) {
            // Aceita conexao do cliente
            struct sockaddr_storage cstorage;
            struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
            socklen_t caddrlen = sizeof(cstorage);

            // A funcao accept aceita a conexao pelo socket sc e cria outro socket pra comunicar com o cliente 
            int csock = accept(sc, caddr, &caddrlen);
            if (csock == -1) {
                logexit("erro ao conectar com o cliente");
            }

            //Inicializa o client_data e armazena as informacoes do csock e do storage
            struct client_data *cdata = malloc(sizeof(*cdata));
            cdata->mrd = &smart;
            

            if (!cdata) {
                logexit("malloc");
            }
            cdata->csock = csock;
            memcpy(&(cdata->storage), &cstorage, sizeof(cstorage));

            //Dispara thread para tratar a conexao do cliente que chegou
            pthread_t tid;
            pthread_create(&tid, NULL, client_thread, cdata);
        }

        //caso haja atividade no socket que faz a conexao com outro servidor 
        if(FD_ISSET(ss, &read_fds)) {
            if(psv == 1) {
                struct sockaddr_storage sstorage;
                struct sockaddr *saddr = (struct sockaddr *)(&sstorage);
                socklen_t saddrlen = sizeof(sstorage);

                // A funcao accept aceita a conexao pelo socket s e cria outro socket pra comunicar com o novo servidor 
                int ssock = accept(ss, saddr, &saddrlen);
                if (ssock == -1) {
                    logexit("erro ao conectar com o peer");
                }
                peer_socket = ssock;
                //Inicializa o client_data e armazena as informacoes do csock e do storage
                struct server_data *sdata = malloc(sizeof(*sdata)); //passa socket do servidor e dados do endereco
                sdata->ssock = ssock;
                sdata->mrd = &smart;
                memcpy(&(sdata->storage), &sstorage, sizeof(sstorage));


                struct sdata *senddata = malloc(sizeof(senddata)); //passa socket do servidor e dados do endereco
	            senddata->ssock = ssock;
	            senddata->saddr = addrS;
	            senddata->saddrlen = addr_lenS;

                //Cria thread com loop para pegar comando do teclado e enviar mensagem resultante ao servidor 
                pthread_t thread_command;
	            if(0 != pthread_create(&thread_command, NULL, get_command, senddata)){
	                logexit("erro ao criar thread");
                }

                //Dispara thread para tratar a conexao do servidor que chegou
                pthread_t tid2;
                pthread_create(&tid2, NULL, server_thread, sdata);
            } else {
                peer_socket = ss;
                char *buf = malloc(BUFSZ);
                ssize_t count = recv(ss, buf, BUFSZ - 1, 0);

                struct sdata *senddata = malloc(sizeof(senddata)); //passa socket do servidor e dados do endereco
	            senddata->ssock = ss;
	            senddata->saddr = addrS;
	            senddata->saddrlen = addr_lenS;

                //Cria thread com loop para pegar comando do teclado e enviar mensagem resultante ao servidor 
                pthread_t thread_command2;
	            if(0 != pthread_create(&thread_command2, NULL, get_command, senddata)){
	                logexit("erro ao criar thread");
                }

                if(count < 0){
                   logexit("erro ao receber mensagem do servidor");
                }

                char preset[MSGSZ];
                strcpy(preset, buf);

                char *token = strtok(buf, "("); //token = type
                unsigned type = parse_msg_type(token);
                switch (type){
                    case OK:
                        token = strtok(NULL, ")"); //token = codigo do erro
                        unsigned code = atoi(token);
                        switch (code){
                            case 1:
                                printf("Successful disconnect\n"); 
                                printf("Peer %02d disconnected \n", peer_id);
                                peer_id = 0;
                                peer_socket = 0;
                                aux_peer = 0;
                                for(int i = 0; i < 30; i++){
                                    if(clients[i][0] != 0 && clients[i][2] == 2){
                                        int aux_id = clients[i][0];
                                        clients[i][0] = 0;
                                        clients[i][1] = 0;
                                        clients[i][2] = 0;
                                        char *str_id = malloc(STR_MIN);
                                        buf = malloc(BUFSZ);
                                        strcpy(buf, "REQ_DC(");
                                        sprintf(str_id, "%02d)", aux_id); //parse int->string
                                        strcat(buf, str_id);
                                        for(int j = 0; j < 30; j++){
                                            if(clients[j][0] != 0 && clients[j][2] == 1){
                                                count = send(clients[j][1], buf, strlen(buf) + 1, 0);
                                                if (count != strlen(buf) + 1) {
                                                    logexit("erro ao enviar mensagem de resposta");
                                                }        
                                            }
                                        }
                                    }
                                }
                            break;
                        }
                    break;
                    case ERROR:
                        token = strtok(NULL, ")"); //token = codigo do erro
                        code = atoi(token);
                        switch (code){  
                            case 2:
                                printf("Peer limit exceeded\n");
                            break;
                            case 3:
                                printf("Peer not found\n");
                            break;

                            default:
                        
                            break;
                        }
                    break;

                    case RES_ADD:
                        token = strtok(NULL, ")"); //token = id do peer
                        int rcvdid = atoi(token);
                        printf("New Peer ID: %02u \n", rcvdid); //imprime o id do peer
                        aux_peer = 1;
                        my_id = rcvdid;
                        int num = rcvdid + 1; 
                        peer_id = num;
                        printf("Peer %02u connected\n", peer_id); //imprime o id do peer conectado
                        char *str_id = malloc(STR_MIN);
                        sprintf(str_id, "%02d", peer_id); //parse int->string
                        strcpy(buf, "RES_ADD(");
                        strcat(buf, str_id);
                        strcat(buf, ")");
                        count = send(ss, buf, strlen(buf) + 1, 0);
                        if (count != strlen(buf) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                    break;     
                        
                    case RES_LIST:
                         //recebe RES_LIST(<id1> <id2> ...  e adiciona todos os ids no vetor de quipamentos do servidor
                        //token = eq_id
                        while((token = strtok(NULL, " ")) != NULL){
                            //adiciona todos os ids listados nas posicoes corretas do vetor
                            int id_received = atoi(token);
                            clients[id_received - 1][0] = id_received;
                              clients[id_received - 1][2] = 2;                          
                        }  
                    break;
                    case REQ_ADDC2P:
                        token = strtok(NULL, ")"); //token = id do Cliento que acabou de ser adicionado
                        int aux_id = atoi(token);
                        clients[aux_id - 1][0] = aux_id;
                        clients[aux_id - 1][2] = 2;
                        printf("Client %02u added \n", aux_id);
                        str_id = malloc(STR_MIN);
                        sprintf(str_id, "%02d", aux_id); //parse int->string
                        strcpy(buf, "RES_ADD(");
                        strcat(buf, str_id);
                        strcat(buf, ")");
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] != 0 && clients[i][2] == 1){
                                count = send(clients[i][1], buf, strlen(buf) + 1, 0);
                                if (count != strlen(buf) + 1) {
                                    logexit("erro ao enviar mensagem de resposta");
                                }
                            }
                        } 
                    break;
                    case REQ_REMCFP:
                        token = strtok(NULL, ")"); //token = id do Cliento que acabou de ser adicionado
                        aux_id = atoi(token);
                        clients[aux_id - 1][0] = 0;
                        clients[aux_id - 1][2] = 0;
                        printf("Client %02u removed \n", aux_id);
                        str_id = malloc(STR_MIN);
                        sprintf(str_id, "%02d", aux_id); //parse int->string
                        strcpy(buf, "REQ_DC(");
                        strcat(buf, str_id);
                        strcat(buf, ")");
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] != 0 && clients[i][2] == 1){
                                count = send(clients[i][1], buf, strlen(buf) + 1, 0);
                                if (count != strlen(buf) + 1) {
                                    logexit("erro ao enviar mensagem de resposta");
                                }
                            }
                        } 
                    break;
                    case REQ_DCPEER:
                        token = strtok(NULL, ")"); //token = id do peer a ser removido
                        aux_id = atoi(token);
                        //verifica o id recebido é igual ao id do peer que esta atualmente conectado
                        if(aux_id != peer_id) {
                            buf = malloc(BUFSZ);
                            build_error_msg(buf, 3);
                            //Envia a mensagem de resposta
                            count = send(ss, buf, strlen(buf) + 1, 0);
                            if (count != strlen(buf) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        } else {
                            buf = malloc(BUFSZ);
                            build_ok_msg(buf, 1);
                            count = send(ss, buf, strlen(buf) + 1, 0);
                            if (count != strlen(buf) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                            printf("Peer %02d disconnected \n", aux_id);
                            peer_id = 0; //remove o id do peer da base de dados
                            peer_socket = 0; //remove o socket do peer da base de dados
                            aux_peer = 0;

                            for(int i = 0; i < 30; i++){
                                    if(clients[i][0] != 0 && clients[i][2] == 2){
                                    aux_id = clients[i][0];
                                    clients[i][0] = 0;
                                    clients[i][1] = 0;
                                    clients[i][2] = 0;
                                    char *str_id = malloc(STR_MIN);
                                    buf = malloc(BUFSZ);
                                    strcpy(buf, "REQ_DC(");
                                    sprintf(str_id, "%02d)", aux_id); //parse int->string
                                    strcat(buf, str_id);
                                    for(int j = 0; j < 30; j++){
                                        if(clients[j][0] != 0 && clients[j][2] == 1){
                                            count = send(clients[j][1], buf, strlen(buf) + 1, 0);
                                            if (count != strlen(buf) + 1) {
                                                logexit("erro ao enviar mensagem de resposta");
                                            }                
                                        }
                                    }
                                }
                            }
                        }
                    break;
                    case REQ_INF:
                        token = strtok(NULL, ", "); //token = eq_id que pediu info
                        int src_id = atoi(token);
                        token = strtok(NULL, ")"); //token = eq_id a ser consultado
                        int dest_id = atoi(token);
                        //checa a posicao do id de destino no banco de dados
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] == dest_id){
                                aux_verify = i;
                                break;
                            }   
                        }
                        if(clients[aux_verify][2] == 1) {
                            buf = malloc(BUFSZ);
                            char *str_id = malloc(STR_MIN);
                            sprintf(str_id, "%02d,", src_id); //parse int->string
                            strcpy(buf, "REQ_INF(");
                            strcat(buf, str_id);
                            sprintf(str_id, " %02d)", dest_id); //parse int->string
                            strcat(buf, str_id);
                            count = send(clients[aux_verify][1], buf, strlen(buf) + 1, 0);
                            if (count != strlen(buf) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        } else {
                            buf = malloc(BUFSZ);
                            char *str_id = malloc(STR_MIN);
                            sprintf(str_id, "%02d,", src_id); //parse int->string
                            strcpy(buf, "REQ_INF(");
                            strcat(buf, str_id);
                            sprintf(str_id, " %02d)", dest_id); //parse int->string
                            strcat(buf, str_id);
                            count = send(peer_socket, buf, strlen(buf) + 1, 0);
                            if (count != strlen(buf) + 1) {
                                logexit("erro ao enviar mensagem de resposta");
                            }
                        }                       
                    break;     
                    case RES_INF:
                        token = strtok(NULL, ", "); //id do eq que enviou a resposta
                        dest_id = atoi(token);
                        token = strtok(NULL, ", "); //id do cliente que requisitou a info
                        src_id = atoi(token);
                        token = strtok(NULL, ")"); //valor da resposta
                        float value = atof(token);
                    
                        for(int i = 0; i < 30; i++){
                            if(clients[i][0] == src_id){
                                aux_verify = i;
                                break;
                            }   
                        }
                        if(clients[aux_verify][2] == 1) {
                                memset(buf, 0, MSGSZ);
                                char *str_id = malloc(STR_MIN);
                                strcpy(buf, "RES_INF(");
                                sprintf(str_id, "%02d,", dest_id); //parse int->string
                                strcat(buf, str_id);
                                sprintf(str_id, " %02d,", src_id); //parse int->string
                                strcat(buf, str_id);
                                sprintf(str_id, " %.2f)", value); 
                                strcat(buf, str_id); //value
                                count = send(clients[aux_verify][1], buf, strlen(buf) + 1, 0);
                                if (count != strlen(buf) + 1) {
                                    logexit("erro ao enviar mensagem de resposta");
                                }
                            } else {
                                buf = malloc(BUFSZ);
                                char *str_id = malloc(STR_MIN);
                                strcpy(buf, "RES_INF(");
                                sprintf(str_id, "%02d,", dest_id); //parse int->string
                                strcat(buf, str_id);
                                sprintf(str_id, " %02d,", src_id); //parse int->string
                                strcat(buf, str_id);
                                sprintf(str_id, " %.2f)", value); 
                                strcat(buf, str_id); //value
                                count = send(peer_socket, buf, strlen(buf) + 1, 0);
                                if (count != strlen(buf) + 1) {
                                    logexit("erro ao enviar mensagem de resposta");
                                }
                            }            
                    break;


                    case REQ_ES:

            
                        aux = 0;

                        for(int i=0; i<smart.nsens; i++){
                            if(smart.util[i] > aux){
                            aux = smart.util[i];
                            sprintf(buf, "RES_ES(external %d sensor %d: %d (%d %d)", peer_id, smart.idsens[i], aux, smart.pot[i], smart.efic[i]);
                        }
                        //printf("%d\n", cdata->mrd->idsens[i]);
                        }
                        count = send(peer_socket, buf, strlen(buf) + 1, 0);
                        if (count != strlen(buf) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                        printf("REQ_ES\n");
                        printf("RES_ES\n");



                    break;


                    case RES_ES:

                        token = strtok(NULL, ")");
                        printf("%s)\n", token);
                        count = send(sockcl, preset, strlen(preset) + 1, 0);
                        if (count != strlen(preset) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                    

                    break;

                    case REQ_EP:

            
                        aux = 0;

                        for(int i=0; i<smart.nsens; i++){

                            aux += smart.util[i];
                        
                            //printf("%d\n", cdata->mrd->idsens[i]);

                        }
                        sprintf(buf, "RES_EP(external %d potency: %d)", peer_id, aux);
                        count = send(peer_socket, buf, strlen(buf) + 1, 0);
                        if (count != strlen(buf) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                        printf("REQ_EP\n");
                        printf("RES_EP\n");


                    break;

            
                    case RES_EP:

                        token = strtok(NULL, ")");
                        printf("%s\n", token);
                        count = send(sockcl, preset, strlen(preset) + 1, 0);
                        if (count != strlen(preset) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }

                    break;


                    case REQ_MS:

            
                        aux = 0;

                        for(int i=0; i<smart.nsens; i++){
                            if(smart.util[i] > aux){
                            aux = smart.util[i];
                            sprintf(buf, "RES_MS(%d %d %d %d %d)", peer_id, smart.idsens[i], aux, smart.pot[i], smart.efic[i]);
                        }
                        //printf("%d\n", cdata->mrd->idsens[i]);
                        }
                        count = send(peer_socket, buf, strlen(buf) + 1, 0);
                        if (count != strlen(buf) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }
                        printf("REQ_ES\n");
                        printf("RES_ES\n");



                    break;


                    case RES_MS:

                        token = strtok(NULL, ") "); 
                        int local = atoi(token);
                        token = strtok(NULL, " "); 
                        int sensor = atoi(token);
                        token = strtok(NULL, " "); 
                        int util = atoi(token);
                        token = strtok(NULL, " "); 
                        int pote = atoi(token);
                        token = strtok(NULL, " "); 
                        int efici = atoi(token);

                        int auxut = 0;
                        int auxid = 0;
                        int auxpot = 0;
                        int auxefic = 0;

                        for(int i=0; i<smart.nsens; i++){
                            if(smart.util[i] > auxut){
                            auxut = smart.util[i];
                            auxid = smart.idsens[i];
                            auxpot = smart.pot[i];
                            auxefic = smart.efic[i];
                            }
                        }

                        if(auxut > util){
                            sprintf(buf, "RES_MS(global %d sensor %d: %d (%d %d)", peer_id, auxid, auxut, auxpot, auxefic);
                        }
                        else{
                            sprintf(buf, "RES_MS(global %d sensor %d: %d (%d %d)", local, sensor, util, pote, efici);
                        }

                        count = send(sockcl, buf, strlen(buf) + 1, 0);
                        if (count != strlen(buf) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }

                        token = strtok(buf, "(");
                        token = strtok(NULL, ")");

                        printf("%s)\n", token);
                    

                    break;


                    case REQ_MN:

                        aux = 0;

                        for(int i=0; i<smart.nsens; i++){

                            aux += smart.util[i];
        
                        }

                        sprintf(buf, "RES_MN(%d %d)", peer_id, aux);
                        count = send(peer_socket, buf, strlen(buf) + 1, 0);
                            if (count != strlen(buf) + 1) {
                              logexit("erro ao enviar mensagem de resposta");
                            }
                        printf("REQ_EP\n");    
                        printf("RES_EP\n");

                    break;


                    case RES_MN:

                    token = strtok(NULL, ") "); 
                    local = atoi(token);
                    token = strtok(NULL, " "); 
                    util = atoi(token);

                    auxut = 0;

                    for(int i=0; i<smart.nsens; i++){
            
                        auxut += smart.util[i];
                            
                    }

                    if(auxut > util){
                        sprintf(buf, "RES_MN(global %d potency: %d", peer_id, auxut);
                    }
                    else{
                        sprintf(buf, "RES_MN(global %d potency: %d", local, util);
                    }

                        count = send(sockcl, buf, strlen(buf) + 1, 0);
                        if (count != strlen(buf) + 1) {
                            logexit("erro ao enviar mensagem de resposta");
                        }

                        token = strtok(buf, "(");
                        token = strtok(NULL, ")");

                        printf("%s\n", token);

                    break;




                }
                free(buf);
                buf = NULL;
                token = NULL;
            }
        }
    }
    exit(EXIT_SUCCESS);
}


