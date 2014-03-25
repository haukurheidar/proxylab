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

FILE *logfile;

/*
 * Function prototypes
 */
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

struct sockaddr_in clientaddr;
struct sockaddr_in sockaddr;    
int listenfd, connfd, port, *connfdp, clientlen;
/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    
     void echo(int connfd);
     int port = atoi(argv[1]);
     int clientfd,sport,serverfd,siz;
     rio_t rio, rio2;
     size_t n;

     int listenSocket;   // File descriptor for listening server socket
     int activeSocket;   // File descriptoir for the active socket

     struct sockaddr_in serverAdd;   
     struct sockaddr_in clientAdd;

    /* Check arguments */
     char *host, buf[MAXLINE], method[16], uri[MAXLINE], path[MAXLINE], logstring; 
    if (argc != 2) {
	   fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	   exit(0);
    }

    host = argv[0];

    // Initialize serverAdd struct
     memset(&serverAdd, 0, sizeof(serverAdd));
     serverAdd.sin_family = AF_INET;
     serverAdd.sin_port = htons(port);
     serverAdd.sin_addr.s_addr = htonl(INADDR_ANY);

// Setup TCP listenSocket: (domain = AF_INET, type = SOCK_STREAM, protocol = 0)
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(listenSocket < 0){
        printf("Error creating socket\n");
        exit(1);
    }

    // listenfd = Open_listenfd(port);
    clientfd = open_listenfd(port);
    while (1) {
        // clientlen = sizeof(clientaddr);
        // connfd = Malloc(sizeof(int));
        // connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        connfd = Accept(clientfd, (SA *)&clientaddr, &clientlen);
        printf("sockaddr is %s\n", sockaddr);
        // doit(connfd);
        // Pthread_create(&tid, NULL, thread, connfdp);
    
        /*Read request from client*/
        rio_readinitb(&rio, connfd);
        rio_readlineb(&rio, buf, MAXLINE);

        /*Get the method (GET) and the uri (http://bla.com/)*/
        sscanf(buf, "%s %s", method, uri);

        /*Parse the uri to get the host, pathname and port*/
        parse_uri(uri, host, path, &sport);

        /*Open connection to end server*/
        serverfd = Open_clientfd(host, sport);

        /*Read first line in server response*/
        Rio_readinitb(&rio2, serverfd);
        Rio_writen(serverfd, buf, strlen(buf));

        /*Read request from client*/
            while(Rio_readlineb(&rio, buf, MAXLINE) != 0) {
                Rio_writen(serverfd, buf, strlen(buf));
                if (strcmp(buf, "\r\n") == 0) {
                break;
                }
            }

        /*Read server response and send it to the client, keep track of the size of the response*/
            while((n = Rio_readlineb(&rio2, buf, MAXLINE)) != 0) {
                Rio_writen(connfd, buf, n);
                siz = sizeof(n)+siz;

                }
    /*Log to proxy.log*/
            format_log_entry(&logstring, (struct sockaddr_in *)&sockaddr, uri, siz);
            Close(serverfd);
            Close(connfd);
    }
    Close(connfd);
    exit(0);
}

void doit(int fd)
{
	int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
/*
    //rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);                   //line:netp:doit:readrequest
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
       clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
       return;
    }*/
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
    if (*hostend == ':')   
	   *port = atoi(hostend + 1);
        printf("port is %d\n",port);    
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
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    FILE *fp;
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
    fp=fopen("proxy.log", "a");

    fprintf(fp, "%s: %d.%d.%d.%d %s - %d \n", time_str, a, b, c, d, uri, size);
    fclose(fp);


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}

