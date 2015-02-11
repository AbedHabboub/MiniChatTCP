/*
* File: wrsock.h
* Author: pascalfares
*
* Created on 6 décembre 2014, 13:35
*/
#ifndef WRSOCK_H
#define	WRSOCK_H
int writes(int, char *, int);
int reads(int, char *, int);
struct sockaddr_in *CreerSockAddr(char *, int);
int SockUdp(char *, int);
int SockTcp(char *nom, int numport);
int connect_tcp_socket(int fd, struct sockaddr_in *addr);
#endif	/* WRSOCK_H */
