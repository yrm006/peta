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

    {
        char aBuf[256];
        aBuf[sizeof(aBuf)-1] = '\0';                        // For safety.
        
        int n = 0;
        n += snprintf(aBuf+n, sizeof(aBuf)-n,
            "PUT %s HTTP/1.0\r\n"
            "content-type: audio/mpeg\r\n"
            "\r\n",
            uri
        );
        
        send(net_sock, aBuf, n, 0);
    }

    const char* req = 
        "PUT / HTTP/1.0\r\n"
        "content-type: audio/mpeg\r\n"
        "\r\n"
    ;
    send(net_sock, req, strlen(req), 0);

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
        if(strcmp(hdr.m_pCode, "100") != 0){
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



lame_global_flags* mp3_lib;

int mp3_init(){
                                                                    // fprintf(stdout, "[%s]", __func__);fflush(stdout);
    mp3_lib = lame_init();
    lame_set_num_channels(mp3_lib, 1);
    lame_set_mode(mp3_lib, MONO);
    lame_set_out_samplerate(mp3_lib, 8000);
    if( lame_init_params(mp3_lib) < 0 ){
        fprintf(stderr, "!lame_init_params\n");
        return 0;
    }

    return 1;
}

void mp3_quit(){
    lame_close(mp3_lib);
}



SDL_AudioDeviceID mic_dev;

void mic_cb(void *userdata, Uint8 *stream, int len) {
                                                                    // fprintf(stdout, "[%s]", __func__);fflush(stdout);
    unsigned char buf[2048*2 + 7200];
    int n = lame_encode_buffer(mp3_lib, (const short*)stream, NULL, len/2, buf, sizeof(buf));

    // fprintf(stderr, "[mic_cb] len:%d, n:%d\n", len, n);
    // fwrite(stream, len, 1, stdout);
    if(0 < n){
        // fwrite(buf, n, 1, stdout);
                                                                    fprintf(stdout, "*");fflush(stdout);
        send(net_sock, (const char*)buf, n, 0);
    }
}

int mic_init() {
                                                                    // fprintf(stdout, "[%s]", __func__);fflush(stdout);
    #ifdef _WIN64
    // putenv("SDL_AUDIODRIVER=DirectSound");
    // CoInitialize(NULL);
    #endif

    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec want, have;

    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = mic_cb;

    mic_dev = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(0, 1), 1, &want, &have, 0);
    if(!mic_dev){
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        #ifdef _WIN64
        fprintf(stderr, "Check 'Setting'->'Privacy'->'Microphone'.\n");
        #endif
        return 0;
    }else
    if (have.format != want.format) {
        fprintf(stderr, "We didn't get the wanted format.\n");
        return 0;
    }

    SDL_PauseAudioDevice(mic_dev, 0);

    return 1;
}

void mic_quit(){
    SDL_CloseAudioDevice(mic_dev);
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
        !mic_init() ||
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

    mic_quit();
    mp3_quit();
    net_quit();

    #ifdef _WIN64
    WSACleanup();
    #endif

    return 0;
}


