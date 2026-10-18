/* Integration tests of the real DLL using the same OBS/D3D fixture as phase 1. */
#define main phase1_main
#include "passthrough-test.c"
#undef main
static int failures;
static void check(bool ok, const char *label)
{
    printf("%s: %s\n",label,ok?"PASS":"FAIL"); if(!ok) failures++;
}
static void upload(struct fixture *f, uint32_t w, uint32_t h, int mode)
{
    obs_enter_graphics();
    f->w=w; f->h=h; bfree(f->rgba); f->rgba=bzalloc((size_t)w*h*4);
    for(uint32_t y=0;y<h;y++) for(uint32_t x=0;x<w;x++) {
        size_t i=((size_t)y*w+x)*4;
        f->rgba[i]=mode==1?255:mode==2 && x==w/4 && y==h/2?255:0;
        f->rgba[i+1]=mode==3?160:0; f->rgba[i+2]=mode==3?80:0;
        f->rgba[i+3]=mode==3?(uint8_t)(x%256):255;
    }
    gs_texture_destroy(f->texture); const uint8_t *pixels=f->rgba;
    f->texture=gs_texture_create(w,h,GS_RGBA,1,&pixels,0);
    obs_leave_graphics();
}
static uint8_t *frame(obs_source_t *s,uint32_t w,uint32_t h)
{
    os_sleep_ms(45); return render(s,w,h);
}
int main(int argc,char **argv)
{
    if(argc!=3) return 2;
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
    struct fixture *fixture=last_fixture;
    obs_source_t *ref=obs_source_create_private("nexus_test_fixture","Reference",NULL);
    obs_source_t *f=manual_filter("Echo");
    if(!s||!ref||!f) return 6;
    obs_source_filter_add(s,f);

    obs_data_t *settings=obs_source_get_settings(f);
    obs_data_set_bool(settings,"audio_enabled",false);
    obs_data_set_bool(settings,"silhouette",true);obs_data_set_bool(settings,"trail_color",true);
    obs_data_set_int(settings,"color_mode",1);obs_data_set_double(settings,"amount",1);
    obs_data_set_double(settings,"feedback",.94);obs_data_set_double(settings,"zoom",1);
    obs_data_set_double(settings,"offset_x",8);obs_source_update(f,settings);
    upload(fixture,512,320,2);
    for(size_t i=0;i<512*320;i++)fixture->rgba[i*4+3]=fixture->rgba[i*4]?255:0;
    obs_enter_graphics();gs_texture_destroy(fixture->texture);const uint8_t *pixels=fixture->rgba;
    fixture->texture=gs_texture_create(512,320,GS_RGBA,1,&pixels,0);obs_leave_graphics();
    for(int i=0;i<8;i++){uint8_t *out=frame(s,512,320);free(out);}
    const char *keys[]={"amount","feedback","zoom","rotation","offset_x","offset_y",
        "echo_hue_step","audio_zoom","exposure","contrast","saturation","hue","lut_strength",
        "gradient_middle","audio_depth","audio_gain","audio_threshold","audio_attack","audio_release"};
    double values[]={.8,.92,1.005,.2,9,1,-10,.03,.1,1.1,.9,2,.7,.4,.5,1,-40,40,300};
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);i++){
        obs_data_set_double(settings,keys[i],values[i]);obs_source_update(f,settings);
        uint8_t *out=frame(s,512,320);unsigned external=0;
        for(size_t j=0;j<512*320;j++)external+=!fixture->rgba[j*4+3] && out[j*4+3]>0;
        char label[150];snprintf(label,sizeof(label),"slider_%s_keeps_existing_history",keys[i]);check(external>0,label);
        size_t at=((size_t)160*512+128)*4;
        check(out[at]==255 && out[at+3]==255,"slider_keeps_opaque_foreground");free(out);
    }
    const char *integers[]={"gradient_bands","gradient_palette","preset_selection"};
    const int numbers[]={3,1,2};
    for(int i=0;i<3;i++){
        obs_data_set_int(settings,integers[i],numbers[i]);obs_source_update(f,settings);
        uint8_t *out=frame(s,512,320);unsigned external=0;
        for(size_t j=0;j<512*320;j++)external+=!fixture->rgba[j*4+3] && out[j*4+3]>0;
        char label[150];snprintf(label,sizeof(label),"selector_%s_keeps_history",integers[i]);check(external>0,label);free(out);
    }
    obs_properties_t *props=obs_source_properties(f);
    obs_property_button_clicked(obs_properties_get(props,"reset"),f);obs_properties_destroy(props);
    uint8_t *out=frame(s,512,320);check(!memcmp(out,fixture->rgba,512*320*4),"explicit_clear_still_resets_history");free(out);
    obs_data_release(settings);obs_source_filter_remove(s,f);obs_source_release(f);obs_source_release(s);obs_source_release(ref);
    obs_shutdown();printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
