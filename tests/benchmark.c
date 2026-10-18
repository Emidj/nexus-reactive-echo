/* GPU timestamps around the actual source/filter draw; no readback in timings. */
#define main phase1_main
#include "passthrough-test.c"
#undef main
#include <windows.h>
#include <psapi.h>
static void wait_frame(uint64_t *last)
{
    uint64_t now=obs_get_video_frame_time();while(now==*last) {os_sleep_ms(1);now=obs_get_video_frame_time();}*last=now;
}
static int compare_double(const void *a,const void *b) {double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y);}
static void configure(obs_source_t *f,int mode,const char *lutdir)
{
    obs_data_t *s=obs_source_get_settings(f);
    obs_data_set_double(s,"amount",mode==1?0:1);obs_data_set_double(s,"feedback",.94);
    obs_data_set_double(s,"zoom",1.015);obs_data_set_double(s,"rotation",.4);
    obs_data_set_bool(s,"silhouette",mode>=3);obs_data_set_bool(s,"trail_color",mode>=4);
    obs_data_set_int(s,"color_mode",mode==4?1:0);
    if(mode==5 || mode==6) {char path[2048];snprintf(path,sizeof(path),"%s/identity65.cube",lutdir);
        obs_data_set_string(s,"lut_path",path);obs_data_set_double(s,"lut_strength",1);}
    if(mode==6) obs_data_set_bool(s,"silhouette",false);
    obs_source_update(f,s);obs_data_release(s);
}
static void begin_target(gs_texrender_t *rt,unsigned w,unsigned h)
{
    gs_texrender_reset(rt);if(!gs_texrender_begin(rt,w,h)) exit(10);
    struct vec4 zero;vec4_zero(&zero);gs_clear(GS_CLEAR_COLOR,&zero,0,0);gs_ortho(0,(float)w,0,(float)h,-100,100);
    gs_enable_blending(true);gs_enable_depth_test(false);gs_set_cull_mode(GS_NEITHER);
    gs_set_linear_srgb(false);gs_enable_framebuffer_srgb(false);
    gs_blend_function_separate(GS_BLEND_SRCALPHA,GS_BLEND_INVSRCALPHA,GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
}
int main(int argc,char **argv)
{
    if(argc!=5 && argc!=6) return 2;
    setvbuf(stdout,NULL,_IONBF,0);
    char data[2048],graphics[2048];snprintf(data,sizeof(data),"%s/data/libobs/",argv[1]);
    snprintf(graphics,sizeof(graphics),"%s/bin/64bit/libobs-d3d11.dll",argv[1]);
    if(!obs_startup("es-ES",NULL,NULL)) return 3;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    long before_path=bnum_allocs();obs_add_data_path(data);
    long path_registry_allocations=bnum_allocs()-before_path;
#pragma clang diagnostic pop
    struct obs_video_info vi={.graphics_module=graphics,.fps_num=60,.fps_den=1,.base_width=1920,.base_height=1080,
        .output_width=1920,.output_height=1080,.output_format=VIDEO_FORMAT_RGBA,.adapter=0,.gpu_conversion=false,
        .colorspace=VIDEO_CS_709,.range=VIDEO_RANGE_FULL,.scale_type=OBS_SCALE_BILINEAR};
    if(obs_reset_video(&vi)!=OBS_VIDEO_SUCCESS) return 4;
    obs_module_t *module=NULL;if(obs_open_module(&module,argv[2],"./plugin")!=MODULE_SUCCESS || !obs_init_module(module)) return 5;
    obs_register_source(&fixture_info);
    FILE *csv=fopen(argv[4],"wb");if(!csv) return 6;
    fprintf(csv,"repeat,width,height,mode,samples,gpu_median_ms,gpu_p95_ms,cpu_submit_median_ms,cpu_submit_p95_ms,invalid_queries\n");
    const char *modes[]={"source","bypass","feedback","silhouette","gradient","silhouette_lut65","feedback_lut65"};
    const unsigned widths[]={1280,1920,3840},heights[]={720,1080,2160};
    int failures=0;unsigned soak=argc==6?(unsigned)strtoul(argv[5],NULL,10):0;
    unsigned repeats=soak?1:2,resolutions=soak?1:3;
    for(unsigned repeat=0;repeat<repeats;repeat++) for(unsigned res=0;res<resolutions;res++) {
        test_w=soak?1920:widths[res];test_h=soak?1080:heights[res];
        obs_source_t *source=obs_source_create_private("nexus_test_fixture","Benchmark input",NULL);
        for(int mode=soak?4:0;mode<(soak?5:7);mode++) {
            obs_source_t *filter=NULL;
            if(mode) {filter=manual_filter("Benchmark filter");if(!filter) return 7;
                configure(filter,mode,argv[3]);obs_source_filter_add(source,filter);}
            obs_enter_graphics();gs_texrender_t *rt=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
            gs_timer_t *timer=gs_timer_create();gs_timer_range_t *range=gs_timer_range_create();
            if(!rt||!timer||!range) return 8;obs_leave_graphics();
            uint64_t last=0,start=os_gettime_ns();long settled_allocs=0;SIZE_T first_private=0;
            double gpu[120],cpu[120];int count=0,invalid=0;
            unsigned frames=soak?soak*60+30:150;
            for(unsigned frame=0;frame<frames;frame++) {
                wait_frame(&last);obs_enter_graphics();gs_blend_state_push();begin_target(rt,test_w,test_h);
                gs_timer_range_begin(range);gs_timer_begin(timer);uint64_t before=os_gettime_ns();
                obs_source_video_render(source);uint64_t after=os_gettime_ns();gs_timer_end(timer);gs_timer_range_end(range);
                gs_texrender_end(rt);gs_blend_state_pop();gs_flush();
                uint64_t ticks=0,frequency=0;bool disjoint=false;
                bool ok=gs_timer_get_data(timer,&ticks) && gs_timer_range_get_data(range,&disjoint,&frequency) && !disjoint && frequency;
                obs_leave_graphics();
                if(frame>=30 && !soak) {if(ok && count<120) {gpu[count]=ticks*1000.0/frequency;cpu[count]=(after-before)/1e6;count++;}else invalid++;}
                if(soak && frame==30) {
                    settled_allocs=bnum_allocs();PROCESS_MEMORY_COUNTERS_EX mem={0};mem.cb=sizeof(mem);
                    if(!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&mem,sizeof(mem))) return 11;
                    first_private=mem.PrivateUsage;
                    printf("SOAK start allocations=%ld private_MiB=%.3f\n",settled_allocs,first_private/1048576.0);
                }
                if(soak && frame>30 && frame%1800==0) {
                    PROCESS_MEMORY_COUNTERS_EX mem={0};mem.cb=sizeof(mem);
                    if(!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&mem,sizeof(mem))) return 11;
                    printf("SOAK frame=%u elapsed=%.1f allocations=%ld delta=%ld private_MiB=%.3f delta_MiB=%.3f\n",
                        frame,(os_gettime_ns()-start)/1e9,bnum_allocs(),bnum_allocs()-settled_allocs,
                        mem.PrivateUsage/1048576.0,((double)mem.PrivateUsage-first_private)/1048576.0);
                    if(bnum_allocs()>settled_allocs+10) failures++;
                }
                if(soak && !ok) {invalid++;failures++;}
            }
            if(!soak) {
                if(count!=120) failures++;
                qsort(gpu,count,sizeof(double),compare_double);qsort(cpu,count,sizeof(double),compare_double);
                if(count) {fprintf(csv,"%u,%u,%u,%s,%d,%.6f,%.6f,%.6f,%.6f,%d\n",repeat,test_w,test_h,modes[mode],count,
                    gpu[count/2],gpu[(count-1)*95/100],cpu[count/2],cpu[(count-1)*95/100],invalid);fflush(csv);
                    printf("BENCH %u %ux%u %s GPU median=%.4f p95=%.4f CPU=%.4f ms\n",repeat,test_w,test_h,modes[mode],gpu[count/2],gpu[(count-1)*95/100],cpu[count/2]);}
            }
            if(soak) printf("SOAK completed frames=%u elapsed=%.3f allocation_delta=%ld invalid_queries=%d\n",
                frames,(os_gettime_ns()-start)/1e9,bnum_allocs()-settled_allocs,invalid);
            obs_enter_graphics();gs_timer_destroy(timer);gs_timer_range_destroy(range);gs_texrender_destroy(rt);obs_leave_graphics();
            if(filter) {obs_source_filter_remove(source,filter);obs_source_release(filter);}
        }
        obs_source_release(source);
    }
    fclose(csv);obs_shutdown();long live=bnum_allocs();
    printf("LIVE_ALLOCS_AFTER_SHUTDOWN=%ld PATH_REGISTRY_ALLOCS=%ld\n",live,path_registry_allocations);
    if(live!=path_registry_allocations) failures++;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    obs_remove_data_path(data); /* libobs keeps the registry's backing array until process exit. */
#pragma clang diagnostic pop
    printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
