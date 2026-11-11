/* The same native-app client talks to the real portable resident service. */
#include "../src/native/client.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FALHOU linha %d: %s\n",__LINE__,#c); fails++; } } while (0)
int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    int port=atoi(argv[1]);
    ga_native_snapshot a,b,unchanged;
    CHECK(ga_native_request(port,NULL,"/api/native",NULL,&a)==0);
    if(fails)return 1;
    CHECK(!strcmp(a.title,"NATIVE_TEST") && ga_native_snapshot_valid(&a));
    CHECK(ga_native_request(port,&a,"/api/native/set","sensitivity=3.1",&b)==0);
    CHECK(fabsf(b.cfg.sensitivity-3.1f)<.001f && b.revision>a.revision);
    memset(&unchanged,0,sizeof(unchanged));unchanged.magic=123;
    CHECK(ga_native_request(port,&a,"/api/native/action","action=restore_defaults",&unchanged)==-409);
    CHECK(unchanged.magic==123);
    a=b;
    CHECK(ga_native_request(port,&a,"/api/native/action","action=restore_defaults",&b)==0);
    CHECK(fabsf(b.cfg.sensitivity-1.5f)<.001f && b.can_undo);
    a=b;
    CHECK(ga_native_request(port,&a,"/api/native/action","action=undo",&b)==0);
    CHECK(fabsf(b.cfg.sensitivity-3.1f)<.001f);
    a=b;
    CHECK(ga_native_request(port,&a,"/api/native/title","next_title=ANOTHER_GAME",&b)==0);
    CHECK(!strcmp(b.title,"ANOTHER_GAME") && fabsf(b.cfg.sensitivity-1.5f)<.001f);
    a=b;
    CHECK(ga_native_request(port,&a,"/api/native/title","next_title=NATIVE_TEST",&b)==0);
    CHECK(fabsf(b.cfg.sensitivity-3.1f)<.001f);
    int listed=0;for(uint32_t i=0;i<b.profile_count;i++)if(!strcmp(b.profiles[i],"NATIVE_TEST"))listed=1;
    CHECK(listed);
    a=b;
    CHECK(ga_native_request(port,&a,"/api/native/save_default","",&b)==0);
    a=b;
    CHECK(ga_native_request(port,&a,"/api/native/title","next_title=THIRD_GAME",&b)==0);
    CHECK(fabsf(b.cfg.sensitivity-3.1f)<.001f);
    a=b;
    CHECK(ga_native_request(port,&a,"/api/native/set","sensitivity=4.1",&b)==0);
    a=b;
    CHECK(ga_native_request(port,&a,"/api/native/action","action=use_default",&b)==0);
    CHECK(fabsf(b.cfg.sensitivity-3.1f)<.001f);
    a=b;a.version++;CHECK(!ga_native_snapshot_valid(&a));
    a=b;a.size--;CHECK(!ga_native_snapshot_valid(&a));
    a=b;a.profile_count=GA_NATIVE_PROFILES+1;CHECK(!ga_native_snapshot_valid(&a));
    a=b;a.cfg.sensitivity=NAN;CHECK(!ga_native_snapshot_valid(&a));
    a=b;memset(a.title,'X',sizeof(a.title));CHECK(!ga_native_snapshot_valid(&a));
    if(!fails)puts("App nativo: IPC real, perfis, conflitos, restaurar e desfazer passaram");
    return !!fails;
}
