#include "threadpool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define ERROR400 "HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\nContent-Length: 113\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H4>400 Bad request</H4>\nBad Request.\n</BODY></HTML>"
#define ERROR403 "HTTP/1.0 403 Forbidden\r\nContent-Type: text/html\r\nContent-Length: 111\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H4>403 Forbidden</H4>\nAccess denied.\n</BODY></HTML>"
#define ERROR404 "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 112\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H4>404 Not Found</H4>\nFile not found.\n</BODY></HTML>"
#define ERROR500 "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H4>500 Internal Server Error</H4>\nSome server side error.\n</BODY></HTML>"
#define ERROR501 "HTTP/1.0 501 Not supported\r\nContent-Type: text/html\r\nContent-Length: 129\r\nConnection: close\r\n\r\n<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n<BODY><H4>501 Not supported</H4>\nMethod is not supported.\n</BODY></HTML>"

/*
This is a partial HTTP Server program, the program only supports a small part of the whole
The concept of an HTTP server.
This server supports GET requests.
The server knows to agree to the client request and return the appropriate response.

*/



int size;
char** filterarr;

typedef  struct{
    int port;
    int poolSize;
    int maxNumOfReq;
}infoServer;

void errorNumber(int errorNumber, int fd){//send error request to the server and close the fd
    if(errorNumber == 400)
        write(fd, ERROR400, strlen(ERROR400));
    if(errorNumber == 403)
        write(fd, ERROR403, strlen(ERROR403));
    if(errorNumber == 404)
        write(fd, ERROR404, strlen(ERROR404));
    if(errorNumber == 500)
        write(fd, ERROR500, strlen(ERROR500));
    if(errorNumber == 501)
        write(fd,ERROR501,strlen(ERROR501));
    close(fd);
}

int isIpInSubnet(char* ip, char* fullip) {
    char* subnets=strstr(fullip, "/");
    if (subnets == NULL) {
         return 0;
    }

    int subnet=atoi(subnets+1);
    unsigned int mask=0xFFFFFFFF<<(32-subnet);
    char ipWithoutSlash[100];
    memset(ipWithoutSlash,0,100);
    
    for (size_t i =0; i < strlen(fullip);i++) {
        if (fullip[i] == '/') {
            ipWithoutSlash[i] = '\0';
            break;
        }
        ipWithoutSlash[i] = fullip[i];
    }

    struct sockaddr_in one,two;
    inet_aton(ip, &one.sin_addr);
    inet_aton(ipWithoutSlash, &two.sin_addr);
    return (ntohl(one.sin_addr.s_addr)&mask)==(ntohl(two.sin_addr.s_addr)&mask);
}

char *get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

int creatSocet(infoServer *str ){//Creating Socket and Bind
    int fd;		/* socket descriptor */
    struct sockaddr_in address;
    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {//creat socket
        perror("error: socket failed \n");
        exit(EXIT_FAILURE);
    }


    address.sin_port = htons(str->port);
    address.sin_family = AF_INET; /* use the Internet addr family */
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(fd, (struct sockaddr*) &address, sizeof(address)) < 0) {//bind
        perror("error:bind failed\n");
        exit(EXIT_FAILURE);
    }
    if(listen(fd, str->maxNumOfReq) < 0){//listen
        perror("error: listen failed\n");
        exit(EXIT_FAILURE);
    }

    return fd;
}



void createDirs(char* str) {//When the file does not exist create a folder
    char *lastSlash=strrchr(str,'/');//Find the last "/" and returns a pointer to its position
    char untilLastSlash[100];
    memset(untilLastSlash, 0, 100);//Resets the lastSlash array
    strncpy(untilLastSlash, str, lastSlash-str);
    char last[100];
    memset(last, 0, 100);
    char *token= strtok(untilLastSlash,"/");
    while (token !=NULL){
        strcat(last, token);
        strcat(last, "/");
        mkdir(last, 0777);
        token = strtok(NULL, "/");
    }
}

int creatMainSocket(int fd1,char* host){
    struct hostent *hp; /*ptr to host info for remote*/
    struct sockaddr_in peeraddr;
    peeraddr.sin_family = AF_INET;
    hp = gethostbyname(host);
    if(hp==NULL){//cheak if gethostbynameis Succeeded
        herror("gethostbyname\n");
        errorNumber(404,fd1);
        exit(EXIT_FAILURE);
    }
    peeraddr.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr))->s_addr;

    for (int i = 0; i < size; i++) {//Checks whether the address is in a filter file
        if (!strcmp(filterarr[i], host)) {
            errorNumber(403, fd1);
            return -1;
        }
        if (filterarr[i][0] >= '0' && filterarr[i][0] <= '9') {
            if (isIpInSubnet(inet_ntoa(peeraddr.sin_addr), filterarr[i])) {
                errorNumber(403, fd1);
                return -1;
            }
        }

    }

    int fd;		/* socket descriptor */
    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("error: socket\n");
        exit(EXIT_FAILURE);
    }

    peeraddr.sin_port = htons(80);

    if(connect(fd, (struct sockaddr*) &peeraddr, sizeof(peeraddr)) < 0) {//connection to socket
        perror("error: connection failed\n");
        exit(EXIT_FAILURE);
    }

    return fd;
}

char** filter (char* path) {
    char **filter_arr;
    size_t len = 0;
    char *line = NULL;
    FILE *fd = fopen(path, "r");
    if (fd == NULL) {
        perror("error: open file\n");
        exit(EXIT_FAILURE);
    }

    filter_arr = (char **) malloc(sizeof(char *));
    if (filter_arr == NULL) {
        perror("Error malloc\n");
        exit(EXIT_FAILURE);
    }

    while (getline(&line, &len, fd) != -1) {
            filter_arr=(char**) realloc(filter_arr, (size+1)*sizeof(char*));
            if(filter_arr==NULL){
                perror("error: realloction\n");
                exit(EXIT_FAILURE);
            }

            filter_arr[size]=(char *) malloc(sizeof(char )* (strlen(line)+1));
            if( filter_arr[size]==NULL){
                perror("error: allocation\n");
                exit(EXIT_FAILURE);
            }
            memset(filter_arr[size], 0, strlen(line)+1);
            line[strcspn(line, "\r\n")] = 0;
            strncpy(filter_arr[size], line, strlen(line));

            size++;
    }

    if (line!=NULL) {
        free(line);
    }

    fclose(fd);
    return filter_arr;
}

int handel_client(void* newSocket){
    int fd=*((int*)newSocket);
    char buf[8000];
    memset(buf,0,8000);
    int n = read(fd, buf, 8000);
    if (n < 0) {
        errorNumber(500, fd);
        return 0;
    }

    if (!strstr(buf, "GET")) {
        errorNumber(501, fd);
        return 0;
    }

    char host[200];
    memset(host,0,200);
    char* hs = strstr(buf, "Host: ");
    if (hs == NULL) {
        errorNumber(400, fd);
    }
    char* he = strstr(hs, "\r\n");
    int i ;
    for (i = 0; i < he-hs; i++) {
        host[i]=*(hs+6+i);
    }
    host[i-6] = '\0';

    char path[200];
    memset(path,0,200);
    char* ps = strstr(buf, "/");
    char* pe = strstr(buf, "HTTP/1");
    for (i = 0; i < pe-ps; i++) {
        path[i]=*(ps+i);
    }
    path[i-1]='\0';

    char diskpath[400];
    memset(diskpath,0,400);
    strcpy(diskpath,host);
    strcat(diskpath,path);
    if (diskpath[strlen(diskpath)-1]=='/') {
        strcat(diskpath, "index.html");
    }

    FILE* f = fopen(diskpath, "r");
    if (f != NULL) {
        fseek(f, 0L, SEEK_END);
        int file_size =(int)ftell(f);
        rewind(f);

        char* output = (char*) malloc(file_size+600);
        if (output == NULL) {
            errorNumber(500, fd);
            return 0;
        }
        memset(output,0,file_size+600);

        printf("File is given from local filesystem\n");
        char* mime = get_mime_type(diskpath);
        if (mime != NULL) {
            sprintf(output, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\nConnection: close\r\n\r\n", file_size, mime);
        } else {
            sprintf(output, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", file_size);
        }

        char* filecontent = (char*)malloc(file_size+1);
        int readAmount = 0;
        int header_len = strlen(output);
        memset(filecontent,0,file_size+1);
        readAmount = fread(filecontent, 1, file_size, f);
        memcpy(output+header_len, filecontent, readAmount);
        if (write(fd, output, header_len+readAmount) < 0) {
            errorNumber(500, fd);
            return 0;
        }

        printf("\n Total response bytes: %d\n",header_len+readAmount);
        free(output);
        free(filecontent);
        fclose(f);
    } else {
        int sd=creatMainSocket(fd,host);
        if (sd == -1) {
            return 0;
        }

        char* http1 = strstr(buf, "HTTP/1");
        if (http1 != NULL) {
            char* http11 = strstr(http1, "1.1");
            if (http11 != NULL) {
                http11[2] = '0'; 
            }
        }
        printf("File is given from origin server\n");
        printf("HTTP request =\n%s\nLEN = %d\n", buf, (int)strlen(buf));

        if (write(sd, buf, sizeof(buf)) < 0) {
            errorNumber(501, fd);
            return 0;
        }

        unsigned char buff[1024];
        int flag=0;
        int startWrite = 0;
        FILE *fd2= NULL;
        int howMuchRead = 0;
        int readAmount = 0;
        memset(buff,0,1024);

        while ((readAmount = read(sd,buff,1024))>0){//Read to the end of the file
            howMuchRead += readAmount;
            if (write(fd, buff, readAmount) <0) {
                errorNumber(501, fd);
                return 0;
            }
            if (flag == 0 && strstr((char*) buff, "200 OK")) {
                flag = 1;
                createDirs(diskpath);
                fd2 = fopen(diskpath, "w");
                if (fd2 == NULL) {
                    errorNumber(501, fd);
                    return 0;
                }
            }
            char* afterRnRn = strstr((char*) buff, "\r\n\r\n");//After the string / r / n/ r /n will start writing to the file
            if (fd2 != NULL && startWrite == 0 && afterRnRn != NULL) {
                startWrite = 1;
                int index = ((unsigned char*) afterRnRn) - buff + 4;
                fwrite(buff + index, 1, readAmount-index, fd2);
            } else if (startWrite == 1 && fd2 != NULL) {
                fwrite(buff, 1, readAmount, fd2);
            }
            memset(buff,0,1024);
        }

        printf("\n Total response bytes: %d\n",howMuchRead);

        if (fd2 != NULL) {
            fclose(fd2);
        }
    }
    close(fd);
    return 0;
}


int main(int argc,char* argv[]){
    if(argc!=5){
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        exit(EXIT_FAILURE);
    }

    infoServer* str=(infoServer*) malloc(sizeof (infoServer));
    if(str==NULL){
        perror("error: allocation failed\n");
        exit(EXIT_FAILURE);
    }


    str->port=atoi(argv[1]);
    str->poolSize=atoi(argv[2]);
    str->maxNumOfReq= atoi(argv[3]);
    filterarr = filter(argv[4]);


    threadpool *thread_pool = create_threadpool(str->poolSize);
    if(thread_pool==NULL)
    {
        perror("error: allocation failed\n");
        exit(EXIT_FAILURE);
    }
    if(str->port<0 || str->port>65535 || str->poolSize<0 || str->maxNumOfReq<0 ){
        printf("Usage: proxyServer <port> <pool size> <max number of request> <filter>\n");
        free(str);
        exit(1);
    }
    int fd = creatSocet(str);

    int *fds=(int*)malloc(sizeof(int)*(str->maxNumOfReq+1));
    if(fds==NULL){
        perror("error:malloc failed\n");
        destroy_threadpool(thread_pool);
        exit(EXIT_FAILURE);
    }
    for(int i=0;i<str->maxNumOfReq;i++){
        fds[i]=accept(fd,NULL,NULL);
        if(fds[i]<0){
            perror("error: accept failed\n");
            exit(EXIT_FAILURE);
        }
        dispatch(thread_pool,handel_client,(void *)&fds[i]);
    }
    destroy_threadpool(thread_pool);
    free(fds);
    for (int i = 0; i < size; i++) {
        free(filterarr[i]);
    }
    free(filterarr);
    free(str);
    return 0;

}

