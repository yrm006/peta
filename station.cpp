extern "C"{
 #include <errno.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <stdint.h>
 #include <unistd.h>
#ifdef _WIN64
 #include <winsock2.h>
#else
 #include <netinet/in.h>
 #include <sys/socket.h>
 #include <arpa/inet.h>
#endif
 #include <sys/fcntl.h>
 #include <pthread.h>
 #include <string.h>
 #include "http.h"
}
#include <map>
#include <string>

#define BACKLOG         0

#ifdef _WIN64
 //none
#else
 #define INVALID_SOCKET  ((SOCKET)-1)
 typedef int SOCKET;
#endif



class channel{
protected:
    enum{ START=0, END=0x1000 };
    volatile uint8_t m_buf[END];
    volatile int     m_p = END;
    volatile int     m_nListener = 0;
    pthread_mutex_t  m_mtx;
public:
    channel(){
        pthread_mutex_init(&m_mtx, NULL);
    }
    ~channel(){
        pthread_mutex_destroy(&m_mtx);
    }
    bool isOnAir(){
        return (m_p != END);
    }
    void addListenerCount(){
        pthread_mutex_lock(&m_mtx);
        {
            ++m_nListener;
        }
        pthread_mutex_unlock(&m_mtx);
    }
    void subListenerCount(){
        pthread_mutex_lock(&m_mtx);
        {
            --m_nListener;
        }
        pthread_mutex_unlock(&m_mtx);
    }
    int getListenerCount(){
        return m_nListener;
    }
    void recv(SOCKET s, http* hdr){
                                                                    fprintf(stdout, "[%s]", __func__);fflush(stdout);
        // #ifdef _WIN64
        //     u_long mode = 1;
        //     ioctlsocket(s, FIONBIO, &mode);
        // #else
        //     fcntl(s, F_SETFL, fcntl(s, F_GETFL)|O_NONBLOCK);
        // #endif

        const char* res = 
            "HTTP/1.0 100 Continue\r\n"
            "\r\n"
        ;
        ::send(s, res, strlen(res), 0);
                                                                    // fprintf(stdout, "[Wrote]");fflush(stdout);

        m_p = START;
                                                                    // fprintf(stdout, "[Start]");fflush(stdout);

        while( ::recv(s, (char*)&m_buf[m_p], 1, 0) == 1 ){
                                                                    if(m_p==START) fprintf(stdout, ".");fflush(stdout);
            m_p = (m_p+1) % END;
        }
                                                                    // fprintf(stdout, "[End]");fflush(stdout);

        m_p = END;
    }
    void send(SOCKET s, http* hdr){
                                                                    fprintf(stdout, "[%s:%d]", __func__, getListenerCount());fflush(stdout);
        #ifdef SO_NOSIGPIPE
        int yes = 1;
        setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
        #endif

        const char* res = 
            "HTTP/1.0 200 OK\r\n"
            "content-type: audio/mpeg\r\n"
            "\r\n"
        ;
        ::send(s, res, strlen(res), 0);

        int p  = m_p;
        int pn = m_p;
        while(pn != END){
                                                                    // fprintf(stdout, "|");
            if(p < pn){
                                                                    // fprintf(stdout, "-");
                int n;
                n = ::send(s, (const char*)&m_buf[p], pn-p, 0);
                                                                    if(n<(pn-p)) break;
                p = pn;
            }else
            if(pn < p){
                                                                    // fprintf(stdout, "*");
                int n;
                n = ::send(s, (const char*)&m_buf[p], END-p, 0);
                                                                    if(n<(END-p)) break;
                p = 0;
                n = ::send(s, (const char*)&m_buf[p], pn-p, 0);
                                                                    if(n<(pn-p)) break;
                p = pn;
            }else
            {}

            usleep(1000*100);
            pn = m_p;
        }
    }
};



std::map<std::string, channel> g_oChannels;
pthread_mutex_t                g_oChannelsMutex;



void* on_accept(void* arg){
                                                                    // fprintf(stdout, "on_accept\n");
    SOCKET s = *(SOCKET*)arg;
    free(arg);

    struct http hdr;
    http_reset(&hdr);

    int n = 0;
    while( 
        (recv(s, hdr.m_aBuf+(n++), 1, 0) == 1)  &&
        !(
            hdr.m_aBuf[n-4]=='\r' && 
            hdr.m_aBuf[n-3]=='\n' && 
            hdr.m_aBuf[n-2]=='\r' && 
            hdr.m_aBuf[n-1]=='\n' &&
        1)                                      &&
        (n < HTTP_BUFSIZE)                      &&
    1){}
                                                                    // fwrite("\n", 1, 1, stdout);
                                                                    // fwrite(hdr.m_aBuf, n, 1, stdout);

    if(0 < http_parse(&hdr, n)){
        if(strcmp(hdr.m_pMethod, "PUT") == 0){
            channel* ch;
            pthread_mutex_lock(&g_oChannelsMutex);
            {
                ch = (g_oChannels.find(hdr.m_pURI) == g_oChannels.end()) ? &g_oChannels[hdr.m_pURI] : NULL;
            }
            pthread_mutex_unlock(&g_oChannelsMutex);

            if(ch){
                                                                    fprintf(stdout, "[Created:%s]", hdr.m_pURI);fflush(stdout);
                ch->recv(s, &hdr);

                pthread_mutex_lock(&g_oChannelsMutex);
                {
                    while(ch->getListenerCount()){
                        pthread_mutex_unlock(&g_oChannelsMutex);
                        usleep(1000*1000);
                        pthread_mutex_lock(&g_oChannelsMutex);
                    }
                    g_oChannels.erase(hdr.m_pURI);
                }
                pthread_mutex_unlock(&g_oChannelsMutex);
                                                                    fprintf(stdout, "[Deleted:%s]", hdr.m_pURI);fflush(stdout);
            }else{
                char aBuf[256];
                aBuf[sizeof(aBuf)-1] = '\0';                        // For safety.
                
                int n = 0;
                n += snprintf(aBuf+n, sizeof(aBuf)-n,
                    "HTTP/1.0 405 Method Not Allowed\r\n"
                    "\r\n"
                );
                
                send(s, aBuf, n, 0);
            }
        }else
        if(strcmp(hdr.m_pMethod, "GET") == 0){
            channel* ch = NULL;
            pthread_mutex_lock(&g_oChannelsMutex);
            {
                auto found = g_oChannels.find(hdr.m_pURI);
                if(found != g_oChannels.end() && found->second.isOnAir()){
                    ch = &found->second;
                    ch->addListenerCount();
                }
            }
            pthread_mutex_unlock(&g_oChannelsMutex);

            if(ch){
                ch->send(s, &hdr);
                ch->subListenerCount();
            }else{
                char aBuf[256];
                aBuf[sizeof(aBuf)-1] = '\0';                        // For safety.
                
                int n = 0;
                n += snprintf(aBuf+n, sizeof(aBuf)-n,
                    "HTTP/1.0 404 Not Found\r\n"
                    "\r\n"
                );
                
                send(s, aBuf, n, 0);
            }
        }else
        {
            char aBuf[256];
            aBuf[sizeof(aBuf)-1] = '\0';                            // For safety.
            
            int n = 0;
            n += snprintf(aBuf+n, sizeof(aBuf)-n,
                "HTTP/1.0 405 Method Not Allowed\r\n"
                "\r\n"
            );
            
            send(s, aBuf, n, 0);
        }
    }else{
                                                                    fprintf(stderr, "!http_parse\n");
		char aBuf[256];
		aBuf[sizeof(aBuf)-1] = '\0';                                // For safety.
		
		int n = 0;
		n += snprintf(aBuf+n, sizeof(aBuf)-n,
			"HTTP/1.0 400 Bad Request\r\n"
			"\r\n"
		);
		
		send(s, aBuf, n, 0);
    }
    
    close(s);
    return NULL;
}



int main(int argc, char *argv[]){
    int e;
    
    #ifdef _WIN64
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    pthread_mutex_init(&g_oChannelsMutex, NULL);

    struct sockaddr_in bindto = {};
    {
        bindto.sin_family      = PF_INET;
        bindto.sin_port        = htons(2000);
        bindto.sin_addr.s_addr = INADDR_ANY;
    }
    
    if(2 <= argc){
        bindto.sin_addr.s_addr = inet_addr(argv[1]);
    }
    if(3 <= argc){
        bindto.sin_port = htons( atoi(argv[2]) );
    }

    SOCKET s = socket(PF_INET, SOCK_STREAM, 0);
                                                                    if(s==INVALID_SOCKET) fprintf(stderr,"!socket:s\n"),exit(errno);
    e = bind(s, (struct sockaddr*)&bindto, sizeof(bindto));
                                                                    if(e!=0) fprintf(stderr,"!bind:%d\n",e),exit(errno);
    e = listen(s, BACKLOG);
                                                                    if(e==-1) fprintf(stderr,"!listen:%d\n",e),exit(errno);



    // show help
    {
        fprintf(stdout, "bindto: %s:%d.\n", inet_ntoa(bindto.sin_addr), (int)ntohs(bindto.sin_port));
        fprintf(stdout, "'Ctrl+C' to stop.\n");
        fflush(stdout);
    }

    while(1){
        SOCKET* a = (SOCKET*)malloc(sizeof(SOCKET));
        *a = accept(s, NULL, NULL);

        pthread_t t;
        e = pthread_create(&t, NULL, on_accept, a);
                                                                    if(e!=0) fprintf(stderr,"!pthread_create:%d\n",e),exit(errno);
    }



    close(s);

    pthread_mutex_destroy(&g_oChannelsMutex);

    #ifdef _WIN64
    WSACleanup();
    #endif

    return 0;
}


