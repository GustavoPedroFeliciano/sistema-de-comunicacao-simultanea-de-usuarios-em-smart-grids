# Trabalho Prático — Sistema de Comunicação Simultânea de Usuários em Smart Grids

**Execução:** Individual  

## 🧠 Introdução

Este trabalho propõe o desenvolvimento de um sistema para comunicação simultânea entre usuários em diferentes redes Smart Grids. A arquitetura envolve:
- **Servidores (MTUs)**: responsáveis pela supervisão de redes smart grids.
- **Clientes (HMIs)**: responsáveis por consultar e interagir com os dados dos sensores via rede.

Os servidores operam de forma **peer-to-peer** (P2P) e devem permitir **múltiplas conexões de clientes** via sockets POSIX em C.

## 📡 Protocolo

- Comunicação via **TCP/IP**
- Apenas **letras, números e espaços** (ASCII)
- Tamanho máximo da mensagem: **500 bytes**
- Sem acentuação ou caracteres especiais

## ✉️ Especificação das Mensagens

### Controle (servidor-servidor e cliente-servidor)
| Tipo       | Payload   | Descrição |
|------------|-----------|-----------|
| `REQ_ADDPEER` | –         | Solicita conexão P2P entre servidores |
| `REQ_DCPEER`  | PidMi     | Solicita desconexão entre servidores |
| `RES_ADDPEER` | PidMi     | Confirma conexão P2P |
| `REQ_ADD`     | –         | Solicita entrada de cliente na rede |
| `REQ_DC`      | IdCi      | Solicita saída de cliente |
| `RES_ADD`     | IdCi      | Confirma entrada de cliente |

### Dados (consultas)
| Tipo     | Payload | Descrição |
|----------|---------|-----------|
| `REQ_LS` | –       | Sensor local com maior potência útil |
| `REQ_ES` | –       | Sensor externo com maior potência útil |
| `REQ_LP` | –       | Potência útil da rede local |
| `REQ_EP` | –       | Potência útil da rede externa |
| `REQ_MS` | –       | Sensor com maior potência útil em ambas redes |
| `REQ_MN` | –       | Rede com maior potência útil |
| `RES_LS` | Dados   | Resposta sensor local |
| `RES_ES` | Dados   | Resposta sensor externo |
| `RES_LP` | Dados   | Resposta potência local |
| `RES_EP` | Dados   | Resposta potência externa |
| `RES_MS` | Dados   | Resposta sensor global |
| `RES_MN` | Dados   | Resposta rede com maior potência |

### Erro/Confirmação
| Tipo   | Código | Descrição |
|--------|--------|-----------|
| `ERROR` | 01-04  | Erros diversos: limite de clientes, peer não encontrado, etc. |
| `OK`    | 01     | Desconexão bem-sucedida |

## 🔁 Funcionalidades

1. **Consultar sensor local**  
   Comando: `show localmaxsensor` → `REQ_LS`

2. **Consultar sensor externo**  
   Comando: `show externalmaxsensor` → `REQ_ES`

3. **Potência útil local**  
   Comando: `show localpotency` → `REQ_LP`

4. **Potência útil externa**  
   Comando: `show externalpotency` → `REQ_EP`

5. **Sensor com maior potência global**  
   Comando: `show globalmaxsensor` → `REQ_MS`

6. **Rede com maior potência útil**  
   Comando: `show globalmaxnetwork` → `REQ_MN`

## 🛠️ Implementação

- Linguagem: **C**
- Comunicação: **Sockets TCP POSIX**
- Utilização de `select()` para múltiplas conexões no servidor
- Até **10 clientes por servidor**
- Os servidores operam com **dois sockets** (um para P2P e outro para clientes)

### Execução

```bash
# Servidor 1
./server 127.0.0.1 90900 90100

# Servidor 2
./server 127.0.0.1 90900 90200

# Cliente 1
./client 127.0.0.1 90100

# Cliente 2
./client 127.0.0.1 90200
