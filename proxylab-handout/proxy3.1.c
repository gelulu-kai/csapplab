// #include <stdio.h>

// /* Recommended max cache and object sizes */
// #define MAX_CACHE_SIZE 1049000
// #define MAX_OBJECT_SIZE 102400

// /* You won't lose style points for including this long line in your code */
// static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

// int main()
// {
//     printf("%s", user_agent_hdr);
//     return 0;
// }

#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key= "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

void doit(int connfd);
void parse_uri(char *uri,char *hostname,char *path,int *port);
void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio);
int connect_endServer(char *hostname,int port,char *http_header);
void *thread(void *vargp);
int reader(int fd, char* uri);
void writer(char* uri, char* buf);
void init_cache();



typedef struct{
    char *name;
    char *object;
}CacheLine;
typedef struct{
    int uesd_cnt;
    CacheLine *objects;
}Cache;
/**
 * reader-writer model
 */
Cache cache;
int readcnt;
sem_t mutex, w;

int main(int argc,char **argv)
{
    int listenfd, *connfdp;
    socklen_t  clientlen;
    char hostname[MAXLINE],port[MAXLINE];
    pthread_t tid;

    struct sockaddr_storage clientaddr;/*generic sockaddr struct which is 28 Bytes.The same use as sockaddr*/

    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    init_cache();

    while(1){
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd,(SA *)&clientaddr,&clientlen);

        /*print accepted message*/
        Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
        printf("Accepted connection from (%s %s).\n",hostname,port);

        /*sequential handle the client transaction*/
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    return 0;
}

void init_cache()
{
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);
    readcnt = 0;
    cache.uesd_cnt = 0;
    cache.objects = (CacheLine *)Malloc(sizeof(CacheLine) * 10);
    for (int i=0; i<10; ++i)
    {
        cache.objects[i].name = Malloc(sizeof(char) * MAXLINE);
        cache.objects[i].object = Malloc(sizeof(char) * MAX_OBJECT_SIZE);
    }
}

void *thread(void *vargp)
{
    Pthread_detach(pthread_self());//free thread
    int connfd = *((int *)vargp);
    Free(vargp);
    doit(connfd);
    Close(connfd);
}

/*handle the client HTTP transaction*/
void doit(int connfd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], object_buf[MAX_OBJECT_SIZE];
    rio_t rio, end_server_rio;
    char hostname[MAXLINE], path[MAXLINE], endserver_http_header[MAXLINE];
    int port, endserver_fd;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s%s%s", method, uri, version);
    if(strcasecmp(method, "GET")){
        printf("Proxy does not implement the method");
        return;
    }


    parse_uri(uri, hostname, path, &port);

    if (reader(connfd, uri)){
        fprintf(stdout, "%s from cache\n", uri);
        fflush(stdout);//clear buf
        return;
    }

    build_http_header(endserver_http_header, hostname, path, port, &rio);

    char portstr[100]; 
    sprintf(portstr, "%d", port);
    endserver_fd = Open_clientfd(hostname, &portstr);//as client
    if(endserver_fd<0){
        printf("Connection faild\n");
        return;
    }
    
    Rio_readinitb(&end_server_rio, endserver_fd);
    Rio_writen(endserver_fd, endserver_http_header, strlen(endserver_http_header));//request_line_header

    /*receive message from end server and send to the client*/
    size_t n;
    int total_size = 0;
    while((n = Rio_readlineb(&end_server_rio, buf, MAXLINE)) != 0){
        printf("proxy received %d bytes,then send\n",n);
        Rio_writen(connfd, buf, n);//to client
        strcpy(object_buf+total_size, buf);
        total_size += n;
    }
    if (total_size < MAX_OBJECT_SIZE){
        writer(uri, object_buf);
    }
    Close(endserver_fd);

}

int reader(int fd, char* uri)
{
    int in_cache = 0;//not in cache return 0 else...

    P(&mutex);
    readcnt++;
    if(readcnt == 1) P(&w);
    V(&mutex);

    for(int i = 0;i < 10; i++){
        if (!strcmp(uri, cache.objects[i].name)){
            Rio_writen(fd, cache.objects[i].name, MAX_OBJECT_SIZE);
            in_cache = 1;
            break;
        }
    }

    P(&mutex);
    readcnt--;
    if(readcnt == 0) V(&w);
    V(&mutex);    
}

void writer(char* uri, char *buf)
{
    P(&w);
    strcpy(cache.objects->name, uri);
    strcpy(cache.objects->object, buf);
    cache.uesd_cnt++;
    V(&w);
}

void build_http_header(char *http_header,char *hostname,char *path,int port,rio_t *client_rio)
{
    char buf[MAXLINE],request_hdr[MAXLINE],other_hdr[MAXLINE],host_hdr[MAXLINE];
    /*request line*/
    sprintf(request_hdr,"GET %s HTTP/1.0\r\n",path);//methlod, uri, version
    /*get other request header for client rio and change it */
    while(Rio_readlineb(client_rio,buf,MAXLINE)>0)
    {
        if(strcmp(buf,"\r\n")==0) break;/*EOF*/

        if(!strncasecmp(buf,"Host",strlen("Host")))/*Host:*/
        {
            strcpy(host_hdr,buf);
            continue;
        }

        if(!strncasecmp(buf,"Connection",strlen("Connection"))
                &&!strncasecmp(buf,"Proxy-Connection",strlen("Proxy-Connection"))
                &&!strncasecmp(buf,"User-Agent",strlen("User-Agent")))
        {
            strcat(other_hdr,buf);
        }
    }
    if(strlen(host_hdr)==0)
    {
        sprintf(host_hdr,"Host: %s\r\n",hostname);
    }
    sprintf(http_header,"%s%s%s%s%s%s%s",
            request_hdr,
            host_hdr,
            conn_hdr,
            prox_hdr,
            user_agent_hdr,
            other_hdr,
            endof_hdr);

    return ;
}

/*parse the uri to get hostname,file path ,port*/
void parse_uri(char *uri,char *hostname,char *path,int *port)
{
    *port = 80;
    char* pos = strstr(uri,"//");//return字符串str中第一次出现子串substr的地址；如果没有检索到子串，则返回NULL

    pos = pos!=NULL? pos+2:uri;

    char* pos2 = strstr(pos,":");//http://localhost:15213/home.html
    if(pos2!=NULL)
    {
        *pos2 = '\0';//':'->'\0'!!!!!!!!!!
        sscanf(pos,"%s",hostname);
        sscanf(pos2+1,"%d%s",port,path);
        // printf(hostname, *port, path);
    }
    else
    {
        pos2 = strstr(pos,"/");//http://localhost/home.html
        if(pos2!=NULL)
        {
            *pos2 = '\0';
            sscanf(pos,"%s",hostname);
            *pos2 = '/';
            sscanf(pos2,"%s",path);
        }
        else
        {
            sscanf(pos,"%s",hostname);
        }
    }
    return;
}