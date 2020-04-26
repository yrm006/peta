extern "C"{
 #include <errno.h>
 #include <unistd.h>
#ifdef _WIN64
 #include <winsock2.h>
#else
 #include <netinet/in.h>
 #include <sys/socket.h>
 #include <arpa/inet.h>
#endif
 #include <pthread.h>
 #include <SDL2/SDL.h>
 #include <lame/lame.h>
 #include "http.h"
}

#ifdef _WIN64
 //none
#else
 typedef int SOCKET;
#endif



SOCKET net_sock;

int net_init(sockaddr_in* pto, const char* uri){
                                                                    // fprintf(stdout, "[%s]", __func__);fflush(stdout);
    int e;

    net_sock = socket(PF_INET, SOCK_STREAM, 0);
    e = connect(net_sock, (sockaddr*)pto, sizeof(*pto));
                                                                    if(e==-1){ fprintf(stderr,"!connect:%d\n",e); return 0; }

    // request
    {
        char aBuf[256];
        aBuf[sizeof(aBuf)-1] = '\0';                        // For safety.
        
        int n = 0;
        n += snprintf(aBuf+n, sizeof(aBuf)-n,
            "GET %s HTTP/1.0\r\n"
            "\r\n",
            uri
        );
        
        send(net_sock, aBuf, n, 0);
    }

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



hip_global_flags* mp3_lib;
volatile short mp3_decoded[576*8];
volatile int   mp3_pd = 0;
volatile int   mp3_end = 0;
pthread_t      mp3_t;

void* mp3_decode(void* arg){
// start:
//     while(1){
//         short c;
//         while( recv(net_sock, (char*)&c, 2, MSG_PEEK) != 2 ){
//             usleep(1000*100);
//         }

//         if( (c&0xfff0) != 0xffe0 ){
//             char a;
//             recv(net_sock, &a, 1, 0);   // garbage
//             break;
//         }
//     }



    while(!mp3_end){
        uint8_t buf[72/4];
        uint32_t m = 0;
        
        while(m < sizeof(buf)){
            int r = recv(net_sock, (char*)buf+m, sizeof(buf)-m, 0);
            if(!r) break;
            m += r;
        }

        short lbuf[576*2];
        mp3data_struct d;
        int n = hip_decode_headers(mp3_lib, buf, m, lbuf, NULL, &d);
                                                                        // fprintf(stderr, "###m:%d, n:%d\n", m, n);fflush(stderr);
        if(0 < n){
                                                                        fprintf(stdout, "+");fflush(stdout);
                                                                        // fprintf(stderr, "st:%d, sr:%d, br:%d, mo:%d, fs:%d\n", 
                                                                        //     d.stereo, d.samplerate, d.bitrate, d.mode, d.framesize);fflush(stderr);
            int i = 0;
            while(i < n){
                mp3_decoded[mp3_pd++] = lbuf[i++];
                mp3_pd %= sizeof(mp3_decoded)/sizeof(mp3_decoded[0]);
            }
        }
        if(n < 0){
            hip_decode_exit(mp3_lib);
            mp3_lib = hip_decode_init();
            // goto start;
        }else
        {}
    }

    return NULL;
}

int mp3_init(){
                                                                    // fprintf(stdout, "[%s]", __func__);fflush(stdout);
    mp3_lib = hip_decode_init();

    pthread_create(&mp3_t, NULL, mp3_decode, NULL);

    return 1;
}

void mp3_quit(){
    mp3_end = 1;
    pthread_join(mp3_t, NULL);

    hip_decode_exit(mp3_lib);
}



SDL_AudioDeviceID snd_dev;
int snd_pd = 0;

void snd_cb(void *userdata, Uint8 *stream, int len) {
                                                                    // fprintf(stderr, "[%s, len:%d]", __func__, len);fflush(stderr);
    if(snd_pd < mp3_pd){                                            // [--s--------m--]
        if(len/sizeof(short) < mp3_pd-snd_pd){                      // [--|<-l->|-----]
            memcpy(stream, (short*)mp3_decoded+snd_pd, len);
            snd_pd += len/sizeof(short);
        }else                                                       // [--s--m--------]
        {}
    }else
    if(mp3_pd < snd_pd){                                            // [-m---s--------]
        int h = sizeof(mp3_decoded)/sizeof(short) - snd_pd;         // [-----|<--h-->|]
        if(len/sizeof(short) < h){                                  // [-----|<-l->|--]
            memcpy(stream, (short*)mp3_decoded+snd_pd, len);
            snd_pd += len/sizeof(short);
        }else{                                                      // [>|---|<---l---]
            if(len/sizeof(short) < (h + mp3_pd)){                   // [---m----------]
                memcpy(stream, (short*)mp3_decoded+snd_pd, sizeof(short)*h);
                memcpy(stream+sizeof(short)*h, (short*)mp3_decoded, len-sizeof(short)*h);
                snd_pd = len/sizeof(short)-h;
            }else                                                   // [m-------------]
            {}
        }
    }else                                                           // [--*-----------] (* == s&m)
    {}
}

int snd_init() {
                                                                    // fprintf(stdout, "[%s]", __func__);fflush(stdout);
    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec want;

    SDL_zero(want);
    want.freq = 8000;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 576/2;//2205/8;
    want.callback = snd_cb;

    snd_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if(!snd_dev){
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        return 0;
    }

    while(!mp3_pd) usleep(1000*100);

    SDL_PauseAudioDevice(snd_dev, 0);

    return 1;
}

void snd_quit(){
    SDL_CloseAudioDevice(snd_dev);
}



int main(int argc, char *argv[]){
    #ifdef _WIN64
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    struct sockaddr_in cnctto = {};
    {
        cnctto.sin_family      = PF_INET;
        cnctto.sin_port        = htons(2000);
        cnctto.sin_addr.s_addr = (127<<0) | (0<<8) | (0<<16) | (1<<24);
    }

    char uri[0x100] = "/";

    if(2 <= argc){
        cnctto.sin_addr.s_addr = inet_addr(argv[1]);
    }
    if(3 <= argc){
        cnctto.sin_port = htons( atoi(argv[2]) );
    }
    if(4 <= argc){
        strncpy(uri, argv[3], sizeof(uri))[sizeof(uri)-1] = '\0';
    }

    if(
        !net_init(&cnctto, uri) ||
        !mp3_init() ||
        !snd_init() ||
    0){
                                                                    fprintf(stdout, "!error\n");fflush(stdout);
        return 1;
    }

    // show help
    {
        fprintf(stdout, "connectto: %s:%d %s.\n", inet_ntoa(cnctto.sin_addr), (int)ntohs(cnctto.sin_port), uri);
        fprintf(stdout, "'Enter' to stop.\n");
        fflush(stdout);
    }

    char cmd[0x100];
    while( fgets(cmd, sizeof(cmd), stdin) ){
        cmd[strlen(cmd)-1] = '\0';
        if( strcmp(cmd, "") == 0 ){
            break;
        }
    }

    snd_quit();
    mp3_quit();
    net_quit();

    return 0;
}


