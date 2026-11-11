/* Native homebrew settings UI. Compiled unchanged for the Linux laboratory.
 * All settings are committed by the resident console service over localhost.
 */
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "client.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __PROSPERO__
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
extern int sceSystemServiceHideSplashScreen(void);
#endif

typedef struct { char ch; unsigned char rows[7]; } glyph;
static const glyph font[] = {
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {'.',{0,0,0,0,0,4,4}}, {':',{0,4,4,0,4,4,0}},
 {'-',{0,0,0,31,0,0,0}}, {'/',{1,2,2,4,8,8,16}},
 {'%',{17,2,4,8,17,0,0}}, {'+',{0,4,4,31,4,4,0}},
 {'(',{2,4,8,8,8,4,2}}, {')',{8,4,2,2,2,4,8}},
 {'>',{16,8,4,2,4,8,16}}, {'=',{0,31,0,31,0,0,0}}
};
static SDL_Renderer *renderer;
#ifdef __PROSPERO__
static SDL_Surface *native_surface;
static SDL_Window *native_window;
static SDL_GLContext native_context;
static GLuint native_texture, native_program, native_array;
static int native_present_init(void)
{
    const char *vertex="#version 330 core\nout vec2 uv;void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}";
    const char *fragment="#version 330 core\nin vec2 uv;out vec4 c;uniform sampler2D panel;void main(){c=texture(panel,vec2(uv.x,1.0-uv.y));}";
    GLuint vs=glCreateShader(GL_VERTEX_SHADER),fs=glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok=0;glShaderSource(vs,1,&vertex,NULL);glCompileShader(vs);glGetShaderiv(vs,GL_COMPILE_STATUS,&ok);if(!ok)return -1;
    glShaderSource(fs,1,&fragment,NULL);glCompileShader(fs);glGetShaderiv(fs,GL_COMPILE_STATUS,&ok);if(!ok)return -1;
    native_program=glCreateProgram();glAttachShader(native_program,vs);glAttachShader(native_program,fs);glLinkProgram(native_program);glGetProgramiv(native_program,GL_LINK_STATUS,&ok);
    glDeleteShader(vs);glDeleteShader(fs);if(!ok)return -1;
    glGenVertexArrays(1,&native_array);glBindVertexArray(native_array);
    glGenTextures(1,&native_texture);glBindTexture(GL_TEXTURE_2D,native_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1280,720,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glDisable(GL_DEPTH_TEST);return 0;
}
static void native_present(void)
{
    glViewport(0,0,1920,1080);glUseProgram(native_program);glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,native_texture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,native_surface->pixels);
    glUniform1i(glGetUniformLocation(native_program,"panel"),0);glBindVertexArray(native_array);glDrawArrays(GL_TRIANGLES,0,3);SDL_GL_SwapWindow(native_window);
}
#endif
static void color(unsigned c) { SDL_SetRenderDrawColor(renderer, c>>16, c>>8, c, 255); }
static void box(int x,int y,int w,int h,unsigned c)
{ color(c); SDL_Rect r={x,y,w,h}; SDL_RenderFillRect(renderer,&r); }
static void text(int x,int y,const char *s,int scale,unsigned c)
{
    color(c);
    for (; *s; s++, x+=6*scale) {
        unsigned char ch=(unsigned char)*s;
        if (ch>='a' && ch<='z') ch-=32;
        if (ch & 0x80) continue;
        for (size_t g=0;g<sizeof(font)/sizeof(font[0]);g++) if (font[g].ch==ch) {
            for (int row=0;row<7;row++) for(int col=0;col<5;col++) if(font[g].rows[row]&(1<<(4-col))) {
                SDL_Rect r={x+col*scale,y+row*scale,scale,scale}; SDL_RenderFillRect(renderer,&r);
            }
            break;
        }
    }
}
static ga_native_snapshot state;
static int connected, port=8090, tab, selected;
static char feedback[96]="Aguardando o servico do console";
static int request(const char *route,const char *fields)
{
    ga_native_snapshot next;
    int r=ga_native_request(port,connected?&state:NULL,route,fields,&next);
    if (!r) { state=next; connected=1; snprintf(feedback,sizeof(feedback),fields?"Acao aplicada no perfil %s":"Servico disponivel - perfil %s",state.title); }
    else if(r==-409) snprintf(feedback,sizeof(feedback),"O perfil mudou. Confira e repita o ajuste.");
    else if(r==-503) snprintf(feedback,sizeof(feedback),"Falha ao salvar. Ajuste anterior preservado.");
    else if(r==-400) snprintf(feedback,sizeof(feedback),"Acao indisponivel no estado atual.");
    else { connected=0; snprintf(feedback,sizeof(feedback),"Ative o servico Gyro Aim na Toolbox do etaHEN."); }
    return r;
}
static void set_value(const char *key,float value)
{ char fields[128];snprintf(fields,sizeof(fields),"%s=%.4f",key,value);request("/api/native/set",fields); }
static void action(const char *name)
{ char fields[128];snprintf(fields,sizeof(fields),"action=%s",name);request("/api/native/action",fields); }
static const char *labels[]={"Gyro habilitado","Sensibilidade","Compensacao da zona morta","Ativar com","Modo de ativacao","Proporcao vertical","Suavizacao","Filtro de tremor","Inverter horizontal","Inverter vertical","Atalhos do touchpad"};
static const char *help[]={"Habilita o gyro somente no perfil selecionado.","Aumente se precisa girar demais o controle.","Aumente se movimentos pequenos nao movem a mira.","L2 para mirar. ALWAYS para camera livre.","Segurar ou apertar para ligar/desligar.","Ajuste a relacao entre mira vertical e horizontal.","Mais suavizacao pode trazer atraso na mira.","Filtro para o tremor natural da mao.","Inverte o movimento horizontal do gyro.","Inverte o movimento vertical do gyro.","Desative se o jogo precisa do clique do touchpad."};
static void value_label(int row,char *buf,size_t size)
{
    const ga_config *c=&state.cfg;
    switch(row){
    case 0:snprintf(buf,size,"%s",c->enabled?"LIGADO":"DESLIGADO");break;
    case 1:snprintf(buf,size,"%.1f",c->sensitivity);break;
    case 2:snprintf(buf,size,"%.0f%%",c->anti_deadzone*100);break;
    case 3:snprintf(buf,size,"%s",ga_button_id(c->activation));break;
    case 4:snprintf(buf,size,"%s",c->toggle_mode?"ALTERNAR":"SEGURAR");break;
    case 5:snprintf(buf,size,"%.0f%%",c->vertical_ratio*100);break;
    case 6:snprintf(buf,size,"%.0f%%",c->smoothing*100);break;
    case 7:snprintf(buf,size,"%.1f GRAUS/S",c->gyro_deadzone);break;
    case 8:snprintf(buf,size,"%s",c->invert_x?"SIM":"NAO");break;
    case 9:snprintf(buf,size,"%s",c->invert_y?"SIM":"NAO");break;
    default:snprintf(buf,size,"%s",c->hotkeys_enabled?"LIGADOS":"DESLIGADOS");break;
    }
}
static void change(int direction)
{
    if(!connected)return;
    ga_config *c=&state.cfg;
    if(tab==0){switch(selected){
    case 0:set_value("enabled",!c->enabled);break;
    case 1:set_value("sensitivity",c->sensitivity+direction*.1f);break;
    case 2:set_value("anti_deadzone",c->anti_deadzone+direction*.01f);break;
    case 3:{int b=(int)c->activation+direction;if(b<=GA_BTN_NONE)b=GA_BTN_COUNT-1;if(b>=GA_BTN_COUNT)b=1;char f[80];snprintf(f,sizeof(f),"activation=%s",ga_button_id((ga_button)b));request("/api/native/set",f);break;}
    case 4:set_value("toggle_mode",!c->toggle_mode);break;
    case 5:set_value("vertical_ratio",c->vertical_ratio+direction*.05f);break;
    case 6:set_value("smoothing",c->smoothing+direction*.05f);break;
    case 7:set_value("gyro_deadzone",c->gyro_deadzone+direction*.1f);break;
    case 8:set_value("invert_x",!c->invert_x);break;
    case 9:set_value("invert_y",!c->invert_y);break;
    case 10:set_value("hotkeys_enabled",!c->hotkeys_enabled);break;
    }}else if(tab==1){
        if(selected==0)action(state.calibrating?"cancel_calibration":"calibrate");
        else if(selected==1)action(state.testing?"keep_test":"begin_test");
        else if(selected==2)action("revert_test");
        else if(selected==3)action("undo");
        else if(selected==4)action("restore_defaults");
    }else if(tab==2){
        if(selected<(int)state.profile_count){
            char fields[128];snprintf(fields,sizeof(fields),"next_title=%s",state.profiles[selected]);
            request("/api/native/title",fields);
        }else if(selected==(int)state.profile_count)action("use_default");
        else if(selected==(int)state.profile_count+1)request("/api/native/save_default","");
    }
}
static int row_count(void){return tab==0?11:tab==1?5:tab==2?(int)state.profile_count+2:1;}
static void navigate(int key)
{
    if(key==SDLK_TAB){tab=(tab+1)%4;selected=0;return;}
    if(key==SDLK_PAGEUP){tab=(tab+3)%4;selected=0;return;}
    if(key==SDLK_PAGEDOWN){tab=(tab+1)%4;selected=0;return;}
    int count=row_count();if(count<1)count=1;
    if(key==SDLK_DOWN)selected=(selected+1)%count;
    if(key==SDLK_UP)selected=(selected+count-1)%count;
    if(tab==0 && key==SDLK_LEFT)change(-1);
    if((tab==0 && key==SDLK_RIGHT)||key==SDLK_RETURN)change(1);
}
static void draw(void)
{
    box(0,0,1280,720,0x0b1019);box(0,0,1280,104,0x141e2d);
    text(38,24,"GYRO AIM",4,0xeaf2ff);text(40,67,"CONFIGURACAO NO PS5",2,0x90bfff);
    text(660,32,connected?"SERVICO CONECTADO":"SERVICO INDISPONIVEL",2,connected?0x80e4b5:0xffd18c);
    char title[96];snprintf(title,sizeof(title),"PERFIL: %s",connected?state.title:"---");text(660,65,title,2,0xd2dcec);
    const char *tabs[]={"MIRA","SENSOR E RECUPERACAO","PERFIS","AJUDA"};
    for(int i=0;i<4;i++){box(38+i*304,119,294,40,tab==i?0x305d92:0x172334);text(50+i*304,133,tabs[i],2,0xeaf2ff);}
    box(38,174,800,432,0x141e2d);box(854,174,388,432,0x141e2d);
    int start=selected>=9?selected-8:0;
    if(tab==0){for(int r=start;r<11&&r<start+9;r++){int y=187+(r-start)*44;char value[64];value_label(r,value,sizeof(value));
        if(r==selected)box(48,y-4,780,37,0x263e5b);
        text(61,y+6,labels[r],2,0xeaf2ff);text(580,y+6,connected?value:"---",2,0x91bfff);}
        text(874,198,"COMO AJUSTAR",2,0x90bfff);
        /* Wrap ASCII help to keep all text within the card. */
        const char *hint=help[selected];char line[28];int pos=0,y=238;
        while(*hint){int n=0;while(hint[n]&&n<27)n++;if(hint[n])while(n>0&&hint[n]!=' ')n--;if(!n)n=27;memcpy(line,hint,n);line[n]=0;text(874,y,line,2,0xc6d2e4);hint+=n;while(*hint==' ')hint++;y+=25;pos++;if(pos>8)break;}
    }else if(tab==1){const char *rows[]={state.calibrating?"Cancelar calibracao":"Calibrar sensor parado","Marcar/manter ponto de retorno","Voltar ao ponto marcado","Desfazer ultimo ajuste","Restaurar default deste perfil"};
        for(int r=0;r<5;r++){int y=201+r*65;if(r==selected)box(48,y-10,780,49,0x263e5b);text(65,y+5,rows[r],2,0xeaf2ff);}
        text(874,200,"RECUPERACAO",2,0x90bfff);text(874,246,"RESTAURAR DEFAULT",2,0xeaf2ff);text(874,278,"AFETA SO ESTE PERFIL.",2,0xc6d2e4);text(874,310,"PODE SER DESFEITO.",2,0xc6d2e4);
        text(874,365,"CALIBRE COM O",2,0xc6d2e4);text(874,394,"CONTROLE APOIADO.",2,0xc6d2e4);
    }else if(tab==2){if(!connected)text(62,205,"CONECTE AO SERVICO PARA LISTAR OS PERFIS.",2,0xc6d2e4);
        for(int r=start;connected&&r<row_count()&&r<start+9;r++){int y=193+(r-start)*44;if(r==selected)box(48,y-4,780,37,0x263e5b);const char *label=r<(int)state.profile_count?state.profiles[r]:r==(int)state.profile_count?"Copiar padrao para este perfil":"Usar este perfil como padrao";text(65,y+5,label,2,0xeaf2ff);}
        text(874,202,"PERFIL POR JOGO",2,0x90bfff);text(874,248,"X: EDITAR PERFIL",2,0xc6d2e4);text(874,290,"AO ABRIR UM JOGO,",2,0xc6d2e4);text(874,318,"O SERVICO CONSULTA",2,0xc6d2e4);text(874,346,"SEU TITLE ID.",2,0xc6d2e4);
    }else{const char *lines[]={"D-PAD CIMA/BAIXO: ESCOLHER AJUSTE","D-PAD ESQUERDA/DIREITA: ALTERAR","X: APLICAR ACAO    L1/R1: ABAS","TRIANGULO: PAUSAR/RETOMAR GYRO","QUADRADO: DESFAZER ULTIMO AJUSTE","CIRCULO: FECHAR O APP DE CONFIGURACAO","O SERVICO ETAHEN CONTINUA SEPARADO.","NAO HA OVERLAY INTERATIVO NO JOGO."};for(int i=0;i<8;i++)text(61,198+i*45,lines[i],2,0xc6d2e4);text(874,204,"PAINEL NO CELULAR",2,0x90bfff);text(874,250,"MESMA REDE DO PS5",2,0xc6d2e4);}
    if(tab!=0){text(874,490,"ENDERECO DO PAINEL",2,0x90bfff);char url[32];snprintf(url,sizeof(url),"%.30s",connected?state.panel_url:"HTTP://IP-DO-PS5:8090");text(874,528,url,2,0xeaf2ff);}
    if(tab==0){text(874,470,"ENTRADA NOS JOGOS",2,0x90bfff);text(874,510,state.output_ready?"BACKEND ATIVO":"AINDA INDISPONIVEL",2,state.output_ready?0x80e4b5:0xffd18c);text(874,548,"VALIDACAO NO PS5",2,0xc6d2e4);text(874,576,"AINDA NECESSARIA",2,0xc6d2e4);}
    text(40,628,feedback,2,0xffd18c);
    box(0,667,1280,53,0x141e2d);text(40,687,"D-PAD: AJUSTAR   X: APLICAR   L1/R1: ABAS   CIRCULO: FECHAR",2,0xeaf2ff);
    SDL_RenderPresent(renderer);
#ifdef __PROSPERO__
    native_present();
#endif
}
int main(int argc,char **argv)
{
    const char *preview=NULL;
    for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--port")&&i+1<argc)port=atoi(argv[++i]);else if(!strcmp(argv[i],"--preview")&&i+1<argc)preview=argv[++i];else if(!strcmp(argv[i],"--tab")&&i+1<argc)tab=atoi(argv[++i]);else if(!strcmp(argv[i],"--selection")&&i+1<argc)selected=atoi(argv[++i]);}
    if(preview)SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO|(preview?0:SDL_INIT_GAMECONTROLLER))){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
#ifdef __PROSPERO__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,0);
    SDL_Window *window=SDL_CreateWindow("Gyro Aim",0,0,1920,1080,SDL_WINDOW_SHOWN|SDL_WINDOW_OPENGL);
    if(!window){SDL_Quit();return 1;}
    native_window=window;native_context=SDL_GL_CreateContext(window);
    if(native_context)SDL_GL_SetSwapInterval(1);
    if(!native_context || native_present_init()){SDL_DestroyWindow(window);SDL_Quit();return 1;}
    native_surface=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_ABGR8888);
    renderer=native_surface?SDL_CreateSoftwareRenderer(native_surface):NULL;
#else
    SDL_Window *window=SDL_CreateWindow("Gyro Aim",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,720,preview?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN);
    if(!window){SDL_Quit();return 1;}
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
#endif
    if(!renderer){SDL_DestroyWindow(window);SDL_Quit();return 1;}
    SDL_RenderSetLogicalSize(renderer,1280,720);
#ifdef __PROSPERO__
    sceSystemServiceHideSplashScreen();
#endif
    SDL_GameController *pad=NULL;
    ga_config_default(&state.cfg);
    request("/api/native",NULL);
    if(tab<0||tab>3)tab=0;
    if(selected<0||selected>=row_count())selected=0;
    if(preview){draw();SDL_Surface *image=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_ARGB8888);
        if(!image){SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;}
        int result=SDL_RenderReadPixels(renderer,NULL,image->format->format,image->pixels,image->pitch);
        if(!result) { result=SDL_SaveBMP(image,preview); }
        SDL_FreeSurface(image);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return !!result;}
    int running=1;Uint32 scan=0;
    while(running){SDL_Event event;while(SDL_PollEvent(&event)){
        if(event.type==SDL_QUIT)running=0;
        if(event.type==SDL_KEYDOWN&&!event.key.repeat){if(event.key.keysym.sym==SDLK_ESCAPE)running=0;else navigate(event.key.keysym.sym);}
        if(event.type==SDL_CONTROLLERDEVICEADDED&&!pad)pad=SDL_GameControllerOpen(event.cdevice.which);
        if(event.type==SDL_CONTROLLERDEVICEREMOVED&&pad&&SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))==event.cdevice.which){SDL_GameControllerClose(pad);pad=NULL;}
        if(event.type==SDL_CONTROLLERBUTTONDOWN){switch(event.cbutton.button){
        case SDL_CONTROLLER_BUTTON_A:navigate(SDLK_RETURN);break;case SDL_CONTROLLER_BUTTON_B:running=0;break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:navigate(SDLK_UP);break;case SDL_CONTROLLER_BUTTON_DPAD_DOWN:navigate(SDLK_DOWN);break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:navigate(SDLK_LEFT);break;case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:navigate(SDLK_RIGHT);break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:navigate(SDLK_PAGEUP);break;case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:navigate(SDLK_PAGEDOWN);break;
        case SDL_CONTROLLER_BUTTON_X:if(connected)action("undo");break;case SDL_CONTROLLER_BUTTON_Y:if(connected)action("pause");break;
        }} }
        Uint32 now=SDL_GetTicks();if(now-scan>500){scan=now;ga_native_snapshot next;if(!ga_native_request(port,NULL,"/api/native",NULL,&next)){state=next;connected=1;}else connected=0;}
        draw();SDL_Delay(16);
    }
    if(pad) { SDL_GameControllerClose(pad); }
    SDL_DestroyRenderer(renderer);
#ifdef __PROSPERO__
    SDL_FreeSurface(native_surface);glDeleteTextures(1,&native_texture);glDeleteVertexArrays(1,&native_array);glDeleteProgram(native_program);SDL_GL_DeleteContext(native_context);
#endif
    SDL_DestroyWindow(window);SDL_Quit();return 0;
}
