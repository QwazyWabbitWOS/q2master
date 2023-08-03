/*
* Copyright (C) 2002-2003 r1ch.net
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

//Originally gloommaster 0.0.3 (c) 2002-2003 r1ch.net
//quake 2 compatible master server for gloom

// QwazyWabbit mods begin
// 11-FEB-2006
// Changes for Linux and Windows portability by Geoff Joy (QwazyWabbit)
// Simply build for Linux with "gcc -o q2master master.c"
//
// 26-JAN-2007
// Made it a general q2 server (q2master v1.0)
//
// 26-JUL-2007
// Added registry keys to tell service what IP to use.
// Added key and handling for non-standard port.
// Server could be modified for use as Q3 or Q4 master server.
//
// 18-AUG-2007
// Dynamic allocation of buffer in SendServerListToClient function.
// 
// 01-SEP-2007
// General release. This project builds in VC++ 6.0 and GCC 4.x
// Complete project easily ported to VC 2005 or higher.
//

// General command line switches:

// -debug	Asserts debug mode. The program prints status messages to console
//			while running. Shows pings, heartbeats, number of servers listed.

// -ip xxx.xxx.xxx.xxx causes server to bind to a particular IP address when
//	used with multi-homed hosts. Default is 0.0.0.0 (any).

// -port xxxxx causes server to bind to a particular port. Default is 27900.
// Default port for Quake 2 master servers is 27900. If you depart from this
// you need to communicate this to your users somehow. This feature is included
// since this code could be modified to provide master services for other games.

// *** Windows ***

// Usage:
// Place executable in %systemroot%\system32 or other known location.
// To debug it as a console program, command: "master -debug" and it outputs
// status messages to the console. Ctrl-C shuts it down.
//
// From a cmd prompt type: q2master -install to install the service with defaults.
// The service will be installed as "Q2MasterServer" in the Windows service list.
//

// -install	Installs the service on Windows.
// -remove	Stops and removes the service.
// When the service is installed it is installed with "Automatic" startup type
// which allows it to start up when Windows starts. Use SCM to change this.

// Other commands:
// net start q2masterserver to start the service.
// net stop q2masterserver to stop the service.
//
// Use the Services control panel applet to start/stop, change startup modes etc.
// To uninstall the server type "master -remove" to stop and remove the active service.

// *** Linux ***

// Usage:
// -debug Sets process to debug mode and it remains attached to console.
// If debug is not specified on command line the process forks a daemon and
// detaches from terminal.
//
// Send the process a SIGTERM to stop the daemon. "kill -n SIGTERM <pid>"
// Use "netstat -anup" to see the processes/pids listening on UDP ports.
// Use "ps -ux" to list detached processes, this will show the command line that
// invoked the q2master process.
// 
// *** Mac/iMac OS/X ***
// Usage:
// Same as Linux
// Compiles on OS/X same as Linux & BSD "gcc -o q2masterd master.c".
//

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock.h>
#include <winerror.h>
#include <conio.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h> 
#include <time.h>
#include <assert.h>
#include <crtdbg.h>
#include "service.h"

// Windows Service structs
SERVICE_STATUS          MyServiceStatus;
SERVICE_STATUS_HANDLE   MyServiceStatusHandle;

// Windows console handles
HANDLE hStdin;
HANDLE hStdout;

WSADATA ws;

#else

// Linux and Mac versions
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <sys/signal.h>
#include <assert.h>

enum { FALSE, TRUE };

typedef unsigned char BYTE;
typedef BYTE far      *LPBYTE;

// stuff not defined in sys/socket.h
#define SOCKET unsigned int
#define SOCKET_ERROR -1
#define TIMEVAL struct timeval

void signal_handler(int sig);	// for Linux & OS X

#endif

int runmode;	// server loop control
int Debug;		// for debugging as a console application in Windows or Linux
unsigned long numservers;	// global count of the currently listed servers

enum	// the run modes
{
	SRV_STOP, // stop request
	SRV_RUN,  // running OK
	SRV_STOPPED, // server has stopped
	SRV_PAUSE,   // service pause request
	SRV_PAUSED   // service is paused
};

typedef struct server_s server_t;

struct server_s {
	server_t		*prev;
	server_t		*next;
	struct sockaddr_in	ip;
	unsigned short	port;
	unsigned int	queued_pings;
	unsigned int	heartbeats;
	time_t			last_heartbeat;
	time_t			last_ping;
	unsigned char	shutdown_issued;
	unsigned char	validated;
};

server_t servers;

struct sockaddr_in listenaddress;
SOCKET out;
SOCKET listener;
TIMEVAL delay;

fd_set set;
char incoming[150000];

#define KEY_LEN 32
char bind_ip[] = "000.000.000.000"; // default IP to bind
char bind_port[] = "27900";	// default port to bind

//
// These are Windows specific but need to be defined here so GCC won't barf
//
#define REGKEY_SUBKEY "SOFTWARE\\Q2MasterServer" // Our config data goes here
#define REGKEY_BIND_IP "Bind_IP"
#define REGKEY_BIND_PORT "Bind_Port"

// Out of band data preamble
#define OOB_SEQ "\xff\xff\xff\xff" //32 bit integer (-1) as string sequence signifying out of band data
#define SERVERSTRING OOB_SEQ"servers " // 12 bytes for the serverstring header (note no space here)

void Ack(struct sockaddr_in *from);
int  AddServer(struct sockaddr_in *from, int normal);
void DropServer(server_t *server);
void ExitNicely(void);
void ForkDaemon(void);
int  HeartBeat(struct sockaddr_in *from, char *data);
//void InitConsole(void);
void ParseCommandLine(int argc, char *argv[]);
int  ParseResponse(struct sockaddr_in *from, char *data, int dglen);
void QueueShutdown(struct sockaddr_in *from, server_t *myserver);
void RunFrame(void);
int  ReceivePackets(void);
void SendServerListToClient(struct sockaddr_in *from);
void SetRegKey(HKEY hive, LPCSTR subkey, LPCSTR name, LPBYTE value);
void GetRegKey(HKEY hive, LPSTR subkey, LPCSTR name, LPBYTE value);
int	Q_stricmp(const char *s1, const char *s2);
int Q_strnicmp(const char *s1, const char *s2, size_t n);
void MasterSleep(int msec);

//
// Portable wrapper for WSAGetLastError
//
int SocketGetLastError(void)
{
#ifdef _WIN32
	return (WSAGetLastError());
#else
	return errno;
#endif
}

// fast "C" macros
#define Q_isupper(c)    ((c) >= 'A' && (c) <= 'Z')
#define Q_islower(c)    ((c) >= 'a' && (c) <= 'z')
#define Q_isdigit(c)    ((c) >= '0' && (c) <= '9')
#define Q_isalpha(c)    (Q_isupper(c) || Q_islower(c))
#define Q_isalnum(c)    (Q_isalpha(c) || Q_isdigit(c))
#define Q_isprint(c)    ((c) >= 32 && (c) < 127)
#define Q_isgraph(c)    ((c) > 32 && (c) < 127)
#define Q_isspace(c)    (c == ' ' || c == '\f' || c == '\n' || \
                         c == '\r' || c == '\t' || c == '\v')

int inline Q_tolower(int c)
{
	if (Q_isupper(c)) {
		c += ('a' - 'A');
	}
	return c;
}

// Case independent string compare.
// If s1 is contained within s2 then return 0, they are "equal"
// else return the difference between them.
int	Q_stricmp(const char *s1, const char *s2)
{
	const unsigned char
		*uc1 = (const unsigned char *)s1,
		*uc2 = (const unsigned char *)s2;

	while (Q_tolower(*uc1) == Q_tolower(*uc2++))
		if (*uc1++ == '\0')
			return (0);
	return (Q_tolower(*uc1) - Q_tolower(*--uc2));
}

// case independent string compare of length n
// compare strings up to length n or until the end of s1
// if s1 is contained within s2 then return 0
// else return the lexicographical difference between them.
int Q_strnicmp(const char *s1, const char *s2, size_t n)
{
	const unsigned char
		*uc1 = (const unsigned char *)s1,
		*uc2 = (const unsigned char *)s2;

	if (n != 0) {
		do {
			if (Q_tolower(*uc1) != Q_tolower(*uc2++))
				return (Q_tolower(*uc1) - Q_tolower(*--uc2));
			if (*uc1++ == '\0')
				break;
		} while (--n != 0);
	}
	return (0);
}

//
// Debug print output
//
void dprintf(char *msg, ...)
{
	va_list		argptr;
	char		text[128];

	va_start(argptr, msg);
	vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	if (Debug)
		printf("%s", text);
}

//
// This becomes main for Linux
// In Windows, main is in service.c and it 
// decides if we're going to see a console or not.
//
#ifdef _WIN32
int My_Main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	int err;

	printf("Q2-Master 1.1 originally GloomMaster\n(c) 2002-2003 r1ch.net, modifications by QwazyWabbit 2007-2014\n");
	numservers = 0;

#ifdef _WIN32
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	err = WSAStartup((WORD)MAKEWORD(1, 1), &ws);
	if (err)
	{
		printf("Error loading Windows Sockets! Error: %i\n", err);
		return (err);
	}
#else
	ParseCommandLine(argc, argv); 	// done in ServiceStart() if Windows
#endif

	printf("Debugging mode: %i\n", Debug);
	listener = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	out = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	memset(&listenaddress, 0, sizeof(listenaddress));

	// only in Windows
	GetRegKey(HKEY_LOCAL_MACHINE, REGKEY_SUBKEY, REGKEY_BIND_IP, (LPBYTE)bind_ip);
	GetRegKey(HKEY_LOCAL_MACHINE, REGKEY_SUBKEY, REGKEY_BIND_PORT, (LPBYTE)bind_port);

	listenaddress.sin_addr.s_addr = inet_addr(bind_ip);
	listenaddress.sin_family = AF_INET;
	listenaddress.sin_port = htons((unsigned short)atoi(bind_port));

	if ((bind(listener, (struct sockaddr *)&listenaddress, sizeof(listenaddress))) == SOCKET_ERROR)
	{
		dprintf("[E] Couldn't bind to port %s UDP\n", bind_port);
		return EXIT_FAILURE;
	}

	delay.tv_sec = 1;
	delay.tv_usec = 0;
	FD_SET(listener, &set);
	memset(&servers, 0, sizeof(servers));
	dprintf("listening on %s:%s (UDP)\n", bind_ip, bind_port);
	runmode = SRV_RUN;
	ForkDaemon();

	while (runmode == SRV_RUN)
	{
		ReceivePackets();
		RunFrame();		// destroy old servers, etc
	}

#ifdef _WIN32
	WSACleanup();
	_CrtDumpMemoryLeaks();
#endif

	runmode = SRV_STOPPED;
	return(err);
}

//
// Fork daemon in Linux/OS X/BSD, does nothing in Windows
//
void ForkDaemon(void)
{
#ifndef _WIN32
	if (!Debug)
	{
		if (daemon(0, 0) < 0)
		{
			printf("Forking error, running as console, error was: %i\n", errno);
			Debug = TRUE;
		}
		else	//fork succeeded
		{
			signal(SIGCHLD, SIG_IGN); /* ignore child */
			signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
			signal(SIGTTOU, SIG_IGN);
			signal(SIGTTIN, SIG_IGN);
			signal(SIGHUP, signal_handler); /* catch hangup signal */
			signal(SIGTERM, signal_handler); /* catch terminate signal */
		}
	}
#endif
}

//
// called by main, this listens on the socket
// for messages and dispatches them to the parser
//
int ReceivePackets(void)
{
	int len;
	int err;
	int ns;
	unsigned fromlen;
	struct sockaddr_in from = { 0 };

	err = 0;
	fromlen = (unsigned) sizeof(from);

	FD_SET(listener, &set);
	delay.tv_sec = 1;
	delay.tv_usec = 0;

	ns = select(listener + 1, &set, NULL, NULL, &delay);
	if (ns == 1)
	{
		len = recvfrom(listener, incoming, sizeof(incoming), 0, (struct sockaddr *)&from, &fromlen);
		if (len != SOCKET_ERROR)
		{
			if (len > 4)
			{
				if (!ParseResponse(&from, incoming, len)) {
					err = 50;
					runmode = SRV_STOP;	// something went wrong, AddServer failed?
				}
			}
			else
				dprintf("[W] runt packet from %s:%d\n", inet_ntoa(from.sin_addr), ntohs(from.sin_port));

			//reset for next packet
			memset(incoming, 0, sizeof(incoming));
		}
		else
		{
			err = SocketGetLastError();
			dprintf("[E] socket error during select from %s:%d (%d)\n",
				inet_ntoa(from.sin_addr), ntohs(from.sin_port), err);
		}
	}
	return (err);
}

//
// Called by ServiceCtrlHandler after the server loop is stopped.
//
void ExitNicely(void)
{
	server_t	*server = &servers;
	server_t	*old = NULL;

	printf("[I] shutting down.\n");
	while (server->next)
	{
		if (old) free(old);
		server = server->next;
		old = server;
	}
	if (old) free(old);
}

void DropServer(server_t *server)
{
	if (!server)
		return;

	//unlink
	if (server->next)
		server->next->prev = server->prev;

	if (server->prev)
		server->prev->next = server->next;

	if (numservers != 0)
		numservers--;

	free(server);
}

//
// returns TRUE if successfully added to list
//
int AddServer(struct sockaddr_in *from, int normal)
{
	server_t	*server = &servers;
	int			preserved_heartbeats = 0;

	while (server->next)
	{
		server = server->next;
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port)
		{
			//already exists - could be a pending shutdown (ie killserver, change of map, etc)
			if (server->shutdown_issued)
			{
				dprintf("[I] scheduled shutdown server %s sent another ping!\n",
					inet_ntoa(from->sin_addr));
				DropServer(server);
				server = &servers;
				while (server->next)
					server = server->next;
				break;
			}
			else
			{
				dprintf("[W] dupe ping from %s:%u!! ignored.\n",
					inet_ntoa(server->ip.sin_addr),
					htons(server->port));
				return TRUE;
			}
		}
	}

	server->next = (server_t *)malloc(sizeof(server_t));
	// Note: printf implicitly calls malloc so this is likely to fail if we're really out of memory
	if (server->next == NULL)
	{
		printf("Fatal Error: memory allocation failed in AddServer\n");
		exit(EXIT_FAILURE);
	}

	server->next->prev = server;
	server = server->next;
	server->heartbeats = preserved_heartbeats;
	memcpy(&server->ip, from, sizeof(server->ip));
	server->last_heartbeat = time(NULL);
	server->next = NULL;
	server->port = from->sin_port;
	server->shutdown_issued = 0;
	server->queued_pings = 0;
	server->last_ping = 0;
	server->validated = 0;
	numservers++;

	dprintf("[I] server %s:%u added to queue! (%d) number: %u\n",
		inet_ntoa(from->sin_addr),
		htons(server->port),
		normal,
		numservers);

	if (normal)
	{
		struct sockaddr_in addr = { 0 };
		memcpy(&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
		sendto(listener, OOB_SEQ"ack", 7, 0, (struct sockaddr *)&addr, sizeof(addr));
	}
	return TRUE;
}

//
// We received a shutdown frame from a server, set the shutdown flag
// and send it a ping to ack the shutdown frame.
//
void QueueShutdown(struct sockaddr_in *from, server_t *myserver)
{
	server_t	*server = &servers;

	if (!myserver)
	{
		while (server->next)
		{
			server = server->next;
			if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port)
			{
				myserver = server;
				break;
			}
		}
	}

	if (myserver)
	{
		struct sockaddr_in addr = { 0 };
		memcpy(&addr.sin_addr, &myserver->ip.sin_addr, sizeof(addr.sin_addr));
		addr.sin_family = AF_INET;
		addr.sin_port = server->port;
		memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
		//hack, server will be dropped in next minute IF it doesn't respond to our ping
		myserver->shutdown_issued = 1;
		dprintf("[I] shutdown queued %s:%u \n", inet_ntoa(myserver->ip.sin_addr), htons(server->port));

		sendto(listener, OOB_SEQ"ping", 8, 0, (struct sockaddr *)&addr, sizeof(addr));
		return;
	}

	else
		dprintf("[W] shutdown issued from unregistered server %s!\n", inet_ntoa(from->sin_addr));
}

//
// Walk the server list and ping them as needed, age the ones
// we have not heard from in a while and when they get too
// old, remove them from the list.
//
void RunFrame(void)
{
	server_t		*server = &servers;
	time_t	curtime = time(NULL);

	while (server->next)
	{
		server = server->next;
		if (curtime - server->last_heartbeat > 600)
		{
			server_t *old = server;

			server = old->prev;

			if (old->shutdown_issued || old->queued_pings > 6)
			{
				dprintf("[I] %s:%u shut down.\n", inet_ntoa(old->ip.sin_addr), htons(server->port));
				DropServer(old);
				continue;
			}

			server = old;

			if (curtime - server->last_ping >= 10)
			{
				struct sockaddr_in addr = { 0 };
				memcpy(&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
				addr.sin_family = AF_INET;
				addr.sin_port = server->port;
				memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
				server->queued_pings++;
				server->last_ping = curtime;
				dprintf("[I] ping %s:%u\n", inet_ntoa(server->ip.sin_addr), htons(server->port));
				sendto(listener, OOB_SEQ"ping", 8, 0, (struct sockaddr *)&addr, sizeof(addr));
			}
		}
	}
}

//
// This function assembles the serverstring preamble and 6 bytes for each
// listed server into a buffer for transmission to the client in response
// to a query frame.
//
void SendServerListToClient(struct sockaddr_in *from)
{
	int				buflen;
	char			*buff;
	server_t		*server = &servers;
	unsigned long	servercount;
	unsigned long	bufsize;
	int		err;

	// assume buffer size needed is for all current servers (numservers)
	// and eligible servers in list will always be less than or equal to numservers

	err = 0;
	bufsize = 12 + 6 * (numservers + 1); // 12 bytes for serverstring, 6 bytes for game server ip and port
	buflen = 0;
	buff = malloc(bufsize);
	if (buff == NULL) {
		printf("Fatal Error: memory allocation failed in SendServerListToClient\n");
		exit(EXIT_FAILURE);
	}

	memset(buff, 0, bufsize);
	memcpy(buff, SERVERSTRING, strlen(SERVERSTRING));	// 12 is length of serverstring
	buflen += 12;
	servercount = 0;

	while (server->next)
	{
		server = server->next;
		if (server->heartbeats >= 2 && !server->shutdown_issued && server->validated)
		{
			memcpy(buff + buflen, &server->ip.sin_addr, 4);
			buflen += 4;

			memcpy(buff + buflen, &server->port, 2);
			buflen += 2;

			servercount++;
		}
	}

	dprintf("[I] query response (%d bytes) sent to %s:%d\n", buflen, inet_ntoa(from->sin_addr), ntohs(from->sin_port));

	err = sendto(listener, buff, buflen, 0, (struct sockaddr *)from, sizeof(*from));
	if (err == SOCKET_ERROR)
	{
		err = SocketGetLastError();
		dprintf("[E] socket error on send! code %d.\n", err);
	}

	dprintf("[I] sent server list to client %s, servers: %u of %u\n",
		inet_ntoa(from->sin_addr),
		servercount, /* sent */
		numservers); /* on record */

	free(buff);
}

//
// We pinged a server, received the ack
// now locate the server in the database
// and update his last ack time
//
void Ack(struct sockaddr_in *from)
{
	server_t	*server = &servers;

	//iterate through known servers
	while (server->next)
	{
		server = server->next;
		//a match!
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port)
		{
			dprintf("[I] ack from %s:%u (%d).\n",
				inet_ntoa(server->ip.sin_addr),
				htons(server->port),
				server->queued_pings);

			server->last_heartbeat = time(NULL);
			server->queued_pings = 0;
			server->heartbeats++;
			return;
		}
	}
}

//
// We received a heartbeat from a server. Ack it and see if
// it matches one we have in the database, if so
// update his heartbeat time, if not
// add it to the list.
//
int HeartBeat(struct sockaddr_in *from, char *data)
{
	server_t	*server = &servers;
	int status;

	status = TRUE;

	//walk through known servers
	while (server->next)
	{
		server = server->next;
		//a match!
		if (*(int *)&from->sin_addr == *(int *)&server->ip.sin_addr && from->sin_port == server->port)
		{
			struct sockaddr_in addr = { 0 };

			memcpy(&addr.sin_addr, &server->ip.sin_addr, sizeof(addr.sin_addr));
			addr.sin_family = AF_INET;
			addr.sin_port = server->port;
			memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

			server->validated = 1;
			server->last_heartbeat = time(NULL);
			dprintf("[I] heartbeat from %s:%u.\n",
				inet_ntoa(server->ip.sin_addr),
				htons(server->port));

			sendto(listener, OOB_SEQ"ack", 7, 0, (struct sockaddr *)&addr, sizeof(addr));
			return status;
		}
	}
	//we didn't find server in our list
	status = AddServer(from, 0);
	return status; // false if AddServer failed
}

//
// determine what kind of packet we have.
// dispatch the functions accordingly
//
int ParseResponse(struct sockaddr_in *from, char *data, int dglen)
{
	char *cmd = data;
	char *line = data;
	int	status = TRUE;

	if (Q_strnicmp(data, "query", 5) == 0 || Q_strnicmp(data, OOB_SEQ"getservers", 14) == 0)
	{
		dprintf("[I] %s:%d : query (%d bytes)\n", inet_ntoa(from->sin_addr), htons(from->sin_port), dglen);
		SendServerListToClient(from);
	}
	else
	{
		while (*line && *line != '\n')
			line++;

		*(line++) = '\0';
		cmd += 4;

		dprintf("[I] %s: %s (%d bytes)\n", cmd, inet_ntoa(from->sin_addr), dglen);

		if (Q_strnicmp(cmd, "ping", 4) == 0)
		{
			status = AddServer(from, 1);
		}
		else if (Q_strnicmp(cmd, "heartbeat", 9) == 0 || Q_strnicmp(cmd, "print", 5) == 0)
		{
			status = HeartBeat(from, line);
		}
		else if (strncmp(cmd, "ack", 3) == 0)
		{
			Ack(from);
		}
		else if (Q_strnicmp(cmd, "shutdown", 8) == 0)
		{
			QueueShutdown(from, NULL);
		}
		else
		{
			dprintf("[W] Unknown command from %s!\n", inet_ntoa(from->sin_addr));
		}
	}
	return status;
}

void ParseCommandLine(int argc, char *argv[])
{
	int i = 0;

	if (argc >= 2)
		Debug = 3; //initializing
	for (i = 1; i < argc; i++)
	{
		if (Debug == 3)
		{
			if (Q_strnicmp(argv[i] + 1, "debug", 5) == 0)
				Debug = TRUE;	//service debugged as console
			else
				Debug = FALSE;
		}

		if (Q_strnicmp((char*)argv[i] + 1, "ip", 2) == 0)
		{
			//bind_ip, a specific host ip if desired
			strncpy(bind_ip, (char*)argv[i] + 4, sizeof(bind_ip) - 1);
			SetRegKey(HKEY_LOCAL_MACHINE, REGKEY_SUBKEY, REGKEY_BIND_IP, (LPBYTE)bind_ip);
		}

		if (Q_strnicmp((char*)argv[i] + 1, "port", 4) == 0)
		{
			//bind_port, if other than default port
			strncpy(bind_port, (char*)argv[i] + 6, sizeof(bind_port) - 1);
			SetRegKey(HKEY_LOCAL_MACHINE, REGKEY_SUBKEY, REGKEY_BIND_PORT, (LPBYTE)bind_port);
		}
	}
}

void MasterSleep(int msec)
{
#ifdef _WIN32
	Sleep(msec);
#else
	sleep(msec / 1000);
#endif
}

//
// This stuff plus a modified service.c and service.h
// from the Microsoft examples allows this application to be
// installed as a Windows service.
//

#ifdef _WIN32

void ServiceCtrlHandler(DWORD Opcode)
{
	switch (Opcode)
	{
	case SERVICE_CONTROL_STOP:
		// Kill the server loop. 
		runmode = SRV_STOP; // zero the loop control
		while (runmode == SRV_STOP)	//give loop time to die
		{
			int i = 0;

			MasterSleep(500);	// SCM times out in 3 secs.
			i++;		// we check twice per sec.
			if (i >= 6)	// hopefully we beat the SCM timer
				break;	// still no return? rats, terminate anyway
		}

		ExitNicely();

		MyServiceStatus.dwWin32ExitCode = 0;
		MyServiceStatus.dwCurrentState = SERVICE_STOPPED;
		MyServiceStatus.dwCheckPoint = 0;
		MyServiceStatus.dwWaitHint = 0;

		if (MyServiceStatusHandle)
			SetServiceStatus(MyServiceStatusHandle, &MyServiceStatus);
		return;
		break;

	case SERVICE_CONTROL_PAUSE:
		//stubbed
		SetServiceStatus(MyServiceStatusHandle, &MyServiceStatus);
		break;

	}
	// else just send current status. 
	SetServiceStatus(MyServiceStatusHandle, &MyServiceStatus);
}

void ServiceStart(DWORD argc, LPTSTR *argv)
{
	ParseCommandLine(argc, argv); // we call it here and in My_Main
	SetRegKey(HKEY_LOCAL_MACHINE, REGKEY_SUBKEY, "Description", (LPBYTE) "A Quake 2 master server listening on the listed IP and UDP port.");

	MyServiceStatus.dwServiceType = SERVICE_WIN32;
	MyServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	MyServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;// | SERVICE_ACCEPT_PAUSE_CONTINUE; 
	MyServiceStatus.dwWin32ExitCode = 0;
	MyServiceStatus.dwServiceSpecificExitCode = 0;
	MyServiceStatus.dwCheckPoint = 0;
	MyServiceStatus.dwWaitHint = 0;

	if (!Debug)
	{
		MyServiceStatusHandle = RegisterServiceCtrlHandler(argv[0],
			(LPHANDLER_FUNCTION)ServiceCtrlHandler);

		if (MyServiceStatusHandle == (SERVICE_STATUS_HANDLE)0)
		{
			printf("%s not started.\n", SZSERVICEDISPLAYNAME);
			return;
		}
		else
		{
			// Initialization complete - report running status. 
			MyServiceStatus.dwCurrentState = SERVICE_RUNNING;
			MyServiceStatus.dwCheckPoint = 0;
			MyServiceStatus.dwWaitHint = 0;
			SetServiceStatus(MyServiceStatusHandle, &MyServiceStatus);

		}
	}
	My_Main(argc, &argv[0]);
}

void ServiceStop(void)
{
	ServiceCtrlHandler(SERVICE_CONTROL_STOP);
}


/* Inputs:
	hive: typically HKEY_LOCAL_MACHINE
	subkey: the name of the subkey to be created/set
	name: the name of the key to be created/set
	value: the value to be assigned to the named key
Return:
	nothing
*/
/*
 * Example Usage:
 * SetRegKey(HKEY_CURRENT_USER, "Software/mySubKey", "myKeyName", "myValue");
 * SetRegKey(HKEY_CURRENT_USER, "Software/mySubKey", "Description",
 *		(LPBYTE) "A short description text goes here.");
 */
void SetRegKey(HKEY hive, LPCSTR subkey, LPCSTR name, LPBYTE value)
{
	HKEY	hKey;
	DWORD	Disposition;
	char	msg[200] = "";

	LONG status = RegCreateKeyEx(hive, subkey,
		0, /* must be 0 */
		NULL, REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,	// the rights to it
		NULL,			// pointer to security attributes
		&hKey,			// a place for the handle to the key
		&Disposition);	// REG_CREATED_NEW_KEY or REG_OPENED_EXISTING_KEY on return

	if (status != ERROR_SUCCESS)
	{
		sprintf(msg, "%s\\%s =\n%s\nfor %s\n", subkey, name, value, SZSERVICEDISPLAYNAME);
		MessageBox(hWnd, msg, "Error creating registry key!", MB_OK);
	}
	else
	{
		DWORD cbData = _tcslen((LPCSTR)value) + sizeof(TCHAR); // account for nul char
		status = RegSetValueEx(hKey, name, 0, REG_SZ, value, cbData);
		if (status != ERROR_SUCCESS)
		{
			sprintf(msg, "Error setting registry key:\n%s\\%s =\n%s\nfor %s\n", subkey, name, value, SZSERVICEDISPLAYNAME);
			MessageBox(hWnd, msg, "Registry key not set!", MB_OK);
		}
	}
	RegCloseKey(hKey);
}


/* Inputs:
	hive: typically HKEY_LOCAL_MACHINE or HKEY_CURRENT_USER
	subkey: the name of the subkey to be created/set
	name: the name of the key to be created/set
	value: the value to be assigned to the named key
Returns:
	nothing
*/
/*
 * Example Usage:
 *	GetRegKey(HKEY_CURRENT_USER, "Software/mySubKey", "myKeyName", (LPBYTE) value);
 */
void GetRegKey(HKEY hive, LPSTR subkey, LPCSTR name, LPBYTE value)
{
	HKEY	hKey;
	DWORD	Disposition;
	DWORD size = KEY_LEN;

	LONG status = RegCreateKeyEx(hive, subkey,
		0, /* must be 0 */
		NULL, REG_OPTION_NON_VOLATILE,
		KEY_READ, NULL, &hKey,
		&Disposition);

	if (status != ERROR_SUCCESS)
		MessageBox(hWnd, subkey, "Registry key not found\n", MB_OK);
	else
	{
		//	DWORD size = _tcslen((LPCSTR) value) + sizeof(TCHAR);
		status = RegQueryValueEx(hKey, name, NULL, NULL, value, &size);
		if (status != ERROR_SUCCESS)
			MessageBox(hWnd, subkey, "Registry value not found", MB_OK);
	}
	RegCloseKey(hKey);
}

#else	// not doing windows

void SetRegKey(HKEY hive, LPCSTR subkey, LPCSTR name, LPBYTE value) {} // stubs in Linux
void GetRegKey(HKEY hive, LPSTR subkey, LPCSTR name, LPBYTE value) {}

//
// handle Linux and BSD signals
//
void signal_handler(int sig)
{
	switch (sig)
	{
	case SIGHUP:
		break;
	case SIGTERM:
		runmode = SRV_STOP;
		while (runmode == SRV_STOP)	//give loop time to die
		{
			int i = 0;

			MasterSleep(500);	// 500 ms
			i++;
			if (i >= 6)
				break;
		}

		ExitNicely();
		break;
	}
}

#endif	// end OS-dependent stuff

// end of Q2master
