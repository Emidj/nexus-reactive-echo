/* Pixel comparison between baseline and optimized DLL in separate processes. */
#define main phase1_main
#include "passthrough-test.c"
#undef main
int main(int argc,char **argv)
{
    if(argc!=5) return 2;
    char path[2048],graphics[2048];
    snprintf(path,sizeof(path),"%s/data/libobs/",argv[1]);
    snprintf(graphics,sizeof(graphics),"%s/bin/64bit/libobs-d3d11.dll",argv[1]);
    if(!obs_startup("es-ES",NULL,NULL)) return 3;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    obs_add_data_path(path);
#pragma clang diagnostic pop
    struct obs_video_info vi={.graphics_module=graphics,.fps_num=60,.fps_den=1,
        .base_width=512,.base_height=320,.output_width=512,.output_height=320,
        .output_format=VIDEO_FORMAT_RGBA,.adapter=0,.gpu_conversion=false,
        .colorspace=VIDEO_CS_709,.range=VIDEO_RANGE_FULL,.scale_type=OBS_SCALE_BILINEAR};
    if(obs_reset_video(&vi)!=OBS_VIDEO_SUCCESS) return 4;
    obs_module_t *module=NULL;
    if(obs_open_module(&module,argv[2],"./plugin")!=MODULE_SUCCESS || !obs_init_module(module)) return 5;
    obs_register_source(&fixture_info);
    test_w=512;test_h=320;
    obs_source_t *s=obs_source_create_private("nexus_test_fixture","Echo fixture",NULL);


    obs_source_t *f=manual_filter("Echo");
    if(!s||!f) return 6;
    obs_source_filter_add(s,f);


    for(int mode=0;mode<6;mode++) {
        obs_data_t *settings=obs_source_get_settings(f);
        obs_data_set_double(settings,"amount",mode>=3?.5:1);
        obs_data_set_bool(settings,"silhouette",true);obs_data_set_bool(settings,"trail_color",mode%3!=0);
        obs_data_set_int(settings,"color_mode",mode%3==1?1:0);
        obs_data_set_double(settings,"feedback",.94);obs_data_set_double(settings,"zoom",1.015);
        obs_data_set_double(settings,"rotation",.4);obs_data_set_double(settings,"offset_x",1);
        char lut[2048]="";if(mode%3==2) snprintf(lut,sizeof(lut),"%s/inverse.cube",argv[3]);
        obs_data_set_string(settings,"lut_path",lut);obs_source_update(f,settings);obs_data_release(settings);os_sleep_ms(80);
        uint8_t *out=NULL;
        for(int step=0;step<16;step++) {free(out);os_sleep_ms(40);out=render(s,512,320);}
        char path[2048];snprintf(path,sizeof(path),"%s-%d.rgba",argv[4],mode);
        FILE *file=fopen(path,"wb");if(!file) return 7;fwrite(out,1,512*320*4,file);fclose(file);free(out);
    }
    obs_source_filter_remove(s,f);obs_source_release(f);obs_source_release(s);obs_shutdown();return 0;
}
