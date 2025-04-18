#include "common.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>


// - Funcao para tratar erros - //
void logexit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// - Logica de Mensagens - // 
//Funcao que pega uma string com o tipo da mensagem e devolve uma variavel do enum
unsigned parse_msg_type(const char *msg_type_in){
    if(!strcmp(msg_type_in, "REQ_ADDPEER"))
        return REQ_ADDPEER;
    else if (!strcmp(msg_type_in, "REQ_DCPEER"))
        return REQ_DCPEER;
    else if (!strcmp(msg_type_in, "REQ_ADD"))
        return REQ_ADD;
    else if (!strcmp(msg_type_in, "REQ_DC"))
        return REQ_DC;
    else if (!strcmp(msg_type_in, "RES_ADD"))
        return RES_ADD;
     else if (!strcmp(msg_type_in, "RES_ADDPEER"))
        return RES_ADDPEER;
     else if (!strcmp(msg_type_in, "REQ_LS"))
        return REQ_LS;
    else if (!strcmp(msg_type_in, "REQ_ES"))
        return REQ_ES;
    else if (!strcmp(msg_type_in, "REQ_LP"))
        return REQ_LP;
    else if (!strcmp(msg_type_in, "REQ_EP"))
        return REQ_EP;
    else if (!strcmp(msg_type_in, "REQ_MS"))
        return REQ_MS;
    else if (!strcmp(msg_type_in, "REQ_MN"))
        return REQ_MN;
    else if (!strcmp(msg_type_in, "RES_LS"))
        return RES_LS;
    else if (!strcmp(msg_type_in, "RES_ES"))
        return RES_ES;
    else if (!strcmp(msg_type_in, "RES_LP"))
        return RES_LP;
    else if (!strcmp(msg_type_in, "RES_EP"))
        return RES_EP;
    else if (!strcmp(msg_type_in, "RES_MS"))
        return RES_MS;
    else if (!strcmp(msg_type_in, "RES_MN"))
        return RES_MN; 
    else if (!strcmp(msg_type_in, "ERROR"))
        return ERROR;
    else if (!strcmp(msg_type_in, "OK"))
        return OK;
    
    
    else if (!strcmp(msg_type_in, "REQ_ADDC2P"))
        return REQ_ADDC2P;
    else if (!strcmp(msg_type_in, "REQ_REMCFP"))
        return REQ_REMCFP;
    else if (!strcmp(msg_type_in, "RES_LIST"))
        return RES_LIST;
    else if (!strcmp(msg_type_in, "REQ_INF"))
        return REQ_INF; 
    else if (!strcmp(msg_type_in, "RES_INF"))
        return RES_INF;   
    else 
        return 0;
}



// - Definicao de enderecos IPV4 // 

//Funcao que passa os enderecos recebidos na chamada do programa para a estrutura do POSIX que guarda os enderecos (sockaddr_storage)
//se tiver erro retorna -1, se nao retorna 0
int addrparse(const char *rcvd_ip, const char *rcvd_port, struct sockaddr_storage *storage){
    if(rcvd_ip == NULL || rcvd_port == NULL){
        return -1;
    }

    //salva a porta como um int
    uint16_t port = (uint16_t)atoi(rcvd_port); 
    if(port == 0){
        return -1;
    }

    // funcao host to network transforma o int port da ordem de bytes do host para ordem de bytes da rede
    port = htons(port); 

    //Para um endereco IPv4
    struct in_addr inaddr4;     //IPv4
    if(inet_pton(AF_INET, rcvd_ip, &inaddr4)) {
        // Salva IPv4 e port no storage
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr = inaddr4;
        return 0;
    }
    return -1;
}

//Funcao que transforma o endereco, que esta em uma estrutura sockaddr, em uma string
void addrtostr(const struct sockaddr *addr, char *str, size_t strsize) {
    uint16_t port;
    char addrstr[INET_ADDRSTRLEN + 1] = " ";
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        //inet_ntop transforma o endereco em formato sockaddr pra string
        if (!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr, INET_ADDRSTRLEN + 1)) {
            logexit("erro ao converter ntop");
        }
        port = ntohs(addr4->sin_port); // network to host short, transforma a endianidade da rede para do sistema
    } else {
        logexit("familia de IP desconhecida");
    }
    //imprime no terminal o endereco
    if (str) {
        snprintf(str, strsize, "IPv4 %s %hu", addrstr, port);
    }
}
