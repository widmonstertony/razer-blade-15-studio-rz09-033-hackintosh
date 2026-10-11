/* Exact-target app-only initialization, once per boot. No framebuffer arm,
 * sysctl write, EFI/NVRAM/security change, workload, wake hook or retry.
 * --inspect / --self-test never open a GPU user client.
 */
#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CommonCrypto/CommonDigest.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define HOME_DIR "/Library/Application Support/RazerNVIDIA"
#define ARGS "-v keepsyms=1 debug=0x100 amfi=0x80 -lilubetaall -razer-oled25g241-test nvaccel=1 nvfb=0 -nvrmnogo -nvrmnobootscreen -nvrmnoflip"
#define ALLOW "# NVIDIA app-only Metal access; WindowServer stays on Intel.\n-WindowServer\n*\n"
#define VRAM UINT64_C(17179869184)
extern int csr_get_active_config(uint32_t *);
typedef struct { int64_t sec,usec; uint64_t id,vram,bar; unsigned vendor,device,sv,sd,n,a,fb,agdc,override,nubs; int args,build,headless,autoOff; } State;

static unsigned count(const char *cls,io_service_t *first){
 io_iterator_t it=0;unsigned n=0;if(first)*first=0;
 if(IOServiceGetMatchingServices(kIOMainPortDefault,IOServiceMatching(cls),&it)!=KERN_SUCCESS)return UINT32_MAX;
 io_service_t s;while((s=IOIteratorNext(it))){if(++n==1&&first)*first=s;else IOObjectRelease(s);}IOObjectRelease(it);return n;
}
static uint64_t num(io_registry_entry_t e,CFStringRef key){
 CFTypeRef p=IORegistryEntryCreateCFProperty(e,key,kCFAllocatorDefault,0);uint64_t n=0;
 if(p&&CFGetTypeID(p)==CFNumberGetTypeID())CFNumberGetValue(p,kCFNumberSInt64Type,&n);
 else if(p&&CFGetTypeID(p)==CFDataGetTypeID()&&CFDataGetLength(p)>0&&CFDataGetLength(p)<=8)memcpy(&n,CFDataGetBytePtr(p),(size_t)CFDataGetLength(p));
 if(p)CFRelease(p);return n;
}
static int text(io_registry_entry_t e,CFStringRef key,char *out,size_t size){
 CFTypeRef p=IORegistryEntryCreateCFProperty(e,key,kCFAllocatorDefault,0);
 int ok=p&&CFGetTypeID(p)==CFStringGetTypeID()&&CFStringGetCString(p,out,(CFIndex)size,kCFStringEncodingUTF8);
 if(p)CFRelease(p);return ok;
}
static int inspect(State *s,io_service_t *service){
 memset(s,0,sizeof *s);*service=0;struct timeval boot={0};size_t len=sizeof boot;
 if(sysctlbyname("kern.boottime",&boot,&len,NULL,0))return 0;s->sec=boot.tv_sec;s->usec=boot.tv_usec;
 char buf[512]={0};len=sizeof buf;s->args=!sysctlbyname("kern.bootargs",buf,&len,NULL,0)&&!strcmp(buf,ARGS);
 len=sizeof buf;s->build=!sysctlbyname("kern.osversion",buf,&len,NULL,0)&&!strcmp(buf,"25G241");
 int h=0;len=sizeof h;s->headless=!sysctlbyname("debug.nvaccel_headless",&h,&len,NULL,0)&&h==1;
 io_service_t n=0,a=0;s->n=count("NVRM",&n);s->a=count("NVAccel",&a);
 s->fb=count("NVRMFB",NULL);s->agdc=count("NVRMAGDC",NULL);s->override=count("NMIntelFB",NULL);s->nubs=count("NVRMDisplay",NULL);
 if(!n||!a){if(n)IOObjectRelease(n);if(a)IOObjectRelease(a);return 0;}
 IORegistryEntryGetRegistryEntryID(n,&s->id);
 CFTypeRef hp=IORegistryEntryCreateCFProperty(a,CFSTR("NVAccelHeadless"),kCFAllocatorDefault,0);
 s->headless=s->headless&&hp&&CFGetTypeID(hp)==CFBooleanGetTypeID()&&CFBooleanGetValue(hp);if(hp)CFRelease(hp);
 s->autoOff=text(n,CFSTR("nvrm-autogo"),buf,sizeof buf)&&!strcmp(buf,"off (-nvrmnogo)");
 if(text(n,CFSTR("nvrm-bars"),buf,sizeof buf)){char *b=strstr(buf,"bar1@0x14:");unsigned long long addr=0,bytes=0;if(b&&sscanf(b,"bar1@0x14:0x%llx+0x%llx",&addr,&bytes)==2)s->bar=bytes;}
 io_registry_entry_t p=0;if(IORegistryEntryGetParentEntry(n,kIOServicePlane,&p)!=KERN_SUCCESS||!IOObjectConformsTo(p,"IOPCIDevice")){if(p)IOObjectRelease(p);IOObjectRelease(n);IOObjectRelease(a);return 0;}
 s->vendor=(unsigned)num(p,CFSTR("vendor-id"));s->device=(unsigned)num(p,CFSTR("device-id"));s->sv=(unsigned)num(p,CFSTR("subsystem-vendor-id"));s->sd=(unsigned)num(p,CFSTR("subsystem-id"));s->vram=num(p,CFSTR("VRAM,totalsize"));
 IOObjectRelease(p);IOObjectRelease(a);*service=n;return 1;
}
static int eligible(const State *s){return s->sec>0&&s->id&&s->args&&s->build&&s->headless&&s->autoOff&&s->n==1&&s->a==1&&s->vendor==0x10de&&s->device==0x1eb5&&s->sv==0x1a58&&s->sd==0x200a&&s->bar==268435456&&!s->fb&&!s->agdc&&!s->override&&((s->vram==0&&s->nubs==0)||(s->vram==VRAM&&s->nubs==1));}
static int panelReady(void){
 struct stat console;if(stat("/dev/console",&console)||console.st_uid<501)return 0;
 CGDirectDisplayID d=CGMainDisplayID();return d&&CGDisplayIsActive(d)&&CGDisplayIsBuiltin(d)&&CGDisplayVendorNumber(d)==0x4c83&&CGDisplayModelNumber(d)==0xa029;
}
static void dump(const State *s){printf("{\"bootSec\":%"PRId64",\"bootUsec\":%"PRId64",\"nvrmId\":%"PRIu64",\"vramBytes\":%"PRIu64",\"bar1Bytes\":%"PRIu64",\"headless\":%s,\"autoGoOff\":%s,\"eligible\":%s,\"intelPanelReady\":%s}\n",s->sec,s->usec,s->id,s->vram,s->bar,s->headless?"true":"false",s->autoOff?"true":"false",eligible(s)?"true":"false",panelReady()?"true":"false");}
static int trustedDir(const char *p){struct stat st;return !lstat(p,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==0&&!(st.st_mode&0022);}
static int pin(const char *p,const char *want){
 int fd=open(p,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return 0;struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||(st.st_mode&0022)||st.st_size<1||st.st_size>536870912){close(fd);return 0;}
 void *map=mmap(NULL,(size_t)st.st_size,PROT_READ,MAP_PRIVATE,fd,0);if(map==MAP_FAILED){close(fd);return 0;}
 unsigned char digest[CC_SHA256_DIGEST_LENGTH];char hex[65];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
 CC_SHA256(map,(CC_LONG)st.st_size,digest);
#pragma clang diagnostic pop
 for(unsigned i=0;i<sizeof digest;i++)snprintf(hex+i*2,3,"%02x",digest[i]);munmap(map,(size_t)st.st_size);close(fd);return !strcmp(hex,want);
}
static int fileGates(void){
 const char *pins[][2]={
 {"/Library/Extensions/NVRM.kext/Contents/MacOS/NVRM","4c74eb5a5be05e8819f2d8bd6ec41e09978b6ef9dccbd06de54f4ac927c32d96"},
 {"/Library/Extensions/NVRM.kext/Contents/Info.plist","e9a4e57961d7da3328a6d33f5db514777db61c87841dc5b8a3c289d5601f3391"},
 {"/Library/Extensions/NVAccel.kext/Contents/MacOS/NVAccel","f687a54d88eda9e065bda5275877d71f0e6eb2345de63813ebe97b69de498af5"},
 {"/Library/Extensions/NVAccel.kext/Contents/Info.plist","52bf9ec56f1bf143a931e61f31afe154bc76972858214e2ffb9578654925f5d8"},
 {"/Library/GPUBundles/NVMTLDriver.bundle/Contents/MacOS/NVMTLDriver","a9877ab9bb61722b7a1b2f5a0458dc5cb83890370e16fa46c1030377b864640b"},
 {"/Library/GPUBundles/NVIDIAShared.bundle/Contents/MacOS/NVIDIAShared","9792f5e8f8e75bf61f74088e3cb8df6dea00ffb3bfbb78c7cc0f060e40a02d1e"},
 {"/Users/Shared/nvfw/nvidia/610.57.04/gsp_tu10x.bin","d157e3b7dd5da2ca8d1ccb6ca98958f9e35d10a9ef7326277ebac133e4b0d1a7"},
 {"/Users/Shared/nvfw/nvidia/610.57.04/ucodes_tu10x.bin","dcbdf512ab09b7f6d946e7f415bcfbc9137f5db1cf0c3c576f4b34b5275acb38"},
 {"/System/Library/KernelCollections/BootKernelExtensions.kc","f94046e9be8f83e014ab1965d3ee5a8f11048c17c60d27850d7dee3672e06cdc"},
 {"/System/Library/KernelCollections/SystemKernelExtensions.kc","9a25de6e34c28f8cd9802aa61bbcf6395f5884de70e39558523ba82e90b3c4ae"},
 {"/Library/KernelCollections/AuxiliaryKernelExtensions.kc","3b323074df4dea0c4e61de275e96e0ca5713eb20d9da5057394e09f9b8562b14"}};
 for(unsigned i=0;i<sizeof pins/sizeof pins[0];i++)if(!pin(pins[i][0],pins[i][1])){fprintf(stderr,"FILE_GATE_REFUSED=%s\n",pins[i][0]);return 0;}
 int fd=open("/Library/GPUBundles/nvmtl-allow.txt",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);char textbuf[256]={0};struct stat st;
 if(fd<0)return 0;int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_uid==0&&!(st.st_mode&0022)&&read(fd,textbuf,sizeof textbuf-1)==(ssize_t)strlen(ALLOW)&&!strcmp(textbuf,ALLOW);close(fd);
 uint32_t sip=0;return ok&&!csr_get_active_config(&sip)&&sip==0x803;
}
static int record(int fd,const char *line){size_t len=strlen(line),off=0;while(off<len){ssize_t n=write(fd,line+off,len-off);if(n<0&&errno==EINTR)continue;if(n<=0)return 0;off+=(size_t)n;}return !fsync(fd)&&fcntl(fd,F_FULLFSYNC)==0;}
typedef struct { kern_return_t (*open)(io_service_t,task_port_t,uint32_t,io_connect_t *);kern_return_t (*call)(mach_port_t,uint32_t,const uint64_t *,uint32_t,uint64_t *,uint32_t *);kern_return_t (*close)(io_connect_t); } API;
typedef struct { unsigned opens,calls,closes; kern_return_t open,call,close; } Result;
static Result perform(const API *api,io_service_t s){Result r={0,0,0,KERN_FAILURE,KERN_FAILURE,KERN_FAILURE};io_connect_t c=0;r.opens=1;r.open=api->open(s,mach_task_self(),0,&c);if(r.open)return r;uint64_t pass=2;uint32_t outs=0;r.calls=1;r.call=api->call(c,0,&pass,1,NULL,&outs);r.closes=1;r.close=api->close(c);return r;}
static int failure,wrong;static kern_return_t fakeOpen(io_service_t s,task_port_t t,uint32_t type,io_connect_t *c){(void)s;(void)t;if(type)wrong=1;*c=123;return failure==1?kIOReturnError:KERN_SUCCESS;}
static kern_return_t fakeCall(mach_port_t c,uint32_t sel,const uint64_t *in,uint32_t n,uint64_t *out,uint32_t *nout){if(c!=123||sel||n!=1||*in!=2||out||*nout)wrong=1;return failure==2?kIOReturnError:KERN_SUCCESS;}
static kern_return_t fakeClose(io_connect_t c){if(c!=123)wrong=1;return failure==3?kIOReturnError:KERN_SUCCESS;}
static int selfTest(void){
 State good={.sec=1,.id=1,.bar=268435456,.vendor=0x10de,.device=0x1eb5,.sv=0x1a58,.sd=0x200a,.n=1,.a=1,.args=1,.build=1,.headless=1,.autoOff=1};if(!eligible(&good))return 1;
#define REJECT(field,value) do{State b=good;b.field=value;if(eligible(&b))return 2;}while(0)
 REJECT(args,0);REJECT(build,0);REJECT(headless,0);REJECT(autoOff,0);REJECT(vendor,0x8086);REJECT(device,1);REJECT(sv,0);REJECT(sd,0);REJECT(n,2);REJECT(a,0);REJECT(fb,1);REJECT(agdc,1);REJECT(override,1);REJECT(bar,0);REJECT(vram,42);REJECT(nubs,1);
 good.vram=VRAM;good.nubs=1;if(!eligible(&good))return 3;
 API fake={fakeOpen,fakeCall,fakeClose};for(failure=0;failure<4;failure++){wrong=0;Result r=perform(&fake,1);if(wrong||r.opens!=1||r.calls!=(failure!=1)||r.closes!=(failure!=1))return 4;}
 char tmp[]="/private/tmp/nvidia-app-only-journal.XXXXXX",name[512];if(!mkdtemp(tmp))return 5;snprintf(name,sizeof name,"%s/test.jsonl",tmp);int fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return 6;int durable=record(fd,"{\"mock\":true}\n");close(fd);fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);int denied=fd<0&&errno==EEXIST;if(fd>=0)close(fd);int removed=!unlink(name)&&!rmdir(tmp);if(!durable||!denied||!removed)return 7;
 puts("SELF_TEST=PASS; 16 rejection cases, initialized-state skip, 4 mocked outcomes, durable exclusive temp journal; GPU_CALLS=0");return 0;
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
 if(argc==2&&!strcmp(argv[1],"--self-test"))return selfTest();
 int run=argc==2&&!strcmp(argv[1],"--run");if(argc!=2||(!run&&strcmp(argv[1],"--inspect")))return 64;
 if(run&&getuid()!=0){fputs("ROOT_SERVICE_REQUIRED; GPU_CALLS=0\n",stderr);return 65;}
 State s={0};io_service_t service=0;
 if(!run){if(!inspect(&s,&service))return 66;dump(&s);IOObjectRelease(service);return 0;}
 /* Wait for desktop/headless readiness, not for a GPU call to succeed. A go()
  * failure or existing journal NEVER gets retried. No KeepAlive in launchd. */
 for(unsigned i=0;i<360;i++){
  if(inspect(&s,&service)&&eligible(&s)&&panelReady())break;
  if(service){IOObjectRelease(service);service=0;}
  char args[512]={0};size_t len=sizeof args;if(!sysctlbyname("kern.bootargs",args,&len,NULL,0)&&strcmp(args,ARGS)){puts("BOOT_ARGS_CHANGED_OR_RECOVERY_ACTIVE; GPU_CALLS=0");return 67;}
  sleep(5);
 }
 if(!service||!eligible(&s)||!panelReady()){if(service)IOObjectRelease(service);puts("DESKTOP_NOT_READY; GPU_CALLS=0");return 68;}
 dump(&s);if(!fileGates()){IOObjectRelease(service);puts("FILE_OR_SECURITY_GATE_REFUSED; GPU_CALLS=0");return 69;}
 if(s.vram==VRAM){IOObjectRelease(service);puts("ALREADY_INITIALIZED_THIS_BOOT; GPU_CALLS=0");return 0;}
 if(!trustedDir(HOME_DIR)||!trustedDir(HOME_DIR "/journals")){IOObjectRelease(service);return 70;}
 char name[512],line[768];snprintf(name,sizeof name,HOME_DIR "/journals/boot-%"PRId64"-%"PRId64".jsonl",s.sec,s.usec);
 int fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0){IOObjectRelease(service);puts("JOURNAL_EXISTS_OR_UNAVAILABLE; NO_RETRY; GPU_CALLS=0");return 71;}
 snprintf(line,sizeof line,"{\"phase\":\"PENDING_BEFORE_OPEN\",\"bootSec\":%"PRId64",\"bootUsec\":%"PRId64",\"nvrmId\":%"PRIu64",\"selector\":0,\"scalar\":2,\"maxCalls\":1}\n",s.sec,s.usec,s.id);
 if(!record(fd,line)){close(fd);IOObjectRelease(service);return 72;}
 const API real={IOServiceOpen,IOConnectCallScalarMethod,IOServiceClose};Result r=perform(&real,service);IOObjectRelease(service);
 snprintf(line,sizeof line,"{\"phase\":\"RETURNED\",\"opens\":%u,\"calls\":%u,\"closes\":%u,\"openStatus\":%u,\"callStatus\":%u,\"closeStatus\":%u}\n",r.opens,r.calls,r.closes,(unsigned)r.open,(unsigned)r.call,(unsigned)r.close);int saved=record(fd,line);close(fd);fputs(line,stdout);
 if(!saved||r.open||r.call||r.close)return 73;
 if(!inspect(&s,&service))return 74;dump(&s);IOObjectRelease(service);
 if(!eligible(&s)||s.vram!=VRAM||!panelReady())return 75;
 puts("RESULT=NVIDIA-APP-ONLY-INITIALIZED-ONCE; INTEL_PANEL_PRESERVED; NO_WORKLOAD_OR_DISPLAY_ARM");return 0;
}
