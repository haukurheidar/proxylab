/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 *
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */

#include "csapp.h"

void doit(int clientfd, struct sockaddr_in *sockaddr);
void send_requestResponse(rio_t *rio_server, int serverfd, rio_t *rio_client, int clientfd, int *size);
int parse_uri(char *uri, char *target_addr, char *path, int *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

/* thread routine protocol */
void *thread(void* vargp);

//log file
FILE *proxylog;

/* thread routines kept in a struct */
typedef struct {
    int fd;
    struct sockaddr_in clientaddr;
} thread_strct;

/* Mutex used to lock before writing to common log file */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{
    //int listenfd, connfd, port, clientlen;
    int listenfd, port, clientlen;
    signal(SIGPIPE, SIG_IGN);
    /* thread id*/
    pthread_t tid;
    
    /* Check command line args */
    if (argc != 2)
    {
    	fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }

    port = atoi(argv[1]);

    //open and make log file
    proxylog = Fopen("proxy.log", "a");

    listenfd = Open_listenfd(port);
    while (1)
    {
        thread_strct *ts;
        clientlen = sizeof(ts->clientaddr);
        ts = Malloc(sizeof(thread_strct));
        ts->fd = Accept(listenfd, (SA *)&ts->clientaddr, &clientlen);
        Pthread_create(&tid, NULL, thread, ts);
    }
    Close(listenfd);
    Fclose(proxylog);

    return 0;
}

void *thread(void *vargp)
{
    thread_strct *ts = (thread_strct*)vargp;
 
    Pthread_detach(pthread_self());

    // Make read timeout after 1 sec if there is nothing to read on socket
    struct timeval t;
    t.tv_sec = 1; t.tv_usec = 0;
    setsockopt(ts->fd, SOL_SOCKET, SO_RCVTIMEO, (const void *)(&t), sizeof(t));

    doit(ts->fd,  &ts->clientaddr);

    // Close the client connection and free memmory
    Close(ts->fd);
    Free(ts);    

    return;
}

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int clientfd, struct sockaddr_in *sockaddr)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rioClient, rioServer;

    /* Read request line and headers */
    Rio_readinitb(&rioClient, clientfd);
    Rio_readlineb(&rioClient, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    /* Lets support GET */
    //if (strcasecmp(method, "GET"))
    //{
    	//clienterror(clientfd, method, "501", "Not Implemented", "method not supported");
        //return;
    //}

    /* Parse URI from request */
    char host[MAXLINE], path[MAXLINE];
    int serverPort;
    if ( -1 == parse_uri(uri, host, path, &serverPort) ) {
    	return;
    }

    /*Open connection to end server*/
    int serverResponseSize;
    int serverfd;
    serverfd = Open_clientfd(host, serverPort);
    // Make read timeout after 1 sec if there is nothing to read on socket
    struct timeval t;
    t.tv_sec = 4; t.tv_usec = 0;
    setsockopt(serverfd, SOL_SOCKET, SO_RCVTIMEO, (const void *)(&t), sizeof(t));

    /* Initialize rioServer and send headers */
     Rio_readinitb(&rioServer, serverfd);
     Rio_writen(serverfd, buf, strlen(buf));

     /* Send rest of request to server and forward response to client */
     send_requestResponse(&rioServer, serverfd, &rioClient, clientfd, &serverResponseSize);

     /* Close the connection with server */
     Close(serverfd);

     /* Log request */
     char logstring[MAXLINE];
     format_log_entry(logstring, sockaddr, uri, serverResponseSize);
     pthread_mutex_lock(&mutex); // lock before writing to common file
     fprintf(proxylog, "%s %d\n", logstring, serverResponseSize);
     pthread_mutex_unlock(&mutex); // unlock after writing to file
}
/* $end doit */

void send_requestResponse(rio_t *rio_server, int serverfd, rio_t *rio_client, int clientfd, int *size)
{
    *size = 0;
    char clientBuf[MAXLINE], serverBuf[MAXLINE];

    /*Read request from client*/
    long int clientN = 0;
    while((clientN = rio_readlineb(rio_client, clientBuf, MAXLINE)) > 0) {
		Rio_writen(serverfd, clientBuf, clientN);
		if (strcmp(clientBuf, "\r\n") == 0) {
			break;
		}
    }

    /*Read response from server and send to client*/
    long int serverN = 0;
    while((serverN = rio_readlineb(rio_server, serverBuf, (MAXLINE-1))) > 0) {
		Rio_writen(clientfd, serverBuf, serverN);
		*size += serverN;
    }

    return;
}

/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	   hostname[0] = '\0';
	   return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':') {
	   *port = atoi(hostend + 1);
    }

	/* Extract the path */
	pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	   pathname[0] = '\0';
    }
    else {
	   pathbegin++;
	   strcpy(pathname, pathbegin);
    }

    return 0;
}


/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /*
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}

