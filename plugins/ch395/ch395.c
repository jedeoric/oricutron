#include "ch395.h"
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>


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

#define INSTANCE_MAX 1

struct ch395 *userdata[INSTANCE_MAX];
struct ch395 *userdata_old[INSTANCE_MAX];


//Used for Linux 
#define ROUTE_PATH "/proc/net/route"
#define RESOLV_PATH "/etc/resolv.conf"

/*
TODO :
* Etant donné que le CH395 doit avoir un source port, si la socket a une connexion en court de fermeture et qu'on essaie sur une autre socket ou sur la même
de se connecter avec un source port identique, alors, la connexion ne sera jamais faite, et la tcp_connect échouera tant que le port source n'est pas libre (ou alors il faut en prendre un nouveau
pour initier la connexion)
*/

// --------1---------------------------------------------------------------------
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

#define BASE_ADDR 0x0360
#define END_ADDR 0x0361


// Nombre d'instances total
unsigned int plugin_instances = 0;

// Description du plugin
static char *description = "CH395";

void ch395_fill_get_ip_inf_linux(struct ch395 *ch395) {



    FILE *route_file, *resolv_file;
    char iface[IF_NAMESIZE];
    char line[256];
    struct ifaddrs *ifaddr, *ifa;
    char ip_address[INET_ADDRSTRLEN];
    char gateway_address[INET_ADDRSTRLEN];
    unsigned long gateway;
    char subnet_mask[INET_ADDRSTRLEN];

    unsigned char ip_first_octet = 0, dns1_first_octet = 0, dns2_first_octet = 0;

    // Ouvrir le fichier de routage
    route_file = fopen(ROUTE_PATH, "r");
    if (!route_file) {
        perror("Erreur lors de l'ouverture de /proc/net/route");
        return EXIT_FAILURE;
    }

    // Lire chaque ligne de la table de routage
    while (fgets(line, sizeof(line), route_file)) {
        unsigned long dest, flags;

        if (sscanf(line, "%s %lx %lx %lx", iface, &dest, &gateway, &flags) == 4) {
            if (dest == 0) { // La destination 0.0.0.0 correspond à la passerelle par défaut
                printf("Interface avec passerelle par défaut : %s\n", iface);

                // Convertir l'adresse de la passerelle en format lisible
                struct in_addr gw_addr;
                gw_addr.s_addr = gateway;

                unsigned char *ip_by_bytes = (unsigned char *)&gateway;

                ch395->ip_chip[11] = ip_by_bytes[0];
                ch395->ip_chip[10] = ip_by_bytes[1];
                ch395->ip_chip[9] = ip_by_bytes[2];
                ch395->ip_chip[8] = ip_by_bytes[3];

                inet_ntop(AF_INET, &gw_addr, gateway_address, INET_ADDRSTRLEN);
                printf("Passerelle : %s\n", gateway_address);

                // Récupérer les adresses associées à cette interface
                if (getifaddrs(&ifaddr) == -1) {
                    perror("getifaddrs");
                    fclose(route_file);
                    return EXIT_FAILURE;
                }

                for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                    if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
                        continue;
                    }

                    if (strcmp(ifa->ifa_name, iface) == 0) {
                        // Obtenir l'adresse IP en format lisible


                        struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;



                        // Convertir le masque en format lisible
                        inet_ntop(AF_INET, &netmask->sin_addr, subnet_mask, INET_ADDRSTRLEN);
                        printf("Masque de sous-réseau pour %s : %s\n", iface, subnet_mask);

                        unsigned char *ip_by_bytes = (unsigned char *)&netmask->sin_addr.s_addr;

                        ch395->ip_chip[7] = ip_by_bytes[0];
                        ch395->ip_chip[6] = ip_by_bytes[1];
                        ch395->ip_chip[5] = ip_by_bytes[2];
                        ch395->ip_chip[4] = ip_by_bytes[3];

                        struct sockaddr_in *addr_ip = (struct sockaddr_in *)ifa->ifa_addr;

                        if (inet_ntop(AF_INET, &addr_ip->sin_addr, ip_address, INET_ADDRSTRLEN) != NULL) {
                            printf("Adresse IP de l'interface %s : %s\n", iface, ip_address);
                            unsigned char *ip_by_bytes = (unsigned char *)&addr_ip->sin_addr.s_addr;

                            ch395->ip_chip[3] = ip_by_bytes[0];
                            ch395->ip_chip[2] = ip_by_bytes[1];
                            ch395->ip_chip[1] = ip_by_bytes[2];
                            ch395->ip_chip[0] = ip_by_bytes[3];

                        }
                        break;
                    }
                }

                freeifaddrs(ifaddr);
                fclose(route_file);

                // Lecture des serveurs DNS depuis /etc/resolv.conf
                resolv_file = fopen(RESOLV_PATH, "r");
                if (!resolv_file) {
                    perror("Erreur lors de l'ouverture de /etc/resolv.conf");
                    return EXIT_FAILURE;
                }

                printf("Serveurs DNS :\n");
                int dns_count = 0;
                while (fgets(line, sizeof(line), resolv_file)) {
                    if (strncmp(line, "nameserver", 10) == 0) {
                        char dns[INET_ADDRSTRLEN];
                        if (sscanf(line, "nameserver %s", dns) == 1) {
                            printf("- %s\n", dns);

                            // Extraire le 1er octet du serveur DNS
                            struct in_addr dns_addr;
                            inet_pton(AF_INET, dns, &dns_addr);
                            if (dns_count == 0) {
                                dns1_first_octet = ((unsigned char *)&dns_addr.s_addr)[0];
                                unsigned char *ip_by_bytes = (unsigned char *)&dns_addr.s_addr;

                                ch395->ip_chip[15] = ip_by_bytes[0];
                                ch395->ip_chip[14] = ip_by_bytes[1];
                                ch395->ip_chip[13] = ip_by_bytes[2];
                                ch395->ip_chip[12] = ip_by_bytes[3];

                            } else if (dns_count == 1) {
                                dns2_first_octet = ((unsigned char *)&dns_addr.s_addr)[0];

                                unsigned char *ip_by_bytes = (unsigned char *)&dns_addr.s_addr;

                                ch395->ip_chip[20] = ip_by_bytes[0];
                                ch395->ip_chip[18] = ip_by_bytes[1];
                                ch395->ip_chip[17] = ip_by_bytes[2];
                                ch395->ip_chip[19] = ip_by_bytes[3];
                            }
                            dns_count++;
                        }
                    }
                }

                fclose(resolv_file);

                // Afficher les 1ers octets
                printf("1er octet de l'adresse IP de l'interface : %u\n", ip_first_octet);
                if (dns1_first_octet != 0) {
                    printf("1er octet du 1er serveur DNS : %u\n", dns1_first_octet);
                }
                if (dns2_first_octet != 0) {
                    printf("1er octet du 2ème serveur DNS : %u\n", dns2_first_octet);
                }


                return EXIT_SUCCESS;
            } 
        }
    }

    fclose(route_file);
    printf("Aucune passerelle par défaut trouvée.\n");
    return EXIT_FAILURE;
}

int perform_connect_socket(struct ch395 *ch395, unsigned char socketid)
{
    char ip[16]; // 16 pour contenir l'IP au format "xxx.xxx.xxx.xxx\"
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = ch395->sockfd_host[socketid];

    // Configurer l'adresse du serveur
    server_addr.sin_family = AF_INET;

    int port = ch395->socket_dest_port[socketid][0] + ch395->socket_dest_port[socketid][1]*256;
    server_addr.sin_port = htons(port); // Port par défaut pour HTTP


    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", ch395->socket_dest_ip[socketid][0], ch395->socket_dest_ip[socketid][1], ch395->socket_dest_ip[socketid][2], ch395->socket_dest_ip[socketid][3]);
    printf("%s %u.%u.%u.%u\n",ip, ch395->socket_dest_ip[socketid][0], ch395->socket_dest_ip[socketid][1], ch395->socket_dest_ip[socketid][2], ch395->socket_dest_ip[socketid][3]);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Adresse IP invalide");
        return 1;
    }
    printf("Connexion vers : %s:%d\n",ip, port);
    dbg_printf("Connexion vers : %s:%d\n",ip, port);
    //ch395->CommandData
    //server_addr.sin_addr.s_addr =*((unsigned long *)host->h_addr_list[0]);

    //Se connecter au serveur
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur lors de la connexion au serveur");
        printf("erreur de la Connexion vers : %s:%d\n",ip, port);
        dbg_printf("erreur de la  Connexion vers : %s:%d\n",ip, port);
        return 1;
    }

        printf("la Connexion semble établie vers : %s:%d\n",ip, port);
        dbg_printf("la Connexion semble établie vers : %s:%d\n",ip, port);
        ch395->socket_status_sn[socketid][1] = CH395_TCP_ESTABLISHED;

    return 0;
}


void ch395_init_internal(struct ch395 *ch395)
{
    unsigned char i;
    for(i=0;i<8;i++)
    {
        //ch395->socket_state[i] = CH395_SOCKET_CLOSED; // Socket state FIXME doublon
        //ch395->socket_proto[i] = CH395_TCP_CLOSED; // State protocol
        ch395->socket_status_sn[i][0] = CH395_SOCKET_CLOSED; // State protocol
        ch395->socket_status_sn[i][1] = CH395_TCP_CLOSED; // State protocol
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
    int i;
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

        for (i = 0; i< 20 ; i++) {
            userdata[plugin_instances]->ip_chip[i] = 0;
        }
        ch395_init_internal(userdata[plugin_instances]);


        return ++plugin_instances;
    }

    return 0;
}


unsigned char ch395_read_command_port(struct ch395 *ch395)
{
    printf(">>[CH395][READ][COMMAND] Can not read command port\n");
    return 0;
}

unsigned char ch395_read_data_port(struct ch395 *ch395)
{
    unsigned char data = 0xff;
    char *msg;
    char *value;
    msg = malloc(200);
    value = malloc(200);

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
            printf("<<[CH395][READ][DATA][CH395_CMD_GET_PHY_STATUS]");
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_PHY_STATUS]");
            // If ch395 is not init, PHY_state is always disconnected
            if (ch395->is_init == CH395_FALSE) {
                printf("ch395 not init Cable disconnected\n");
                dbg_printf("ch395 not init Cable disconnected\n");
                data = CH395_PHY_DISCONN;
            }
            else {
                ch395->phy_state = CH395_PHY_100M_FLL;
                printf("Return val %x Cable connected\n", ch395->phy_state);
                dbg_printf("Return val %d Cable connected\n", ch395->phy_state);
                data = ch395->phy_state;
                // The next call, we set CH395_PHY_100M_FLL. FIXME : in order to be correct, it should test network host stack
                // Depending of the network when the oric is connected, this value should not be this, but we set this now

            }
            break;

        case CH395_CMD_GET_IP_INF:
            printf(">>[CH395][READ][DATA][CH395_CMD_GET_IP_INF]");
            dbg_printf("[CH395][READ][DATA][CH395_CMD_GET_IP_INF]");
            if (ch395->pos_rw_in_cmd_data  == 20) {
                printf("CH395 PANIC impossible to read more than 4 bytes \n");
                dbg_printf("CH395 PANIC impossible to read more than 4 bytes \n");
            }
            else {
                data = ch395->ip_chip[ch395->pos_rw_in_cmd_data];
                ch395->pos_rw_in_cmd_data ++;
                printf(" send : %d\n",data);
            }
            break;

        case CH395_CMD_READ_RECV_BUF_SN:
            printf("<<[CH395][READ][DATA][CH395_CMD_READ_RECV_BUF_SN] ");
            dbg_printf("<<[CH395][READ][COMMAND][CH395_CMD_READ_RECV_BUF_SN] ");
            int current_socket = ch395->cmd_data.CMD_SocketGetRecvBuf[0];
            int pos_buffer = ch395->pos_rw_in_cmd_data + ch395->transmit_buffer_start_block[current_socket] * CH395_SIZE_BLOCK_BUFFER;
            printf("POS buffer : %d ", pos_buffer);

            data = ch395->buffer[pos_buffer]; // FIXME
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
            break;

        case CH395_CMD_GET_MAC_ADDR:
            if (ch395->nb_bytes_in_cmd_data == 6) {
                printf("CH395 panic : impossible to read mac adress more than 6 bytes");
                data = 0;
            }
            else {
                data = ch395->mac_address[ch395->nb_bytes_in_cmd_data];
                ch395->nb_bytes_in_cmd_data++;
            }
            break;

        case CH395_CMD_GET_RECV_LEN_SN:
            char *msg = "<<[CH395][READ][DATA][CH395_CMD_GET_RECV_LEN_SN]";
            printf("%s", msg);
            dbg_printf("%s", msg);
            if ( ch395->pos_rw_in_cmd_data == 0 )
            {
                data = ch395->buffer_position_receive[ch395->cmd_data.CMD_SocketGetRecvLen[0]] & 0xFF;
                printf("Receiving length low %d\n", data);
                dbg_printf("Receiving length low %d\n", data);
            }

            if ( ch395->pos_rw_in_cmd_data == 1 )
            {
                data = ch395->buffer_position_receive[ch395->cmd_data.CMD_SocketGetRecvLen[0]] >> 8;
                printf("Receiving length high %d\n", data);
                dbg_printf("Receiving length high %d\n", data);
            }

            ch395->pos_rw_in_cmd_data ++;
            break;

        case CH395_CMD_GET_SOCKET_STATUS_SN:
            char *msg_read_data_socket_status_sn = "<<[CH395][READ][DATA][CH395_CMD_GET_SOCKET_STATUS_SN]";
            printf("%s",msg_read_data_socket_status_sn);
            dbg_printf("%s",msg_read_data_socket_status_sn);

            if ( ch395->pos_rw_in_cmd_data == 0 )
            {
                data = ch395->socket_status_sn[ch395->cmd_data.CMD_SocketState[0]][0];
                switch (data)
                {
                    case CH395_SOCKET_CLOSED:
                        strcpy(msg," SOCKET_CLOSED");
                        break;

                    case CH395_SOCKET_OPEN:
                        strcpy(msg," CH395_SOCKET_OPEN");
                        break;

                    default:
                        strcpy(msg," Unknown socket state");
                        break;
                }

                printf("Socket: %d socket State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
                dbg_printf("Socket: %d socket State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
            }

            if ( ch395->pos_rw_in_cmd_data == 1 )
            {
                data = ch395->socket_status_sn[ch395->cmd_data.CMD_SocketState[0]][1];
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
                default:
                    strcpy(msg," Unknown protocol state");
                    break;
                }
                printf("Socket: %d socket  bla  protocol State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
                dbg_printf("Socket: %d socket bla protocol State:%s\n", ch395->cmd_data.CMD_SocketState[0], msg);
            }


            ch395->pos_rw_in_cmd_data ++;
            break;

        case CH395_CMD_GET_GLOB_INT_STATUS:
            printf("<<[CH395][READ][DATA][CH395_CMD_GET_GLOB_INT_STATUS] value : %d \n",ch395->glob_int_status);
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_GLOB_INT_STATUS] value : %d \n",ch395->glob_int_status);

            data = ch395->glob_int_status;

            if (ch395->glob_int_status & CH395_GINT_STAT_SOCK3)
            {
                ch395->glob_int_status &= ~CH395_GINT_STAT_SOCK3;
            }
            if (ch395->glob_int_status & CH395_GINT_STAT_SOCK2)
            {
                ch395->glob_int_status &= ~CH395_GINT_STAT_SOCK2;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_SOCK1)
            {
                ch395->glob_int_status &= ~CH395_GINT_STAT_SOCK1;
            }

            if (ch395->glob_int_status & CH395_GINT_STAT_SOCK0)
            {
                ch395->glob_int_status &= ~CH395_GINT_STAT_SOCK0;
            }

            break;

        case CH395_CMD_GET_INT_STATUS_SN:
            unsigned char socket = ch395->cmd_data.CMD_SocketGetIntStatusSn[0];
            data = ch395->socket_int_status[socket];
            // When a socket is not open CH395_CMD_GET_INT_STATUS_SN command returns always 0 for socket state
            // Clear status
            // Each time CH395_CMD_GET_INT_STATUS_SN is read, we reset necessary status


            if (ch395->socket_int_status[socket] & CH395_SINT_STAT_SEND_OK)
            {
                // Remettre le bit CH395_SINT_STAT_SEND_OK à 0
                ch395->socket_int_status[socket] &= ~CH395_SINT_STAT_SEND_OK;
            }

            if (ch395->socket_int_status[socket] & CH395_SINT_STAT_RECV)
            {
                // Remettre le bit CH395_SINT_STAT_RECV à 0
                ch395->socket_int_status[socket] &= ~CH395_SINT_STAT_RECV;
            }

            if (ch395->socket_int_status[socket] & CH395_SINT_STAT_SENBUF_FREE)
            {
                // Remettre le bit CH395_SINT_STAT_SENBUF_FREE à 0
                ch395->socket_int_status[socket] &= ~CH395_SINT_STAT_SENBUF_FREE;
            }


            sprintf(value, "INT_STATUS_SN states : %d=", data);
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

                // Send recv

                // Compute position of the buffer
                void *p = &ch395->buffer[ch395->transmit_buffer_start_block[socket]*CH395_SIZE_BLOCK_BUFFER];
                // Read data from socket
                int bytes_received = recv(ch395->sockfd_host[socket], p, 800, 0);
                if (bytes_received < 0)
                {
                    perror("Erreur lors de la réception des données");
                    return 1;
                }
                ch395->socket_int_status[socket] |= CH395_SINT_STAT_RECV;
                ch395->buffer_position_receive[socket] = bytes_received;
                strcat(msg," CH395_SINT_STAT_SEND_OK");
                strcat(msg," => CH395_SINT_STAT_SEND_OK state cleared, setting CH395_SINT_STAT_RECV now (recv is performed)");
                //

                switch(socket)
                {
                    case 0:
                        ch395->glob_int_status |= CH395_GINT_STAT_SOCK0;
                        break;
                    case 1:
                        ch395->glob_int_status |= CH395_GINT_STAT_SOCK1;
                        break;
                    case 2:
                        ch395->glob_int_status |= CH395_GINT_STAT_SOCK2;
                        break;
                    case 3:
                        ch395->glob_int_status |= CH395_GINT_STAT_SOCK3;
                        break;
                }

            }

            if (data & CH395_SINT_STAT_SENBUF_FREE)
            {
                strcat(msg," CH395_SINT_STAT_SENBUF_FREE cleared");
            }

            printf("<<[CH395][READ][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d %s\n", socket, msg);
            dbg_printf("<<[CH395][READ][DATA][CH395_CMD_GET_INT_STATUS_SN] Socket : %d %s\n", socket, msg);
            //ch395->cmd_data.CMD_SocketGetIntStatusSn[0] = 0; // Reset status
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
            break;
        case CH395_CMD_SET_GWIP_ADDR:
            break;
        case CH395_CMD_SET_MASK_ADDR :
            break;
        case CH395_CMD_SET_MAC_FILT:
            break;

        case CH395_CMD_GET_PHY_STATUS:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_GET_PHY_STATUS]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_GET_PHY_STATUS]\n");

            ch395->command = CH395_CMD_GET_PHY_STATUS;
            break;

        case CH395_CMD_INIT:
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_INIT]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_INIT]\n");
            ch395->command = CH395_CMD_INIT;
            ch395->is_init = CH395_TRUE;
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
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_IPRAW_PRO_SN]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_SET_IPRAW_PRO_SN]\n");
            ch395->command = CH395_CMD_SET_IPRAW_PRO_SN;
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

            ch395_fill_get_ip_inf_linux(ch395);


            // Let's get IP and so on
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
            printf(">>[CH395][WRITE][COMMAND][CH395_CMD_SET_TTL]\n");
            dbg_printf("[CH395][WRITE][COMMAND][CH395_CMD_SET_TTL]\n");
            ch395->command = CH395_CMD_SET_TTL;
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
            break;
        case CH395_CMD_SET_KEEP_LIVE_INTVL:
            break;
        case CH395_CMD_SET_KEEP_LIVE_CNT:
            break;
        case CH395_CMD_SET_KEEP_LIVE_SN:
            break;
        case CH395_CMD_EEPROM_ERASE:
            break;
        case CH395_CMD_EEPROM_WRITE:
            break;
        case CH395_CMD_EEPROM_READ:
            break;
        case CH395_CMD_READ_GPIO_REG:
            break;
        case CH395_CMD_WRITE_GPIO_REG:
            break;
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
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_PHY_STATUS] can not accepted data on data port!\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_PHY_STATUS] can not accepted data on data port!\n");
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
                printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_PROTO_TYPE_SN][ERROR] Too much data");
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
                    ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = SOCK_STREAM;
                }

                if (data == CH395_PROTO_TYPE_UDP)
                {
                    printf("Setting CH395_PROTO_TYPE_UDP\n");
                    dbg_printf("Setting CH395_PROTO_TYPE_UDP\n");
                    ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = SOCK_DGRAM;
                }

                if (data == CH395_PROTO_TYPE_MAC_RAW)
                {
                    printf("Setting CH395_PROTO_TYPE_MAC_RAW\n");
                    dbg_printf("Setting CH395_PROTO_TYPE_MAC_RAW\n");
                    ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = SOCK_RAW;
                }

                if (data == CH395_PROTO_TYPE_IP_RAW)
                {
                    printf("Setting CH395_PROTO_TYPE_IP_RAW\n");
                    dbg_printf("Setting CH395_PROTO_TYPE_IP_RAW\n");
                    ch395->socket_proto[ch395->cmd_data.CMD_SocketSetProto[0]] = SOCK_RAW;
                }
                // Store Proto into socket_proto

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
                ch395->socket_status_sn[data][0] = CH395_SOCKET_OPEN;


                ch395->sockfd_host[data] = socket(AF_INET, ch395->socket_proto[data], 0);
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

                // Set SINT_STAT_SEND_OK
                ch395->socket_int_status[ch395->cmd_data.CMD_SocketWriteBuffer[0]] = ch395->socket_int_status[ch395->cmd_data.CMD_SocketWriteBuffer[0]]  | CH395_SINT_STAT_SENBUF_FREE;

                p = &ch395->buffer[ch395->receive_buffer_start_block[ch395->cmd_data.CMD_SocketWriteBuffer[0]]*CH395_SIZE_BLOCK_BUFFER];
                // Réception des données

                ch395->socket_int_status[ch395->cmd_data.CMD_SocketState[0]] = ch395->socket_int_status[ch395->cmd_data.CMD_SocketState[0]] | CH395_SINT_STAT_SEND_OK;

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
            printf(">>[CH395][WRITE][DATA][CH395_CMD_READ_RECV_BUF_SN]");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_READ_RECV_BUF_SN]");

            switch(ch395->nb_bytes_in_cmd_data)
            {
                case 0:
                    printf("Socket : %d\n",data);
                    dbg_printf("Socket : %d\n",data);
                    break;
                case 1:
                    printf("low length : %d\n",data);
                    dbg_printf("low length : %d\n",data);
                    break;
                case 2:
                    printf("high length %d\n",data);
                    dbg_printf("high length %d\n",data);
                    break;
                default:
                    printf("Panic\n");
                    dbg_printf("Panic\n");
                    break;
            }
            if (ch395->nb_bytes_in_cmd_data < 4 )
            {
                ch395->cmd_data.CMD_SocketGetRecvBuf[ch395->nb_bytes_in_cmd_data] = data;
                ch395->nb_bytes_in_cmd_data ++;
            }

            break;

        case CH395_CMD_CLOSE_SOCKET_SN:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_CLOSE_SOCKET_SN] Socket %d\n", data);
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_CLOSE_SOCKET_SN] Socket %d\n", data);
            close(ch395->sockfd_host[data]);
            break;

        case CH395_CMD_SET_IPRAW_PRO_SN:
            char *msg = ">>[CH395][WRITE][DATA][CH395_CMD_SET_IPRAW_PRO_SN] Socket";

            if  (ch395->pos_rw_in_cmd_data == 0) {
                printf("%s %d\n",msg, data);
                dbg_printf("%s %d\n",msg, data);
                ch395->pos_rw_in_cmd_data ++;
            }
            else {
                char *msg_panic = "PANIC : CH395_CMD_SET_IPRAW_PRO_SN can not receive 2 bytes on data port";
                printf("%s %d\n",msg, data);
                dbg_printf("%s %d\n",msg, data);
            }
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
            printf(">>[CH395][WRITE][DATA][CH395_CMD_GET_IP_INF] CH395 PANIC impossible to write into DATA port with CH395_CMD_GET_IP_INF\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_GET_IP_INF] CH395 PANIC impossible to write into DATA port with CH395_CMD_GET_IP_INF\n");
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

            switch(ch395->nb_bytes_in_cmd_data)
            {
                char *msg = ">>[CH395][WRITE][DATA][CH395_CMD_SET_TTL]";
                case 0:

                    ch395->cmd_data.CMD_SocketTTL[0] = data;
                    printf("%s Setting socket : %d\n",msg, data);
                    dbg_printf("%s %d\n",msg, data);
                    ch395->pos_rw_in_cmd_data ++;
                    break;
                case 1:
                    if (data > 128)
                    {
                        printf("%s PANIC : CH395_CMD_SET_TTL can not have a value greater than 128 received : %d\n",msg, data);
                        dbg_printf("%s PANIC : CH395_CMD_SET_TTL can not have a value greater than 128 received : %d\n",msg, data);
                    }
                    else
                    {
                        printf("%s socket : %d TTL : \n",msg, ch395->cmd_data.CMD_SocketTTL[0], data);
                        ch395->socket_ttl[ch395->cmd_data.CMD_SocketTTL[0]] = data;
                        ch395->pos_rw_in_cmd_data ++;
                    }
                default:
                    printf("%s PANIC : CH395_CMD_SET_TTL can not receive 3 bytes on data port %d\n",msg, data);
                    dbg_printf("%s PANIC : CH395_CMD_SET_TTL can not receive 3 bytes on data port %d\n",msg, data);
                    break;

            }
            break;

        case CH395_CMD_SET_RECV_BUF:
            break;

        case CH395_CMD_SET_SEND_BUF:
            break;

        case CH395_CMD_SET_FUN_PARA:
            printf(">>[CH395][WRITE][DATA][CH395_CMD_SET_FUN_PARA] Not emulated\n");
            dbg_printf("[CH395][WRITE][DATA][CH395_CMD_SET_FUN_PARA] Not emulated\n");
            break;

        case CH395_CMD_SET_KEEP_LIVE_IDLE:
            break;
        case CH395_CMD_SET_KEEP_LIVE_INTVL:
            break;
        case CH395_CMD_SET_KEEP_LIVE_CNT:
            break;
        case CH395_CMD_SET_KEEP_LIVE_SN:
            break;
        case CH395_CMD_EEPROM_ERASE:
            break;
        case CH395_CMD_EEPROM_WRITE:
            break;
        case CH395_CMD_EEPROM_READ:
            break;
        case CH395_CMD_READ_GPIO_REG:
            break;
        case CH395_CMD_WRITE_GPIO_REG:
            break;
        default:
            printf(">>[CH395][UNKNOWN]\n");
            break;

    }


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
    return (Uint8) 1;
}

SDL_bool ch395_write(struct machine *oric, SDL_bool fBank, unsigned int instance, Uint16 addr, Uint8 data)
{
    fBank = fBank; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return SDL_FALSE;

    instance--;

    if (addr == 0x00)
    {
        ch395_write_data_port(userdata[instance], data);
        return CH395_TRUE;
    }

    if (addr == 0x01)
    {
        ch395_write_command_port(userdata[instance], data);
        return CH395_TRUE;
    }

    return CH395_TRUE;

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

    return SDL_TRUE;
}



// -------------------------------------------------------------------------
//                  Mise à jour de la page du moniteur
// -------------------------------------------------------------------------
// Monitor page
// Rows: 19 (1-19)
// Columns: 28 (1-28)
void mon_ch395_update(struct textzone *ptz, unsigned int instance, Uint16 base_addr, SDL_bool oldvalid, SDL_bool run)
{
    int i;
    base_addr = base_addr; // gcc [-Wunused-parameter]

    if ( (!instance) || (instance > plugin_instances) )
        return;

    instance--;

    //ptz->cfc = 5;
    dbg_printf("ch395: mon update (instance=%d)\n", instance);
    //ch395->socket_status_sn[i][0]
    my_tzprintfpos( ptz, 2, 2,  "Initialized :  %02d", userdata[instance]->is_init);
    my_tzprintfpos( ptz, 2, 3,  "S |  BUFBLOCK   | STATE |   FLAGS  | SN");
    int pos_state = 4;
    for (i = 0; i < 8 ; i++)
    {
        // Display socket ID
        my_tzprintfpos( ptz, 2, 4 + i,  "%d",i);
        pos_state = 5;
        my_tzprintfpos( ptz, pos_state, 4 + i,  "R%02d/%02d", userdata[instance]->receive_buffer_start_block[i], userdata[instance]->receive_buffer_number_of_block[i]);
        pos_state += 7;
        my_tzprintfpos( ptz, pos_state, 4 + i,  "T%02d/%02d", userdata[instance]->transmit_buffer_start_block[i], userdata[instance]->transmit_buffer_number_of_block[i]);
        pos_state += 7;
        //
        // Display block


        switch(userdata[instance]->socket_status_sn[i][0])
        {
            case CH395_SOCKET_CLOSED:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "CLOSED");
                break;
            case CH395_SOCKET_OPEN:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "OPENED");
                break;
            default:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "ERR");
                break;
        }
        pos_state += 8;
        switch(userdata[instance]->socket_status_sn[i][1])
        {
            case CH395_TCP_CLOSED:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "TCP_CLOSED");
                break;
            case CH395_TCP_LISTEN:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "TCP_LISTEN");
                break;
            case CH395_TCP_ESTABLISHED:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "TCP_ESTABL");
                break;
            default:
                my_tzprintfpos( ptz, pos_state, 4 + i,  "ERR");
                break;
        }



        pos_state += 12;

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_TIM_OUT)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "TIM_OUT");
            pos_state += strlen("TIM_OUT");
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_SEND_OK)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "SEND_OK");
            pos_state += strlen("SEND_OK") + 1;
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_RECV)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "RECV");
            pos_state += strlen("RECV");
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_SENBUF_FREE)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "SENBUF_FREE");
            pos_state += strlen("SENBUF_FREE");
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_CONNECT)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "CONNECT");
            pos_state += strlen("CONNECT");
        }

        if (userdata[instance]->socket_int_status[i] & CH395_SINT_STAT_DISCONNECT)
        {
            my_tzprintfpos( ptz, pos_state, 4 + i,  "DISCONNECT");
            pos_state += strlen("DISCONNECT");
        }
    }

    // if (userdata[instance]->ch395->glob_int_status & )
 /*


    // Trait de séparation en ligne 8
    ptz->px = 0;
    ptz->py = 8;
    my_tzputc( ptz, 6 );

    for (int i=0; i < ptz->w-2; i++)
//        my_tzputc( ptz, 2 );
        my_tzputc( ptz, 12 );

    my_tzputc( ptz, 8 );

    my_tzprintfpos( ptz, 2, 9 , "Twil register : $%02X", twilighte->t_register );

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

}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
struct PLUGIN plugin = { "ch395",
                BASE_ADDR, END_ADDR-BASE_ADDR+1,
                PLG_DEVICE,
                NULL,
                ch395_create,
                ch395_shutdown,
                ch395_reset,
                ch395_read,
                ch395_write,
                NULL,
                mon_ch395_update,
                mon_ch395_store,
    };



