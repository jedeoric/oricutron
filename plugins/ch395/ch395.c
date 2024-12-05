#include "ch395.h"
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>



#include "../../system.h"
#include "../../6502.h"
#include "../../via.h"
#include "../../8912.h"
#include "../../gui.h"
#include "../../disk.h"
#include "../../monitor.h"
#include "../../6551.h"

#include "plugin.h"
#include "../../machine.h"

#define INSTANCE_MAX 2

struct ch395 *userdata[INSTANCE_MAX];
struct ch395 *userdata_old[INSTANCE_MAX];

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
// extern struct textzone *tz[];
// extern struct osdmenu menus[];
// Si Oricutron est compilé sans l'option -rdynamic alors il faut passer la
// référence des fonctions tzprintfpos et tzpoutc au plugin.
#ifndef RDYNAMIC
void (*my_tzprintfpos)( struct textzone *ptz, int x, int y, char *fmt, ... );
void (*my_tzputc)( struct textzone *ptz, char c );
void (*mon_periphmod)( int x, int y, int w, struct textzone *vtz );
#endif



// Pour les fonction de lecture du fichier de configuration
#include "../../main.h"

#if defined(__MORPHOS__) || defined (__AMIGA__) || defined (__AROS__)

#define __NOLIBBASE__

#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

extern struct Library *SysBase;




#elif defined(WIN32)

#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include <sys/stat.h>
//@iss #include <shlwapi.h>

#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__) || defined(__MINT__)

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#ifdef __ANDROID__
#include <sys/vfs.h>
#define statvfs statfs
#else
#include <sys/statvfs.h>
#endif
#include <sys/stat.h>
#include <time.h>



#else
#error "FixMe!"
#endif



#ifdef DEBUG_PLUGIN
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif




#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)

/* /// "POSIX system functions" */

static void * system_alloc_mem(int size)
{
    return malloc(size);
}
#endif

#define BASE_ADDR 0x0360
#define END_ADDR 0x0361


// Nombre d'instances total
unsigned int plugin_instances = 0;

// Description du plugin
static char *description = "CH395";


void ch395_init(struct ch395 *ch395)
{
    //is_init
}
/*
void ch395_set_des_port_sn()
{

}
*/
int perform_connect_socket(struct ch395 *ch395, unsigned char socketid)
{

    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = ch395->sockfd_host[socketid];

    // Configurer l'adresse du serveur
    server_addr.sin_family = AF_INET;

    int port = ch395->socket_dest_port[socketid][0] + ch395->socket_dest_port[socketid][1]*256;
    server_addr.sin_port = htons(port); // Port par défaut pour HTTP
    if (inet_pton(AF_INET, "192.168.1.77", &server_addr.sin_addr) <= 0) {
        perror("Adresse IP invalide");
        return 1;
    }
    //ch395->CommandData
    //server_addr.sin_addr.s_addr =*((unsigned long *)host->h_addr_list[0]);

    // Se connecter au serveur
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur lors de la connexion au serveur");
        return 1;
    }
    return 0;
}


void ch395_init_internal(struct ch395 *ch395)
{
    unsigned char i;
    for(i=0;i<8;i++)
    {
        ch395->socket_state[i] = CH395_SOCKET_CLOSED; // Socket state
        ch395->socket_proto[i] = CH395_TCP_CLOSED; // State protocol
        ch395->socket_int_status[i] = CH395_SINT_STAT_DISCONNECT; //Socket state
        ch395->buffer_position_write_from_data[i] = 0;
        ch395->buffer_position_read_from_data[i] = 0;
        ch395->socket_length_received[i] = 0;
        ch395->socket_length_to_send[i] = 0;
    }

    // Define buffers for 4 sockets, others sockets are not allocated at the start of ch395. It must be user allocated

    // Init socket from 4 to 7
    for(i=4; i<8; i++)
    {
        ch395->receive_buffer_start_block[i] = 0; // Should be NULL
        ch395->transmit_buffer_number_of_block[i] = 0;
        ch395->receive_buffer_number_of_block[i] = 0;
        ch395->transmit_buffer_start_block[i] = 0; // Should be NULL
    }

    for(i=0; i<4; i++)
    {
        ch395->receive_buffer_number_of_block[i] = 8;
        ch395->transmit_buffer_number_of_block[i] = 4;

    }

    ch395->receive_buffer_start_block[0] = 0;
    ch395->receive_buffer_start_block[1] = 12;
    ch395->receive_buffer_start_block[2] = 24;
    ch395->receive_buffer_start_block[3] = 36;

    ch395->transmit_buffer_start_block[0] = 8;
    ch395->transmit_buffer_start_block[1] = 20;
    ch395->transmit_buffer_start_block[2] = 32;
    ch395->transmit_buffer_start_block[3] = 44;

}

//struct ch395 * ch395_create()
int ch395_create(struct machine *oric)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;

    userdata[plugin_instances] = malloc(sizeof(struct ch395));
    userdata_old[plugin_instances] = malloc(sizeof(struct ch395));

    if (userdata[plugin_instances])
    {
        if (userdata_old[plugin_instances] == NULL)
        {
            free(userdata[plugin_instances]);
            return 0;
        }
        // ch395 is not initialized
        userdata[plugin_instances]->is_init = CH395_FALSE;
        userdata[plugin_instances]->phy_state = CH395_PHY_DISCONN;
        // Mac address can be accessed even if ch395 is not init
        userdata[plugin_instances]->mac_address[5] = 0xaa;
        userdata[plugin_instances]->mac_address[4] = 0xbb;
        userdata[plugin_instances]->mac_address[3] = 0xcc;
        userdata[plugin_instances]->mac_address[2] = 0xdd;
        userdata[plugin_instances]->mac_address[1] = 0xee;
        userdata[plugin_instances]->mac_address[0] = 0xff;

        return ++plugin_instances;
    }

    return 0;
    // struct ch395 *ch395 = (struct ch395 *)malloc(sizeof(struct ch395));
    // if(ch395)
    // {
    //     ch395->is_init = CH395_FALSE;
    //     ch395->nb_bytes_in_cmd_data = 15; // ???
    //     ch395_init_internal(ch395);
    // }
    // else
    // {
    //     free(ch395);
    //     ch395 = NULL;
    // }

    // return ch395;
}



// void ch395_oric_reset(struct ch395 *ch395)
// {

// }

// unsigned char 	ch395_oric_read(struct  ch395 *ch395, uint16_t addr)
// {

// }

// void ch395_oric_config(struct ch395 *ch395)
// {

// }

// void ch395_oric_destroy(struct ch395 *ch395)
// {

// }

unsigned char ch395_read_command_port(struct ch395 *ch395)
{
    printf(">>[CH395][READ][COMMAND]\n");
    return 0;
}

unsigned char ch395_read_data_port(struct ch395 *ch395)
{
    unsigned char data = 0xff;
    char *msg;
    char *value;
    msg = malloc(200);
    value = malloc(200);

    // uint8_t data_out = 0xff; // Hi-Z
    //printf(">>[CH395][READ][DATA] %d\n", ch395->command);

    // dbg_printf(">> [READ][DATA] for during command &%02x status &%02x\n", ch395->command, ch395->command_status);

    switch(ch395->command)
    {
        case CH395_CMD_CHECK_EXIST:
            data = ch395->cmd_data.CMD_CheckByte;
            dbg_printf("[CH395][READ][DATA][CH395_CMD_CHECK_EXIST] setting data port to 0x%02x\n", ch395->cmd_data.CMD_CheckByte);
            printf("[CH395][READ][DATA][CH395_CMD_CHECK_EXIST] setting data port to 0x%02x\n", ch395->cmd_data.CMD_CheckByte);
            return data;

         case CH395_CMD_GET_IC_VER:
            printf("<<[CH395][READ][DATA][CH395_CMD_GET_IC_VER]\n");
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_IC_VER]\n");
            data = 70; //VERSION
            break;

        case CH395_CMD_GET_PHY_STATUS:
            printf("<<[CH395][READ][DATA][CH395_CMD_GET_PHY_STATUS]\n");
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_PHY_STATUS]\n");
            // If ch395 is not init, PHY_state is always disconnected
            if (ch395->is_init == CH395_FALSE)
                data = CH395_PHY_DISCONN;
            else
                data = ch395->phy_state;
            break;

        case CH395_CMD_READ_RECV_BUF_SN:
            printf("<<[CH395][READ][DATA][CH395_CMD_READ_RECV_BUF_SN] ");
            dbg_printf("<<[CH395][READ][COMMAND][CH395_CMD_READ_RECV_BUF_SN] ");

            data = ch395->buffer[ch395->pos_rw_in_cmd_data]; // FIXME
            printf("Send byte : %d ",ch395->pos_rw_in_cmd_data);
            dbg_printf("Send byte : %d ",ch395->pos_rw_in_cmd_data);
            switch (data)
            {
                case 0x0a:
                    printf("data : \\n\n");
                    dbg_printf("data : \\n\n");
                case 0x0d:
                    printf("data : \\r\n");
                    dbg_printf("data : \\r\n");
                default:
                    printf("data : %c\n", data);
                    dbg_printf("data : %c\n", data);
                    break;
            }

            ch395->pos_rw_in_cmd_data ++;
            printf("\n");
            break;

        case CH395_CMD_GET_MAC_ADDR:
            if (ch395->nb_bytes_in_cmd_data == 6) {
                print("CH395 panic : impossible to read mac adress more than 6 bytes");
                data = 0;
            }
            else {
                data = ch395->mac_address[ch395->nb_bytes_in_cmd_data];
                ch395->nb_bytes_in_cmd_data++;
            }
            break;

        case CH395_CMD_GET_RECV_LEN_SN:
            printf("<<[CH395][READ][DATA][CH395_CMD_GET_RECV_LEN_SN]");
            dbg_printf("<<[CH395][READ][COMMAND][CH395_CMD_GET_RECV_LEN_SN]");
            if ( ch395->pos_rw_in_cmd_data == 0 )
            {
                data = ch395->buffer_position_receive[ch395->cmd_data.CMD_SocketGetRecvLen[0]] & 0xFF;
                printf("Sending length low %d\n", data);
                dbg_printf("Sending length low %d\n", data);
            }

            if ( ch395->pos_rw_in_cmd_data == 1 )
            {
                data = ch395->buffer_position_receive[ch395->cmd_data.CMD_SocketGetRecvLen[0]] >> 8;
                printf("Sending length high %d\n", data);
                dbg_printf("Sending length high %d\n", data);
            }

            ch395->pos_rw_in_cmd_data ++;
            break;

        case CH395_CMD_GET_SOCKET_STATUS_SN:
            printf("<<[CH395][READ][DATA][CH395_CMD_GET_SOCKET_STATUS_SN]");
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_SOCKET_STATUS_SN]");

            if ( ch395->pos_rw_in_cmd_data == 0 )
            {
                data = ch395->socket_state[ch395->cmd_data.CMD_SocketState[0]];
                if (data == CH395_SOCKET_CLOSED)
                {
                    strcpy(msg," SOCKET_CLOSED");
                }

                if (data == CH395_SOCKET_OPEN)
                {
                    strcpy(msg," SOCKET_OPEN");
                }
                printf("Socket: %d socket State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
                dbg_printf("Socket: %d socket State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
            }

            if ( ch395->pos_rw_in_cmd_data == 1 )
            {
                data = ch395->socket_state[ch395->cmd_data.CMD_SocketState[0]];
                switch(data)
                {
                    case CH395_TCP_CLOSED:
                        strcpy(msg," TCP_CLOSED");
                        break;
                    case CH395_TCP_LISTEN:
                        strcpy(msg," TCP_LISTEN");
                        break;
                    case CH395_TCP_SYN_SENT:
                        strcpy(msg," TCP_SYN_SENT");
                        break;
                    case CH395_TCP_SYN_REVD:
                        strcpy(msg," TCP_SYN_REVD");
                        break;
                    case CH395_TCP_ESTABLISHED:
                        strcpy(msg," TCP_ESTABLISHED");
                        break;
                    case CH395_TCP_FIN_WAIT_1:
                        strcpy(msg," TCP_FIN_WAIT_1");
                        break;
                    case CH395_TCP_FIN_WAIT_2:
                        strcpy(msg," TCP_FIN_WAIT_2");
                        break;
                    case CH395_TCP_CLOSE_WAIT:
                        strcpy(msg," CLOSE_WAIT");
                        break;
                    case CH395_TCP_CLOSING:
                        strcpy(msg," TCP_CLOSING");
                        break;
                    case CH395_TCP_LAST_ACK:
                        strcpy(msg," LAST_ACK");
                        break;
                    case CH395_TCP_TIME_WAIT:
                        strcpy(msg," TCP_TIME_WAIT");
                        break;
                }
                printf("Socket: %d socket protocol State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
                dbg_printf("Socket: %d socket protocol State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
            }
            // Reset states
            ch395->cmd_data.CMD_SocketState[0] = 0;

            ch395->pos_rw_in_cmd_data ++;
            break;



        case CH395_CMD_GET_GLOB_INT_STATUS:
            printf("<<[CH395][READ][DATA][CH395_CMD_GET_GLOB_INT_STATUS] value : %d \n",ch395->glob_int_status);
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_GLOB_INT_STATUS] value : %d \n",ch395->glob_int_status);

            data = ch395->glob_int_status;
            break;

        case CH395_CMD_GET_INT_STATUS_SN:
            unsigned char socket = ch395->cmd_data.CMD_SocketGetIntStatusSn[0];
            data = ch395->socket_int_status[socket];
            // When a socket is not open CH395_CMD_GET_INT_STATUS_SN command returns always 0 for socket state

            sprintf(value, "value : %d", data);
            strcpy(msg,"->");
            strcat(msg,value);

            if (data & CH395_SINT_STAT_TIM_OUT)
            {
                strcat(msg," CH395_SINT_STAT_TIM_OUT");
            }

            if (data & CH395_SINT_STAT_DISCONNECT)
            {
                strcat(msg," CH395_SINT_STAT_DISCONNECT");
            }

            if (data & CH395_SINT_STAT_CONNECT)
            {
                strcat(msg," CH395_SINT_STAT_CONNECT");
            }

            if (data & CH395_SINT_STAT_RECV)
            {
                strcat(msg," CH395_SINT_STAT_RECV");
            }

            if (data & CH395_SINT_STAT_SEND_OK)
            {
                strcat(msg," CH395_SINT_STAT_SEND_OK");
            }

            if (data & CH395_SINT_STAT_SENBUF_FREE)
            {
                strcat(msg," CH395_SINT_STAT_SENBUF_FREE");
            }

            printf("<<[CH395][READ][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d %s\n", socket, msg);
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d %s\n", socket, msg);
            ch395->cmd_data.CMD_SocketGetIntStatusSn[0] = 0; // Reset status
            break;
    }
    free(value);
    free(msg);
    return data;
}

void ch395_write_command_port(struct ch395 *ch395, uint8_t command)
{
    // FIXME test nb_bytes_in_cmd_data in order to see if there is partial data sent into buffer

    ch395->nb_bytes_in_cmd_data = 0;
    ch395->pos_rw_in_cmd_data = 0;
    switch(command)
    {
        case CH395_CMD_GET_IC_VER:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_IC_VER]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_IC_VER]\n");
            break;

        case CH395_CMD_CHECK_EXIST:
            ch395->command = CH395_CMD_CHECK_EXIST;
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_CHECK_EXIST]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_CHECK_EXIST] waiting for check byte\n");
            break;

        case CH395_CMD_SET_BAUDRATE:
            break;

        case CH395_CMD_ENTER_SLEEP:
            break;

        case CH395_CMD_RESET_ALL:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_RESET_ALL]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_RESET_ALL]\n");
            ch395->command = CH395_CMD_RESET_ALL;
            break;

        case CH395_CMD_SET_PHY:
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS_ALL:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_GLOB_INT_STATUS_ALL]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_GLOB_INT_STATUS_ALL]\n");
            ch395->command = CH395_CMD_GET_GLOB_INT_STATUS_ALL;
            break;

        case CH395_CMD_SET_MAC_ADDR:
            break;

        case CH395_CMD_SET_IP_ADDR:
        case CH395_CMD_SET_GWIP_ADDR:
        case CH395_CMD_SET_MASK_ADDR :
        case CH395_CMD_SET_MAC_FILT:

        case CH395_CMD_GET_PHY_STATUS:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_PHY_STATUS]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_PHY_STATUS]\n");

            ch395->command = CH395_CMD_GET_PHY_STATUS;
            break;

        case CH395_CMD_INIT:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_INIT]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_INIT]\n");
            ch395->command = CH395_CMD_INIT;
            break;

        case CH395_CMD_GET_UNREACH_IPPORT:

        case CH395_CMD_GET_GLOB_INT_STATUS:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_GLOB_INT_STATUS]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_GLOB_INT_STATUS]\n");
            ch395->command = CH395_CMD_GET_GLOB_INT_STATUS;
            break;

        case CH395_CMD_SET_RETRAN_COUNT:
            break;

        case CH395_CMD_SET_RETRAN_PERIOD:
            break;

        case CH395_CMD_GET_CMD_STATUS:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_CMD_STATUS]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_CMD_STATUS]\n");
            ch395->command = CH395_CMD_GET_CMD_STATUS;
            break;

        case CH395_CMD_GET_REMOT_IPP_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_REMOT_IPP_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_REMOT_IPP_SN]\n");
            ch395->command = CH395_CMD_GET_REMOT_IPP_SN;
            break;

        case CH395_CMD_CLEAR_RECV_BUF_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_CLEAR_RECV_BUF_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_CLEAR_RECV_BUF_SN]\n");
            ch395->command = CH395_CMD_CLEAR_RECV_BUF_SN;
            break;

        case CH395_CMD_GET_SOCKET_STATUS_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_SOCKET_STATUS_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_SOCKET_STATUS_SN]\n");
            ch395->command = CH395_CMD_GET_SOCKET_STATUS_SN;
            break;

        case CH395_CMD_GET_INT_STATUS_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_INT_STATUS_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_INT_STATUS_SN]\n");
            ch395->command = CH395_CMD_GET_INT_STATUS_SN;
            break;

        case CH395_CMD_SET_IP_ADDR_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_IP_ADDR_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_SET_IP_ADDR_SN]\n");
            ch395->command = CH395_CMD_SET_IP_ADDR_SN;
            break;

        case CH395_CMD_SET_DES_PORT_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_DES_PORT_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_SET_DES_PORT_SN]\n");
            ch395->command = CH395_CMD_SET_DES_PORT_SN;
            break;

        case CH395_CMD_SET_SOUR_PORT_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_SOUR_PORT_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_SET_SOUR_PORT_SN]\n");
            ch395->command = CH395_CMD_SET_SOUR_PORT_SN;
            break;

        case CH395_CMD_SET_PROTO_TYPE_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_PROTO_TYPE_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_SET_PROTO_TYPE_SN]\n");
            ch395->command = CH395_CMD_SET_PROTO_TYPE_SN;
            break;

        case CH395_CMD_OPEN_SOCKET_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_OPEN_SOCKET_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_OPEN_SOCKET_SN]\n");
            ch395->command = CH395_CMD_OPEN_SOCKET_SN;
            break;

        case CH395_CMD_TCP_LISTEN_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_TCP_LISTEN_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_TCP_LISTEN_SN]\n");
            ch395->command = CH395_CMD_TCP_LISTEN_SN;
            break;

        case CH395_CMD_TCP_CONNECT_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_TCP_CONNECT_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_TCP_CONNECT_SN]\n");
            ch395->command = CH395_CMD_TCP_CONNECT_SN;
            break;

        case CH395_CMD_TCP_DISNCONNECT_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_TCP_DISNCONNECT_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_TCP_DISNCONNECT_SN]\n");
            ch395->command = CH395_CMD_TCP_DISNCONNECT_SN;
            break;

        case CH395_CMD_WRITE_SEND_BUF_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_WRITE_SEND_BUF_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_WRITE_SEND_BUF_SN]\n");
            ch395->command = CH395_CMD_WRITE_SEND_BUF_SN;
            break;

        case CH395_CMD_GET_RECV_LEN_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_RECV_LEN_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_RECV_LEN_SN]\n");
            ch395->command = CH395_CMD_GET_RECV_LEN_SN;
            break;

        case CH395_CMD_READ_RECV_BUF_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_READ_RECV_BUF_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_READ_RECV_BUF_SN]\n");
            ch395->command = CH395_CMD_READ_RECV_BUF_SN;
            break;

        case CH395_CMD_CLOSE_SOCKET_SN:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_CLOSE_SOCKET_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_CLOSE_SOCKET_SN]\n");
            ch395->command = CH395_CMD_CLOSE_SOCKET_SN;
            break;

        case CH395_CMD_SET_IPRAW_PRO_SN:
            break;

        case CH395_CMD_PING_ENABLE:
            break;

        case CH395_CMD_GET_MAC_ADDR:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_MAC_ADDR]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_MAC_ADDR]\n");
            ch395->command = CH395_CMD_GET_MAC_ADDR;
            break;

        case CH395_CMD_DHCP_ENABLE:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_DHCP_ENABLE]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_DHCP_ENABLE]\n");
            ch395->command = CH395_CMD_DHCP_ENABLE;
            break;

        case CH395_CMD_GET_DHCP_STATUS:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_DHCP_STATUS]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_DHCP_STATUS]\n");
            ch395->command = CH395_CMD_GET_DHCP_STATUS;
            break;

        case CH395_CMD_GET_IP_INF:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_IP_INF]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_IP_INF]\n");
            ch395->command = CH395_CMD_GET_IP_INF;
            break;

        case CH395_CMD_PPPOE_SET_USER_NAME:
            break;

        case CH395_CMD_PPPOE_SET_PASSWORD:
            break;
        case CH395_CMD_PPPOE_ENABLE:
            break;
        case CH395_CMD_GET_PPPOE_STATUS:
            break;
        case CH395_CMD_SET_TCP_MSS:
            break;
        case CH395_CMD_SET_TTL:
            break;
        case CH395_CMD_SET_RECV_BUF:
            break;
        case CH395_CMD_SET_SEND_BUF:
            break;
        case CH395_CMD_SET_FUN_PARA:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_FUN_PARA]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_SET_FUN_PARA]\n");
            ch395->command = CH395_CMD_SET_FUN_PARA;
            break;
        case CH395_CMD_SET_KEEP_LIVE_IDLE:
        case CH395_CMD_SET_KEEP_LIVE_INTVL:
        case CH395_CMD_SET_KEEP_LIVE_CNT:
        case CH395_CMD_SET_KEEP_LIVE_SN:
        case CH395_CMD_EEPROM_ERASE:
        case CH395_CMD_EEPROM_WRITE:
        case CH395_CMD_EEPROM_READ:
        case CH395_CMD_READ_GPIO_REG:
        case CH395_CMD_WRITE_GPIO_REG:
        default:
            printf(">>[CH395][UNKNOWN]\n");
            break;

    }
}

int ch395_write_data_port(struct ch395 *ch395, uint8_t data)
{

    switch(ch395->command)
    {
         case CH395_CMD_GET_IC_VER:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_IC_VER]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_IC_VER]\n");
            break;

        case CH395_CMD_CHECK_EXIST:
            ch395->cmd_data.CMD_CheckByte = ~data;
            printf(">>[CH395][WRITE][DATA][CH395_CMD_CHECK_EXIST] check byte received : 0x%02x convert : 0x%02x \n",data, ch395->cmd_data.CMD_CheckByte);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_CHECK_EXIST] check byte received : 0x%02x convert : 0x%02x \n",data, ch395->cmd_data.CMD_CheckByte);
            break;

        case CH395_CMD_SET_BAUDRATE:
            break;

        case CH395_CMD_ENTER_SLEEP:
            break;

        case CH395_CMD_RESET_ALL:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_RESET_ALL][ERROR] RESET_ALL can not accepted data on data port!\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_RESET_ALL][ERROR] RESET_ALL can not accepted data on data port!\n");
            break;

        case CH395_CMD_SET_PHY:
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS_ALL:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_GLOB_INT_STATUS_ALL] Socket : %d", data);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_GLOB_INT_STATUS_ALL] Socket : %d", data);
            break;

        case CH395_CMD_SET_MAC_ADDR:
            break;

        case CH395_CMD_SET_IP_ADDR:
            break;

        case CH395_CMD_SET_GWIP_ADDR:
            break;

        case CH395_CMD_SET_MASK_ADDR :
            break;

        case CH395_CMD_SET_MAC_FILT:
            break;

        case CH395_CMD_GET_PHY_STATUS:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_PHY_STATUS]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_PHY_STATUS]\n");
            break;

        case CH395_CMD_INIT:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_INIT][ERROR] INIT can not accepted data on data port!\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_INIT][ERROR] INIT can not accepted data on data port!\n");
            break;

        case CH395_CMD_GET_UNREACH_IPPORT:
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_GLOB_INT_STATUS]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_GLOB_INT_STATUS]\n");
            break;

        case CH395_CMD_SET_RETRAN_COUNT:
            break;

        case CH395_CMD_SET_RETRAN_PERIOD:
            break;

        case CH395_CMD_GET_CMD_STATUS:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_CMD_STATUS]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_CMD_STATUS]\n");
            break;

        case CH395_CMD_GET_REMOT_IPP_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_REMOT_IPP_SN]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_REMOT_IPP_SN]\n");
            break;

        case CH395_CMD_CLEAR_RECV_BUF_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_CLEAR_RECV_BUF_SN]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_CLEAR_RECV_BUF_SN]\n");
            break;
        case CH395_CMD_GET_SOCKET_STATUS_SN:

//    ;;@explain 00H SOCKET_CLOSED
//     ;;@explain 05H SOCKET_OPEN
//     ;;@explain The second status code is TCP status code, which is only meaningful when TCP mode has been on. TCP
//     ;;@explain status code is defined as follows:
//     ;;@explain Code Name Description
//     ;;@explain 00H TCP_CLOSED Closed
//     ;;@explain 01H TCP_LISTEN Monitoring
//     ;;@explain 02H TCP_SYN_SENT SYN sent
//     ;;@explain 03H TCP_SYN_RCVD SYN received
//     ;;@explain 04H TCP_ESTABLISHED TCP connection established
//     ;;@explain 05H TCP_FIN_WAIT_1 The active closing side first sends FIN
//     ;;@explain 06H TCP_FIN_WAIT_2 The active closing side receives an ACK from FIN
//     ;;@explain 07H TCP_CLOSE_WAIT The passive closing side receives FIN
//     ;;@explain 08H TCP_CLOSING Closing
//     ;;@explain 09H TCP_LAST_ACK The passive closing side sends FIN
//     ;;@explain 0AH TCP_TIME_WAIT 2MLS waiting status
//     ;;@explain TCP status is defined in TCP/IP protocol. Please refer to TCP/IP protocol for the detailed meaning
//     ;;@inputA Socket id

            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_SOCKET_STATUS_SN]");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_SOCKET_STATUS_SN]");

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                ch395->cmd_data.CMD_SocketState[0] = data;
                printf(" Socket : %d\n", data);
                dbg_printf(" Socket : %d\n", data);
            }
            else
            {
                printf("Error too much bytes into data port\n");
                dbg_printf("Error too much bytes into data port\n");
            }
            ch395->nb_bytes_in_cmd_data ++;
            break;

        case CH395_CMD_GET_INT_STATUS_SN:

            if (ch395->nb_bytes_in_cmd_data == 1)
            {
                printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_INT_STATUS_SN][ERROR] Accept only one byte (data already sent)\n");
                dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_INT_STATUS_SN] Accept only one byte  (data already sent)\n");
                break;
            }
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d\n", data);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d\n", data);

            ch395->cmd_data.CMD_SocketGetIntStatusSn[0] = data;
            ch395->nb_bytes_in_cmd_data ++;

//#define CH395_SINT_STAT_TIM_OUT       0x40
//#define CH395_SINT_STAT_DISCONNECT    0x10
//#define CH395_SINT_STAT_CONNECT       0x08
//#define CH395_SINT_STAT_RECV          0x04
//#define CH395_SINT_STAT_SEND_OK       0x02
//#define CH395_SINT_STAT_SENBUF_FREE   0x01

            break;

        case CH395_CMD_SET_IP_ADDR_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_IP_ADDR_SN]");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_SET_IP_ADDR_SN]");

            if (ch395->nb_bytes_in_cmd_data > 5)
            {
                printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_IP_ADDR][ERROR] Too much data");
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                printf("Socket : %d\n",data);
                dbg_printf("Socket %d\n",data);
                // Set socket
                ch395->cmd_data.CMD_SocketSetIpAddr[0] = data;
            }
            else
            {
                printf("Setting IP : %d\n",data);
                dbg_printf("Setting IP : %d\n",data);
                // Store dest ip
                ch395->socket_dest_ip[ch395->cmd_data.CMD_SocketSetIpAddr[0]][ch395->nb_bytes_in_cmd_data-1] = data;
            }
            // For errors detection we increment to test
            ch395->nb_bytes_in_cmd_data ++;
            break;

        case CH395_CMD_SET_DES_PORT_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_DES_PORT_SN]");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_SET_DES_PORT_SN]");

            if (ch395->nb_bytes_in_cmd_data > 2)
            {
                printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_DES_PORT_SN][ERROR] Too much data");
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                if (data > 7)
                {
                    printf("Error Socket impossible to have %d id (must be between 0 and 7)\n",data);
                    dbg_printf("Error Socket impossible to have %d id (must be between 0 and 7)\n",data);
                }
                else
                {
                    printf("Socket : %d\n",data);
                    dbg_printf("Socket %d\n",data);
                    // Set socket
                    ch395->cmd_data.CMD_SocketSetDesPort[0] = data;
                }
            }
            else
            {
                printf("Dest port : %d\n",data);
                dbg_printf("Dest port  : %d\n",data);
                // Store dest ip
                ch395->socket_dest_port[ch395->cmd_data.CMD_SocketSetDesPort[0]][ch395->nb_bytes_in_cmd_data-1] = data;

            }
            // For errors detection we increment to test
            ch395->nb_bytes_in_cmd_data ++;

            break;

        case CH395_CMD_SET_SOUR_PORT_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_SOUR_PORT_SN]");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_SET_SOUR_PORT_SN]");

            if (ch395->nb_bytes_in_cmd_data > 2)
            {
                printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_SOUR_PORT_SN][ERROR] Too much data");
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                printf("Socket : %d\n",data);
                dbg_printf("Socket %d\n",data);
                // Set socket
                ch395->cmd_data.CMD_SocketSetSrcPort[0] = data;
            }
            else
            {
                printf("Src port : %d\n",data);
                dbg_printf("Scr port  : %d\n",data);
                // Store dest ip
                ch395->socket_src_port[ch395->cmd_data.CMD_SocketSetSrcPort[0]][ch395->nb_bytes_in_cmd_data-1] = data;

            }
            // For errors detection we increment to test
            ch395->nb_bytes_in_cmd_data ++;
            break;
        case CH395_CMD_SET_PROTO_TYPE_SN:
            if (ch395->nb_bytes_in_cmd_data > 1)
            {
                printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_PROTO_TYPE_SN][ERROR] Too much");
                break;
            }

            printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_PROTO_TYPE_SN]");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_SET_PROTO_TYPE_SN]");

            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                printf("Selected socket : %d\n",data);
                dbg_printf("Selected socket : %d\n",data);

                // Set socket
                ch395->cmd_data.CMD_SocketSetProto[0] = data;
            }

            if (ch395->nb_bytes_in_cmd_data == 1)
            {
                if (data == CH395_PROTO_TYPE_TCP)
                {
                    printf("Setting CH395_PROTO_TYPE_TCP\n");
                    dbg_printf("Setting CH395_PROTO_TYPE_TCP\n");
                }

                if (data == CH395_PROTO_TYPE_UDP)
                {
                    printf("Setting CH395_PROTO_TYPE_UDP\n");
                    dbg_printf("Setting CH395_PROTO_TYPE_UDP\n");
                }

                if (data == CH395_PROTO_TYPE_MAC_RAW)
                {
                    printf("Setting CH395_PROTO_TYPE_MAC_RAW\n");
                    dbg_printf("Setting CH395_PROTO_TYPE_MAC_RAW\n");
                }

                if (data == CH395_PROTO_TYPE_IP_RAW)
                {
                    printf("Setting CH395_PROTO_TYPE_IP_RAW\n");
                    dbg_printf("Setting CH395_PROTO_TYPE_IP_RAW\n");
                }
                // Store Proto into socket_proto
                ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = data;
            }
            // For errors detection we increment to test
            ch395->nb_bytes_in_cmd_data ++;
            break;

        case CH395_CMD_OPEN_SOCKET_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_OPEN_SOCKET_SN]");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_OPEN_SOCKET_SN]");
            if (ch395->nb_bytes_in_cmd_data == 0)
            {   // opening socket
                printf("Opening socket : %d\n",data);
                dbg_printf("Opening socket : %d\n",data);

                // Set socket
                ch395->socket_state[data] = CH395_SOCKET_OPEN;


                ch395->sockfd_host[data] = socket(AF_INET, SOCK_STREAM, 0);
                if (ch395->sockfd_host[data] < 0)
                {
                    printf("Erreur lors de la création du socket %d", data);
                    dbg_printf("Erreur lors de la création du socket %d", data);
                    return 1;
                }
            }

            break;

        case CH395_CMD_TCP_LISTEN_SN:
            if (ch395->nb_bytes_in_cmd_data == 0)
            {   // opening socket
                ch395->socket_proto[ch395->cmd_data.CMD_SocketTCPListenSn[0]] = data;
                printf(">>[CH395][WRITE][DATA][CH395_CMD_TCP_LISTEN_SN] Selected socket : %d\n",data);
                dbg_printf("[CH395][WRITE][DATA][CH395_CMD_TCP_LISTEN_SN] Selected socket : %d\n",data);
            }
            else
            {
                printf(">>[CH395][WRITE][DATA][CH395_CMD_TCP_LISTEN_SN] Error two much bytes");
                dbg_printf("[CH395][WRITE][DATA][CH395_CMD_TCP_LISTEN_SN] Error two much bytes %d\n");
            }

            ch395->nb_bytes_in_cmd_data ++;
            break;

        case CH395_CMD_TCP_CONNECT_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_TCP_CONNECT_SN]");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_TCP_CONNECT_SN]");
            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                ch395->cmd_data.CMD_SocketState[0] = data;
                printf("Connecting socket %d ...\n", data);
                dbg_printf(" Connecting socket %d ...\n", data);
            }
            else
            {
                printf("Error too much bytes into data port\n");
                dbg_printf("Error too much bytes into data port\n");
            }
            ch395->nb_bytes_in_cmd_data ++;
            perform_connect_socket(ch395, ch395->cmd_data.CMD_SocketWriteBuffer[0]);
            printf("Launching connect from socket %d\n", ch395->cmd_data.CMD_SocketWriteBuffer[0]);
            dbg_printf("Launching connect from socket %d\n", ch395->cmd_data.CMD_SocketWriteBuffer[0]);
            ch395->socket_int_status[ch395->cmd_data.CMD_SocketState[0]] = CH395_SINT_STAT_CONNECT;

            break;

        case CH395_CMD_TCP_DISNCONNECT_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_TCP_DISNCONNECT_SN]\n");
            dbg_printf(">>[CH395][WRITE][DATA][CH395_CMD_TCP_DISNCONNECT_SN]\n");
            break;

        case CH395_CMD_WRITE_SEND_BUF_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_WRITE_SEND_BUF_SN]");
            dbg_printf(">>[CH395][WRITE][DATA][CH395_CMD_WRITE_SEND_BUF_SN]");
            if (ch395->nb_bytes_in_cmd_data == 0)
            {
                ch395->cmd_data.CMD_SocketWriteBuffer[0] = data;
                printf("Socket : %d\n",data);
                dbg_printf("Socket : %d\n",data);
                ch395->nb_bytes_in_cmd_data ++;
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 1)
            {
                printf("length data low : %d\n", data);
                dbg_printf("length data low : %d\n", data);
                ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]] = data;
                ch395->nb_bytes_in_cmd_data ++;
                break;
            }

            if (ch395->nb_bytes_in_cmd_data == 2)
            {
                printf("length data high : %d\n", data);
                dbg_printf("length data high : %d\n", data);
                ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]] += data*256;
                ch395->nb_bytes_in_cmd_data ++;
                break;
            }
            // Launch socket (for instance stub)
            ch395->buffer[ch395->transmit_buffer_start_block[ch395->cmd_data.CMD_SocketWriteBuffer[0]]*CH395_SIZE_BLOCK_BUFFER + ch395->nb_bytes_in_cmd_data - 3]  = data;

            if (data == 13)
                {
                printf("value : %d/char 13\n", data);
                dbg_printf("value : %d/char 13\n", data);
                }
            else
            {
                printf("value : %d/char %c\n", data, data);
                dbg_printf("value : %d/char %c\n", data, data);
            }

            ch395->nb_bytes_in_cmd_data ++;
            if (ch395->nb_bytes_in_cmd_data == ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]] + 3 )
            {
                // Send transmit buffer
                // Get the position of the buffer
                void *p = &ch395->buffer[ch395->transmit_buffer_start_block[ch395->cmd_data.CMD_SocketWriteBuffer[0]]*CH395_SIZE_BLOCK_BUFFER];

                // Send buffer (p is the adress of the position of transmit_buffer)
                if (send(ch395->sockfd_host[ch395->cmd_data.CMD_SocketWriteBuffer[0]], p, ch395->socket_length_to_send[ch395->cmd_data.CMD_SocketWriteBuffer[0]], 0) < 0)
                {
                    perror("Erreur lors de l'envoi de la requête");
                    return 1;
                }
                //ch395->socket_int_status[ch395->cmd_data.CMD_SocketWriteBuffer[0]] = ch395->socket_int_status[ch395->cmd_data.CMD_SocketWriteBuffer[0]] | CH395_SINT_STAT_SEND_OK;

                // Set SINT_STAT_SEND_OK
                ch395->socket_int_status[ch395->cmd_data.CMD_SocketWriteBuffer[0]] = ch395->socket_int_status[ch395->cmd_data.CMD_SocketWriteBuffer[0]]  | CH395_SINT_STAT_SENBUF_FREE;

                p = &ch395->buffer[ch395->receive_buffer_start_block[ch395->cmd_data.CMD_SocketWriteBuffer[0]]*CH395_SIZE_BLOCK_BUFFER];
                // Recevoir des données
                int bytes_received = recv(ch395->sockfd_host[ch395->cmd_data.CMD_SocketWriteBuffer[0]], p, 800, 0);
                if (bytes_received < 0)
                {
                    perror("Erreur lors de la réception des données");
                    return 1;
                }

                // STAT_SEND_OK
                ch395->socket_int_status[ch395->cmd_data.CMD_SocketState[0]] = ch395->socket_int_status[ch395->cmd_data.CMD_SocketState[0]] | CH395_SINT_STAT_SEND_OK;

                ch395->buffer_position_receive[ch395->cmd_data.CMD_SocketWriteBuffer[0]] = 800;
                //
                switch (ch395->cmd_data.CMD_SocketWriteBuffer[0])
                {
                    case 0:
                        ch395->glob_int_status = ch395->glob_int_status | CH395_GINT_STAT_SOCK0;
                        break;
                    case 1:
                        ch395->glob_int_status = ch395->glob_int_status | CH395_GINT_STAT_SOCK1;
                        break;
                    case 2:
                        ch395->glob_int_status = ch395->glob_int_status | CH395_GINT_STAT_SOCK2;
                        break;
                    case 3:
                        ch395->glob_int_status = ch395->glob_int_status | CH395_GINT_STAT_SOCK3;
                        break;
                }

            }
            break;

        case CH395_CMD_GET_RECV_LEN_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_RECV_LEN_SN]");
            dbg_printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_RECV_LEN_SN]");

            ch395->cmd_data.CMD_SocketGetRecvLen[0] = data;
            printf("Socket : %d\n",data);
            dbg_printf("Socket : %d\n",data);
            ch395->nb_bytes_in_cmd_data ++;
            break;

        case CH395_CMD_READ_RECV_BUF_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_READ_RECV_BUF_SN]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_READ_RECV_BUF_SN]\n");
            break;

        case CH395_CMD_CLOSE_SOCKET_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_CLOSE_SOCKET_SN] Socket %d\n", data);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_CLOSE_SOCKET_SN] Socket %d\n", data);
            close(ch395->sockfd_host[data]);
            break;

        case CH395_CMD_SET_IPRAW_PRO_SN:
            break;

        case CH395_CMD_PING_ENABLE:
            break;

        case CH395_CMD_GET_MAC_ADDR:
            break;

        case CH395_CMD_DHCP_ENABLE:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_DHCP_ENABLE]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_DHCP_ENABLE]\n");
            break;

        case CH395_CMD_GET_DHCP_STATUS:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_DHCP_STATUS]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_DHCP_STATUS]\n");
            break;

        case CH395_CMD_GET_IP_INF:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_IP_INF]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_IP_INF]\n");
            break;

        case CH395_CMD_PPPOE_SET_USER_NAME:
        case CH395_CMD_PPPOE_SET_PASSWORD:
        case CH395_CMD_PPPOE_ENABLE:
        case CH395_CMD_GET_PPPOE_STATUS:
        case CH395_CMD_SET_TCP_MSS:
        case CH395_CMD_SET_TTL:
        case CH395_CMD_SET_RECV_BUF:
        case CH395_CMD_SET_SEND_BUF:
        case CH395_CMD_SET_FUN_PARA:

            printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_FUN_PARA]\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_SET_FUN_PARA]\n");
            break;
        case CH395_CMD_SET_KEEP_LIVE_IDLE:
        case CH395_CMD_SET_KEEP_LIVE_INTVL:
        case CH395_CMD_SET_KEEP_LIVE_CNT:
        case CH395_CMD_SET_KEEP_LIVE_SN:
        case CH395_CMD_EEPROM_ERASE:
        case CH395_CMD_EEPROM_WRITE:
        case CH395_CMD_EEPROM_READ:
        case CH395_CMD_READ_GPIO_REG:
        case CH395_CMD_WRITE_GPIO_REG:
        default:
            printf(">>[CH395][UNKNOWN]\n");
            break;

    }

   // dbg_printf(">> [ch395][WRITE][COMMAND] Write command &%02x status &%02x\n", command, ch395->command_status);

    // ch395->interface_status = 0;

    // // Emulate CH376 bug which can get the check byte
    // // from the command port instead of the data port!
    // if(ch395->command == CH395_CMD_CHECK_EXIST)
    // {
    //     ch395->cmd_data.CMD_CheckByte = ~command;
    //     dbg_printf("[CH395][WRITE][DATA][CH395_CMD_CHECK_EXIST] got check byte &%02x from command port!\n", command);
    // }
    return 0;
}


Uint8  ch395_read(struct machine *oric, SDL_bool fBank, unsigned int instance, Uint16 addr, SDL_bool run)
{
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return (Uint8) 0;

    instance--;

    if (addr == 0x00) ch395_read_data_port(userdata[instance]);
    if (addr == 0x01) ch395_read_command_port(userdata[instance]);

}

SDL_bool ch395_write(struct machine *oric, SDL_bool fBank, unsigned int instance, Uint16 addr, Uint8 data)
{
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    if (addr == 0x00) ch395_write_data_port(userdata[instance], data);
    if (addr == 0x01) ch395_write_command_port(userdata[instance], data);

}


// -----------------------------------------------------------------------------
//                              ch395_addresses
// -----------------------------------------------------------------------------
// Called to check plugin addresses (if multiples addresses)
SDL_bool ch395_addresses(unsigned int instance,  Uint16 offset)
{
    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    dbg_printf("---CH395 addresses(%04x)\n", offset);

    instance--;

    // [base_addr, base_addr+3]
    if (offset <= 0x03)
        return SDL_TRUE;

    // [base_addr+0x0c, base_addr+0x0f]
    if ((offset >= 0x22) && (offset <= 0x23))
        return SDL_TRUE;

    return SDL_FALSE;
}

/*
unsigned int plugin_create(struct machine *oric)
{
    oric = oric; // gcc [-Wunused-parameter]

    if (plugin_instances >= INSTANCE_MAX)
        return 0;

    userdata[plugin_instances] = malloc(sizeof(struct ch395));
    userdata_old[plugin_instances] = malloc(sizeof(struct ch395));

    if (userdata[plugin_instances])
    {
        if (userdata_old[plugin_instances] == NULL)
        {
            free(userdata[plugin_instances]);
            return 0;
        }


      return ++plugin_instances;
    }

    return 0;
}
*/
// -----------------------------------------------------------------------------
//                              ch395_shutdown
// -----------------------------------------------------------------------------
// Called on exit
SDL_bool ch395_shutdown(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]
    instance = instance; // gcc [-Wunused-parameter]

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                              plugin_init
// -----------------------------------------------------------------------------
// Run once right after thz library load
//
SDL_bool plugin_init(void *tzprintfpos, void *tzputc, void *_mon_periphmod)
{
    dbg_printf("---CH395 init\n");

    my_tzprintfpos = tzprintfpos;
    my_tzputc = tzputc;
    mon_periphmod = _mon_periphmod;

    return SDL_TRUE;
}

// -----------------------------------------------------------------------------
//                                  ch395_reset
// -----------------------------------------------------------------------------
// Called by init_machine and [F4]
SDL_bool ch395_reset(struct expansion_bus *oric, unsigned int instance)
{
    dbg_printf("CH395_reset(%d)\n", instance);
    /*
    error_printf(": oric->romdis = %02x\n", oric->romdis);
    error_printf(": oric->cpu.a  = %02x\n", oric->cpu->a);

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    userdata[instance].firmware_version = 2;
    userdata[instance].t_register = (userdata[instance].firmware_version & 0x03) | 0x80;
    userdata[instance].t_banking_register = 0;
    userdata[instance].DDRA = 0xa7;         // 0b10100111;
    userdata[instance].IORAh = 0x07;
    userdata[instance].DDRB = 0xc0;         // 0b11000000;
    userdata[instance].IORB = 0;


    // À voir si on initialise avec des données aléatoires au lieu de 0x00
    config_load(&userdata[instance]);

    // Désactive la rom interne
    *oric->romdis = SDL_TRUE;
    // Défaut par setromon()
    // oric->romon = ! oric->romdis;
*/
    return SDL_TRUE;
}



// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
void mon_ch395_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid)
{
    base_addr = base_addr; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // int bank;
    // struct BOARD *twilighte = &userdata[instance];

    dbg_printf("ch395: mon update (instance=%d)\n", instance);

    //
    // Board version = $xx
    //
    // Bank set      = $xx
    // Bank hardware number = $xx
    // Bank software number = $xx
    // Bank type     = ssss
    // ---------------------------
    // Twil register = $xx
    // Bank register = $xx
    //
    // IORB          = $xx
    // IORAh         = $xx
    // DDRB          = $xx
    // DDRA          = $xx
    //
    //
    //
    //
    //123456789.123456789.12345678
/*
    bank = logical_bank(twilighte);

    my_tzprintfpos( ptz, 2, 2,  "Board version :  %02d", twilighte->t_register & 0x07 );
    my_tzprintfpos( ptz, 2, 4,  "Bank set      : $%02X", twilighte->t_banking_register );
    my_tzprintfpos( ptz, 2, 5,  "Bank hardware :  %02d", cpld(twilighte) );
    my_tzprintfpos( ptz, 2, 6,  "Bank software :  %02d", (bank > 32 ? bank - 32 : bank) );
    my_tzprintfpos( ptz, 2, 7,  "Bank type     : %s"  , ((bank == 0) ? "Overlay" : ((bank > 32) ? "SRAM" : "EEPROM")) );

    // Trait de séparation en ligne 8
    ptz->px = 0;
    ptz->py = 8;
    my_tzputc( ptz, 6 );

    for (int i=0; i < ptz->w-2; i++)
//        my_tzputc( ptz, 2 );
        my_tzputc( ptz, 12 );

    my_tzputc( ptz, 8 );

    my_tzprintfpos( ptz, 2, 9 , "Twil register : $%02X", twilighte->t_register );
    my_tzprintfpos( ptz, 2, 10, "Bank register : $%02X", twilighte->t_banking_register );

    my_tzprintfpos( ptz, 2, 12, "IORB          : $%02X", twilighte->IORB );
    my_tzprintfpos( ptz, 2, 13, "IORAh         : $%02X", twilighte->IORAh );
    my_tzprintfpos( ptz, 2, 14, "DDRB          : $%02X", twilighte->DDRB );
    my_tzprintfpos( ptz, 2, 15, "DDRA          : $%02X", twilighte->DDRA );


    // Affiche sur fond rouge les valeurs différentes par rapport au précédent appel au moniteur.
    if (oldvalid)
    {
        struct BOARD *twilighte_old = &userdata_old[instance];
        int bank_old = logical_bank(twilighte_old);

        // Board version
        if ( (twilighte->t_register & 0x07) != (twilighte_old->t_register & 0x07) )
            mon_periphmod( 19, 2, 2, ptz );


        // Bank set
        if (twilighte->t_banking_register != twilighte_old->t_banking_register)
            mon_periphmod( 19, 4, 2, ptz );

        // Bank hardware
        if (cpld(twilighte) != cpld(twilighte_old))
            mon_periphmod( 19, 5, 2, ptz );

        // Bank software
        if (bank != bank_old)
            mon_periphmod( 19, 6, 2, ptz );

        // Bank type
        if ( ((bank == 0) ? 0 : ((bank > 32) ? 1 : 2)) != ((bank_old == 0) ? 0 : ((bank_old > 32) ? 1 : 2)) )
            mon_periphmod( 18, 7, 7, ptz );


        // Twil register
        if (twilighte->t_register != twilighte_old->t_register)
            mon_periphmod( 19, 9,  2, ptz );

        // Bank register
        if (twilighte->t_banking_register != twilighte_old->t_banking_register)
            mon_periphmod( 19, 10, 2, ptz );


        // IORB
        if (twilighte->IORB != twilighte_old->IORB)
            mon_periphmod( 19, 12, 2, ptz );

        // IORah
        if (twilighte->IORAh != twilighte_old->IORAh)
            mon_periphmod( 19, 13, 2, ptz );

        // DDRB
        if (twilighte->DDRB != twilighte_old->DDRB)
            mon_periphmod( 19, 14, 2, ptz );

        // DDRA
        if (twilighte->DDRA != twilighte_old->DDRA)
            mon_periphmod( 19, 15, 2, ptz );
    }
    dbg_printf("TWILIGHTE: mon update]\n");
*/
}

// -------------------------------------------------------------------------
//                      Sauvegarde de l'état
// -------------------------------------------------------------------------
// Called by monitor
void mon_ch395_store(struct machine *oric, unsigned int instance)
{
    oric = oric; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    // Copy data+ptr
    //memcpy(&userdata_old[instance], &userdata[instance], sizeof(struct BOARD));
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "ch395",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                ch395_addresses,
                ch395_create,
                ch395_shutdown,
                ch395_reset,
                ch395_read,
                ch395_write,
                NULL,
                mon_ch395_update,
                mon_ch395_store,
    };



