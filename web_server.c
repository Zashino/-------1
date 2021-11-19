#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#define REQUEST_SIZE 32384
#define BSIZE 8192

const char *get_content_type(const char* path) {
    const char *last_dot = strrchr(path, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".csv") == 0) return "text/csv";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }

    return "application/octet-stream";
}

void server_work(int newfd)
{
	static char request[REQUEST_SIZE];

	int r = read(newfd, request, REQUEST_SIZE);
	if (r < 1) {
        	printf("Unexpected disconnect from child process %d\n", newfd);
        	exit(1);
        }
	else {
		printf("%s",request);

               	if(strncmp("GET / ", request, 6) == 0) {
                        strcpy(request,"GET /index.html\0");
		}

		if(strncmp("GET /", request, 5) == 0) {
			char *path = request + 5;
			char *q = path;
			while(*q!=' '){
				q++;
			}
			*q = '\0';
		
			int file_fd = open(path,O_RDONLY);
			const char *ct = get_content_type(path);

			char buffer[BSIZE];

    			sprintf(buffer, "HTTP/1.1 200 OK\r\n");
			write(newfd, buffer, strlen(buffer));

			sprintf(buffer, "Content-Type: %s\r\n", ct);
    			write(newfd, buffer, strlen(buffer));
			sprintf(buffer, "\r\n");
			write(newfd, buffer, strlen(buffer));

			int ret;
			while ((ret = read(file_fd, buffer, BSIZE))>0) {
				write(newfd, buffer, ret);
			}
		}
		else if(strncmp("POST /", request, 6) == 0) {
			char *tmp = strstr(request, "filename");
			char filename[BSIZE];
			memset (filename, '\0', BSIZE);
			char *a,*b;
    			a = strstr(tmp,"\"");
    			b = strstr(a+1,"\"");
    			strncpy (filename,a+1,b-a-1);
		
			char dir[BSIZE];
			memset (dir, '\0', BSIZE);
			strcat (dir, "upload/");
    			strcat (dir, filename);
			
			int fd = open(dir, O_CREAT|O_WRONLY|O_TRUNC|O_SYNC,S_IRWXO|S_IRWXU|S_IRWXG);

			char buffer[BSIZE];
			
			a = strstr(tmp,"\n");
    			b = strstr(a+1,"\n");
    			a = strstr(b+1,"\n");
    			b = strstr(b, "---------------------------");

			int last_write, last_ret;
    			if (b != 0)
    				write(fd, a+1, b-a-3);
    			else {
    				int start = (int )(a - &request[0])+1;
    				last_write = write(fd, a+1, r -start -61);
    				last_ret = r;
				
    				memcpy (buffer, a+1+last_write, 61);
    				while ((r=read(newfd, request, BSIZE))>0) {
        				write(fd, buffer, 61);
        				last_write = write(fd, request, r - 61);
        				memcpy (buffer, request+last_write, 61);
        				last_ret = r;
        				if (r!=8096)
            					break;
    				}
    			}
			close(fd);

			int file_fd = open("index.html", O_RDONLY);
			sprintf(buffer, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
			write(newfd, buffer, strlen(buffer));

			int ret;
                        while ((ret = read(file_fd, buffer, BSIZE))>0) {
                                write(newfd, buffer, ret);
                        }
		}
	}

}

void sig_child(int signo)
{
	pid_t pid;
	int stat;

	pid = wait(&stat);
	printf("Child %d terminated\n", pid);
	return;
}

int main()
{
	int status; //getaddrinfo() status
	int sockfd, newfd;
	pid_t childpid;
	socklen_t addr_size;
	struct addrinfo hints, *res;
	struct sockaddr_storage their_addr;
	time_t ticks;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // 不用管是 IPv4 或 IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE; // 幫我填好我的 IP

	if ((getaddrinfo(NULL, "3495", &hints, &res)) != 0) {
  		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
  		exit(1);
	}

	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	if(bind(sockfd, res->ai_addr, res->ai_addrlen)==-1){
		fprintf(stderr, "bind error\n");
		exit(1);
	}
	freeaddrinfo(res);
	listen(sockfd, 20);

	signal(SIGCHLD, sig_child);
	for( ; ; ){
		addr_size = sizeof(their_addr);
		newfd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

		if((childpid = fork()) == 0) {
			close(sockfd);

			server_work(newfd);

			close(newfd);
			exit(0);
		}
		close(newfd);
	}
}
