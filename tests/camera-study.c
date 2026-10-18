/* Visual parameter study using the actual DLL; not a likeness acceptance test. */
#define main phase1_main
#include "passthrough-test.c"
#undef main
static const char *camera_audio_name(void *p){UNUSED_PARAMETER(p);return "Camera recording audio";}
static void *camera_audio_create(obs_data_t *s,obs_source_t *o){UNUSED_PARAMETER(s);UNUSED_PARAMETER(o);return bzalloc(1);}
static struct obs_source_info camera_audio_info={.id="camera_test_audio",.type=OBS_SOURCE_TYPE_INPUT,.output_flags=OBS_SOURCE_AUDIO,
    .get_name=camera_audio_name,.create=camera_audio_create,.destroy=bfree};
int main(int argc,char **argv)
{
    if(argc!=5 && argc!=6 && argc!=9) return 2;
    bool bands_study=argc==6 && !strcmp(argv[5],"bands");
    bool reference_study=argc==9 && !strcmp(argv[5],"reference");
    char path[2048],graphics[2048];
    snprintf(path,sizeof(path),"%s/data/libobs/",argv[1]);
    snprintf(graphics,sizeof(graphics),"%s/bin/64bit/libobs-d3d11.dll",argv[1]);
    if(!obs_startup("es-ES",NULL,NULL)) return 3;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    obs_add_data_path(path);
#pragma clang diagnostic pop
    struct obs_video_info vi={.graphics_module=graphics,.fps_num=60,.fps_den=1,
        .base_width=512,.base_height=288,.output_width=512,.output_height=288,
        .output_format=VIDEO_FORMAT_RGBA,.adapter=0,.gpu_conversion=false,
        .colorspace=VIDEO_CS_709,.range=VIDEO_RANGE_FULL,.scale_type=OBS_SCALE_BILINEAR};
    if(obs_reset_video(&vi)!=OBS_VIDEO_SUCCESS) return 4;
    obs_module_t *module=NULL;
    if(obs_open_module(&module,argv[2],"./plugin")!=MODULE_SUCCESS || !obs_init_module(module)) return 5;
    char filter_path[2048],filter_data[2048];
    snprintf(filter_path,sizeof(filter_path),"%s/obs-plugins/64bit/obs-filters.dll",argv[1]);
    snprintf(filter_data,sizeof(filter_data),"%s/data/obs-plugins/obs-filters",argv[1]);
    obs_module_t *filters=NULL;
    if(obs_open_module(&filters,filter_path,filter_data)!=MODULE_SUCCESS || !obs_init_module(filters)) return 7;
    struct obs_audio_info ai={.samples_per_sec=48000,.speakers=SPEAKERS_MONO};
    if(!obs_reset_audio(&ai))return 13;
    obs_register_source(&camera_audio_info);
    obs_source_t *camera_audio=obs_source_create("camera_test_audio","Camera audio",NULL,NULL);
    if(!camera_audio)return 14;
    obs_register_source(&fixture_info);
    test_w=512;test_h=288;
    input_rgba=NULL;
    obs_source_t *s=obs_source_create_private("nexus_test_fixture","Echo fixture",NULL);


    obs_source_t *f=manual_filter("Echo");
    if(!s||!f) return 6;
    obs_data_t *ks=obs_data_create();
    obs_data_set_string(ks,"key_color_type","custom");
    obs_data_set_int(ks,"key_color",0xff808000);
    obs_data_set_int(ks,"similarity",120);
    obs_data_set_int(ks,"smoothness",80);
    obs_data_set_int(ks,"spill",80);
    if(reference_study){
        obs_data_set_string(ks,"key_color_type","green");obs_data_set_int(ks,"similarity",480);
        obs_data_set_int(ks,"smoothness",1);obs_data_set_int(ks,"spill",21);
        obs_data_set_double(ks,"contrast",.94);obs_data_set_double(ks,"brightness",-.05);
    }
    obs_source_t *key=obs_source_create_private("chroma_key_filter","Camera chroma",ks);
    obs_data_release(ks); if(!key) return 8;
    obs_source_filter_add(s,key);obs_source_filter_add(s,f);


    for(int mode=0;mode<3;mode++) {
        obs_data_t *settings=obs_source_get_settings(f);
        obs_data_set_bool(settings,"audio_enabled",false);obs_source_update(f,settings);clear_history(f);
        obs_data_set_double(settings,"amount",!bands_study && !reference_study && mode==0?0:1);
        obs_data_set_bool(settings,"silhouette",true);obs_data_set_bool(settings,"trail_color",true);
        obs_data_set_int(settings,"color_mode",1);
        obs_data_set_bool(settings,"reference_mode",reference_study && mode>0);
        if(reference_study && mode>0){
            obs_data_set_string(settings,"base_lut_path",argv[7]);
            obs_data_set_string(settings,"reactive_lut_path",argv[8]);
            obs_data_set_int(settings,"color_mode",mode==1?1:0);
            obs_data_set_bool(settings,"audio_enabled",true);
            obs_data_set_string(settings,"audio_source",obs_source_get_uuid(camera_audio));
            obs_data_set_double(settings,"audio_attack",30);obs_data_set_double(settings,"audio_release",150);
        }
        obs_data_set_int(settings,"gradient_bands",reference_study?0:bands_study?(mode==0?0:mode==1?3:6):0);
        const double persistence[]={.94,.94,.94}, expansion[]={1.015,1.015,1.04};
        obs_data_set_double(settings,"feedback",reference_study && mode>0?.8:persistence[mode]);obs_data_set_double(settings,"zoom",reference_study && mode>0?1.14:bands_study || reference_study?1.04:expansion[mode]);
        obs_data_set_double(settings,"rotation",0);obs_data_set_double(settings,"offset_x",reference_study && mode>0?-1.6:0);
        obs_data_set_double(settings,"offset_y",reference_study && mode>0?5.6:0);
        char lut[2048]="";
        obs_data_set_string(settings,"lut_path",lut);obs_source_update(f,settings);obs_data_release(settings);os_sleep_ms(80);
        FILE *input=fopen(argv[3],"rb"); if(!input) return 9;
        char path[2048];snprintf(path,sizeof(path),"%s-%d.rgba",argv[4],mode);
        FILE *file=fopen(path,"wb"); if(!file) return 10;
        FILE *sound=reference_study?fopen(argv[6],"rb"):NULL;
        if(reference_study && !sound)return 15;
        for(int step=0;step<120;step++) {
            if(sound){
                float samples[800];if(fread(samples,sizeof(float),800,sound)!=800)return 16;
                struct obs_source_audio block={.data={(const uint8_t *)samples},.frames=800,.speakers=SPEAKERS_MONO,
                    .format=AUDIO_FORMAT_FLOAT_PLANAR,.samples_per_sec=48000,.timestamp=os_gettime_ns()};
                obs_source_output_audio(camera_audio,&block);
            }
            if(fread(last_fixture->rgba,1,512*288*4,input)!=512*288*4) return 11;
            obs_enter_graphics();
            gs_texture_destroy(last_fixture->texture);
            const uint8_t *pixels=last_fixture->rgba;
            last_fixture->texture=gs_texture_create(512,288,GS_RGBA,1,&pixels,0);
            obs_leave_graphics();
            os_sleep_ms(18);uint8_t *out=render(s,512,288);
            if(fwrite(out,1,512*288*4,file)!=512*288*4) return 12;
            free(out);
        }
        if(sound)fclose(sound);fclose(input);fclose(file);
    }
    obs_source_filter_remove(s,f);obs_source_filter_remove(s,key);obs_source_release(key);obs_source_release(f);obs_source_release(s);obs_source_release(camera_audio);obs_shutdown();return 0;
}
