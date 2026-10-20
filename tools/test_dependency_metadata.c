/* Actual dependency-file parser; only the platform file API is replaced. */
#include "test_paths.h"
#define _CX16_H
#include "../src/dependencyResolver.c"

char gameId[GAMEID_MAX_LENGTH + 1] = "KQ3";
static FILE *input;
static boolean readFailure;
#define CHECK(c,m) do { if (!(c)) { fprintf(stderr,"%s\n",m); exit(1); } } while (0)

unsigned char cbm_open(unsigned char lfn, unsigned char device, unsigned char address, const char *name)
{
    char path[4096], relative[128];
    (void)lfn; (void)device; (void)address;
    CHECK(sscanf(name,"%127[^,]",relative)==1,"Invalid metadata filename");
    suite_test_path(path,sizeof(path),suite_fixture_directory,relative);
    input=fopen(path,"rb"); return input?0:4;
}
int cbm_read(unsigned char lfn, void *buffer, unsigned int size)
{ (void)lfn; return readFailure?-1:(int)fread(buffer,1,size,input); }
void cbm_close(unsigned char lfn) { (void)lfn; CHECK(!fclose(input),"Metadata close failed"); input=NULL; }

static void fixture(const char *type, const char *extension, const byte *bytes, size_t size)
{
    char name[128], path[4096];
    snprintf(name,sizeof(name),"meta/KQ3-%s-ids.%s",type,extension);
    suite_test_path(path,sizeof(path),suite_fixture_directory,name);
    FILE *f=fopen(path,"wb"); CHECK(f,"Cannot create metadata fixture");
    CHECK(fwrite(bytes,1,size,f)==size&&!fclose(f),"Cannot write metadata fixture");
}
static void valid_files(void)
{
    static const byte index[]={0,0,1}, data[]={7};
    const char *types[]={"logic","sound","view"};
    for(unsigned i=0;i<3;++i) { fixture(types[i],"idx",index,sizeof(index)); fixture(types[i],"bin",data,sizeof(data)); }
}
int main(void)
{
    char path[4096];
    suite_test_paths_init("dependency-metadata");
    suite_test_path(path,sizeof(path),suite_fixture_directory,"meta");
    CHECK(!mkdir(path,0700),"Cannot create metadata directory");
    b4InitMetadata(); CHECK(!b4IsInited,"Missing dependency files were published as initialized");
    valid_files(); b4InitMetadata(); CHECK(b4IsInited,"Valid dependency files were rejected");
    static const byte truncated[]={0,0}, outside[]={0xff,0x7f,1};
    fixture("sound","idx",truncated,sizeof(truncated));
    b4InitMetadata(); CHECK(!b4IsInited,"Partial dependency index record was accepted");
    valid_files(); fixture("view","idx",outside,sizeof(outside));
    b4InitMetadata(); CHECK(!b4IsInited,"Dependency offset outside loaded metadata was accepted");
    byte oversized[701]; memset(oversized,7,sizeof(oversized));
    valid_files(); fixture("view","bin",oversized,sizeof(oversized));
    b4InitMetadata(); CHECK(!b4IsInited,"Oversized metadata file was silently truncated");
    valid_files(); readFailure=TRUE; b4InitMetadata();
    CHECK(!b4IsInited,"Negative metadata read was accepted");
    readFailure=FALSE; valid_files(); b4InitMetadata();
    CHECK(b4IsInited,"Dependency metadata could not recover after failure");
    static const byte emptyIndex[]={0,0,0};
    fixture("sound","idx",emptyIndex,sizeof(emptyIndex)); fixture("sound","bin",emptyIndex,0);
    b4InitMetadata(); CHECK(b4IsInited,"Empty dependency list was rejected");
    puts("PASS dependency metadata rejects missing, partial, out-of-range and failed reads, then retries");
    return 0;
}
