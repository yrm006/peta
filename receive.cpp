extern "C"{
 #include <errno.h>
 #include <unistd.h>
 #include <netinet/in.h>
 #include <sys/socket.h>
 #include <SDL2/SDL.h>
 #include "http.h"
}

typedef int SOCKET;



SOCKET net_sock;

int net_init(){
    int e;

    struct sockaddr_in cnctto = {};
    {
        cnctto.sin_family      = PF_INET;
        cnctto.sin_port        = htons(2000);
        cnctto.sin_addr.s_addr = 0x0100007f;
    }

    net_sock = socket(PF_INET, SOCK_STREAM, 0);
    e = connect(net_sock, (struct sockaddr*)&cnctto, sizeof(cnctto));
                                                                    if(e==-1) fprintf(stderr,"!connect:%d\n",e),exit(errno);

    const char* req = 
        "GET / HTTP/1.0\r\n"
        "\r\n"
    ;
    write(net_sock, (const void*)req, strlen(req));

    //response
    struct http hdr;
    http_reset(&hdr);

    int n = 0;
    while( 
        (recv(net_sock, hdr.m_aBuf+(n++), 1, 0) == 1)  &&
        !(
            hdr.m_aBuf[n-4]=='\r' && 
            hdr.m_aBuf[n-3]=='\n' && 
            hdr.m_aBuf[n-2]=='\r' && 
            hdr.m_aBuf[n-1]=='\n' &&
        1)                                      &&
        (n < HTTP_BUFSIZE)                      &&
    1){}

    if(0 < http_parse(&hdr, n)){
        if(strcmp(hdr.m_pCode, "200") != 0){
                                                                    fprintf(stderr, "!http code:%s\n", hdr.m_pCode);
                                                                    return 0;
        }
    }else{
                                                                    fprintf(stderr, "!http_parse\n");
                                                                    return 0;
    }

    return 1;
}

void net_quit(){
    close(net_sock);
}



int main(int argc, char *argv[]){
    net_init();
while(1){
    char buf[1];
    read(net_sock, buf, 1);
    fwrite(buf, 1, 1, stdout);
}

    char cmd[0x100];
    while( fgets(cmd, sizeof(cmd), stdin) ){
        cmd[strlen(cmd)-1] = '\0';
        if( strcmp(cmd, "") == 0 ){
            break;
        }
    }

    net_quit();

    return 0;
}


