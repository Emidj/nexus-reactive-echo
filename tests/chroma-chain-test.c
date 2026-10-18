/* Integration with the official OBS chroma filter, no replacement keyer. */
#define main phase1_main
#include "passthrough-test.c"
#undef main
static int failures;
static void check(bool ok,const char *label) {printf("%s: %s\n",label,ok?"PASS":"FAIL");failures+=!ok;}
static uint8_t *frame(obs_source_t *source) {os_sleep_ms(45);return render(source,512,320);}
int main(int argc,char **argv)
{
    if(argc!=4) return 2;
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
    char filter_path[2048],filter_data[2048];
    snprintf(filter_path,sizeof(filter_path),"%s/obs-plugins/64bit/obs-filters.dll",argv[1]);
    snprintf(filter_data,sizeof(filter_data),"%s/data/obs-plugins/obs-filters",argv[1]);
    obs_module_t *filters=NULL;
    if(obs_open_module(&filters,filter_path,filter_data)!=MODULE_SUCCESS || !obs_init_module(filters)) return 7;
    input_rgba=argv[3];
    obs_register_source(&fixture_info);
    test_w=512;test_h=320;
    obs_source_t *s=obs_source_create_private("nexus_test_fixture","Echo fixture",NULL);

    obs_source_t *ref=obs_source_create_private("nexus_test_fixture","Reference",NULL);
    obs_source_t *f=manual_filter("Echo");
    if(!s||!ref||!f) return 6;



    obs_source_t *key=obs_source_create_private("chroma_key_filter","Chroma before NEXUS",NULL);
    obs_source_t *key_ref=obs_source_create_private("chroma_key_filter","Reference chroma",NULL);
    if(!key||!key_ref) return 8;
    obs_source_filter_add(s,key);obs_source_filter_add(s,f);obs_source_filter_add(ref,key_ref);
    check(obs_filter_get_target(f)==key,"NEXUS_receives_output_of_actual_chroma_filter");
    uint8_t *base=frame(ref),*out=frame(s);
    check(!memcmp(base,out,512*320*4),"zero_amount_exact_identity_after_chroma");free(out);
    unsigned transparent=0,partial=0,opaque=0;
    for(size_t i=0;i<512*320;i++) {unsigned a=base[i*4+3];transparent+=a==0;partial+=a>0&&a<255;opaque+=a==255;}
    printf("chroma alpha: transparent=%u partial=%u opaque=%u\n",transparent,partial,opaque);
    check(transparent>10000 && opaque>1000,"OBS_chroma_removes_green_and_keeps_subject");
    check(partial>0,"OBS_chroma_produces_partial_alpha_edges");
    obs_data_t *settings=obs_source_get_settings(f);
    obs_data_set_bool(settings,"silhouette",true);obs_data_set_double(settings,"amount",1);
    obs_data_set_double(settings,"feedback",.94);obs_data_set_double(settings,"zoom",1.03);
    obs_data_set_double(settings,"echo_hue_step",-5);obs_source_update(f,settings);obs_data_release(settings);
    for(int step=0;step<12;step++) {out=frame(s);if(step<11) free(out);}
    unsigned changed_foreground=0,external=0,spill=0,premul=0;
    for(size_t i=0;i<512*320;i++) {
        size_t at=i*4;
        changed_foreground+=base[at+3]==255 && memcmp(base+at,out+at,4)!=0;
        external+=base[at+3]==0 && out[at+3]>0;
        spill+=out[at+3]==0 && (out[at]||out[at+1]||out[at+2]);
        for(int c=0;c<3;c++) premul+=out[at+c]>out[at+3];
    }
    printf("composite: changed_foreground=%u external=%u spill=%u premul=%u\n",changed_foreground,external,spill,premul);
    check(!changed_foreground,"actual_chroma_foreground_opaque_pixels_exact");
    check(external>1000,"sillhouettes_extend_into_chroma_removed_background");
    check(!spill,"actual_chroma_zero_alpha_no_RGB_residue");check(!premul,"actual_chroma_composite_RGB_premultiplied");
    FILE *fp=fopen("build/chroma-echo.rgba","wb");if(!fp) return 9;
    fwrite(out,1,512*320*4,fp);fclose(fp);free(out);
    settings=obs_source_get_settings(f);obs_data_set_double(settings,"amount",0);obs_source_update(f,settings);obs_data_release(settings);
    out=frame(s);check(!memcmp(base,out,512*320*4),"chroma_soft_edges_exact_when_strength_zero");free(out);
    obs_source_set_enabled(key,false);out=frame(s);
    check(out[3]==255 && out[1]>240,"disabling_chroma_restores_green_input");free(out);
    obs_source_set_enabled(key,true);out=frame(s);
    check(!memcmp(base,out,512*320*4),"reenabling_chroma_restores_exact_keyed_input");free(out);
    obs_source_filter_remove(s,f);obs_source_filter_remove(s,key);obs_source_filter_remove(ref,key_ref);
    obs_source_release(f);obs_source_release(key);obs_source_release(key_ref);obs_source_release(s);obs_source_release(ref);
    free(base);obs_shutdown();printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
