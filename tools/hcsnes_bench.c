/* Headless libretro benchmark, HCSNES 2026-10-25. See LICENSE-HCSNES.
 * Reports emulated and rendered rates separately. No display/audio-device cost.
 * Usage: hcsnes_bench core.so game.sfc [frames] [raw_state|-] [disabled|fixed] [cap]
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <dlfcn.h>
#include "libretro.h"

static const char *skip_mode="disabled", *skip_cap="1";
static struct retro_core_option_v2_definition *options;
static unsigned rendered, duplicated;
static uint64_t samples;
static void log_message(enum retro_log_level level,const char *fmt,...)
{
   va_list ap; (void)level;
   va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap);
}
static bool environment(unsigned cmd,void *data)
{
   switch(cmd)
   {
      case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *(unsigned*)data=2; return true;
      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
         options=((struct retro_core_options_v2*)data)->definitions; return true;
      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
         options=((struct retro_core_options_v2_intl*)data)->us->definitions; return true;
      case RETRO_ENVIRONMENT_GET_VARIABLE: {
         struct retro_variable *v=data;
         unsigned i;
         v->value=NULL;
         if(!strcmp(v->key,"snes9x_2005_frameskip")) v->value=skip_mode;
         else if(!strcmp(v->key,"snes9x_2005_frameskip_interval")) v->value=skip_cap;
         else if(options) for(i=0;options[i].key;++i)
            if(!strcmp(options[i].key,v->key)) {v->value=options[i].default_value;break;}
         return v->value!=NULL;
      }
      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *(bool*)data=false; return true;
      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: *(int*)data=3; return true;
      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
         return *(enum retro_pixel_format*)data==RETRO_PIXEL_FORMAT_RGB565;
      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
         ((struct retro_log_callback*)data)->log=log_message; return true;
      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char**)data="."; return true;
      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: return true;
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: return true;
      default: return false;
   }
}
static void video(const void *data,unsigned w,unsigned h,size_t pitch)
{ (void)w;(void)h;(void)pitch; if(data)++rendered;else ++duplicated; }
static size_t audio(const int16_t *data,size_t frames)
{ (void)data; samples+=frames; return frames; }
static void audio_one(int16_t l,int16_t r) { (void)l;(void)r;++samples; }
static void poll_input(void) {}
static int16_t input(unsigned p,unsigned d,unsigned i,unsigned id)
{ (void)p;(void)d;(void)i;(void)id;return 0; }
static double now(void)
{
   struct timespec ts;
   if(clock_gettime(CLOCK_MONOTONIC,&ts)) {perror("clock_gettime");exit(2);}
   return ts.tv_sec+ts.tv_nsec*1e-9;
}
static void *read_file(const char *path,size_t *size)
{
   FILE *f=fopen(path,"rb"); long n; void *data;
   if(!f){perror(path);exit(2);}
   if(fseek(f,0,SEEK_END)|| (n=ftell(f))<=0 || n>64*1024*1024) {
      fprintf(stderr,"Invalid input size: %s\n",path);exit(2);
   }
   rewind(f);data=malloc((size_t)n);
   if(!data || fread(data,1,(size_t)n,f)!=(size_t)n) {fprintf(stderr,"Read failed\n");exit(2);}
   fclose(f);*size=(size_t)n;return data;
}
#define LOAD(name) do { \
   void *symbol=dlsym(lib,#name); \
   if(!symbol){fprintf(stderr,"Missing %s: %s\n",#name,dlerror());return 2;} \
   memcpy(&name,&symbol,sizeof(name)); \
} while(0)
int main(int argc,char **argv)
{
   void *lib,*rom;
   size_t rom_size;
   unsigned i,frames=3600;
   double start,elapsed;
   struct retro_game_info game={0};
   struct retro_system_av_info av;
   struct retro_system_info info;
   void (*retro_init)(void),(*retro_deinit)(void),(*retro_run)(void),(*retro_unload_game)(void);
   void (*retro_set_environment)(retro_environment_t);
   void (*retro_set_video_refresh)(retro_video_refresh_t);
   void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
   void (*retro_set_audio_sample)(retro_audio_sample_t);
   void (*retro_set_input_poll)(retro_input_poll_t);
   void (*retro_set_input_state)(retro_input_state_t);
   void (*retro_get_system_av_info)(struct retro_system_av_info*);
   void (*retro_get_system_info)(struct retro_system_info*);
   bool (*retro_load_game)(const struct retro_game_info*);
   bool (*retro_unserialize)(const void*,size_t);
   size_t (*retro_serialize_size)(void);
   if(argc<3){fprintf(stderr,"Usage: %s core.so game.sfc [frames] [raw_state|-] [disabled|fixed] [cap]\n",argv[0]);return 2;}
   if(argc>3){char *end;unsigned long n=strtoul(argv[3],&end,10);if(*end||!n||n>1000000)return 2;frames=(unsigned)n;}
   if(argc>5)skip_mode=argv[5];
   if(strcmp(skip_mode,"disabled")&&strcmp(skip_mode,"fixed"))return 2;
   if(argc>6)skip_cap=argv[6];
   if(strlen(skip_cap)!=1||skip_cap[0]<'0'||skip_cap[0]>'5')return 2;
   lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
   if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
   LOAD(retro_init);LOAD(retro_deinit);LOAD(retro_run);LOAD(retro_unload_game);
   LOAD(retro_set_environment);LOAD(retro_set_video_refresh);
   LOAD(retro_set_audio_sample_batch);LOAD(retro_set_audio_sample);
   LOAD(retro_set_input_poll);LOAD(retro_set_input_state);
   LOAD(retro_get_system_av_info);LOAD(retro_get_system_info);LOAD(retro_load_game);
   LOAD(retro_unserialize);LOAD(retro_serialize_size);
   retro_set_environment(environment);retro_set_video_refresh(video);
   retro_set_audio_sample_batch(audio);retro_set_audio_sample(audio_one);
   retro_set_input_poll(poll_input);retro_set_input_state(input);
   retro_init();retro_get_system_info(&info);
   rom=read_file(argv[2],&rom_size);
   game.path=argv[2];game.data=rom;game.size=rom_size;
   if(!retro_load_game(&game)){fprintf(stderr,"Core rejected ROM\n");return 2;}
   if(argc>4 && strcmp(argv[4],"-")) {
      size_t size;void *state=read_file(argv[4],&size);
      if(size!=retro_serialize_size()||!retro_unserialize(state,size)){
         fprintf(stderr,"State rejected: use an uncompressed state from this build and audio variant\n");return 2;
      }
      free(state);
   }
   retro_get_system_av_info(&av);
   for(i=0;i<120;++i)retro_run();
   rendered=duplicated=0;samples=0;start=now();
   for(i=0;i<frames;++i)retro_run();
   elapsed=now()-start;
   printf("core=%s\nframes=%u\nseconds=%.6f\nemulated_fps=%.3f\n"
          "nominal_fps=%.6f\nspeed_percent=%.2f\nrendered=%u\n"
          "rendered_fps=%.3f\nduplicated=%u\naudio_frames=%llu\n",
          info.library_name,frames,elapsed,frames/elapsed,av.timing.fps,
          100.0*frames/elapsed/av.timing.fps,rendered,rendered/elapsed,
          duplicated,(unsigned long long)samples);
   retro_unload_game();retro_deinit();free(rom);dlclose(lib);
   return 0;
}
