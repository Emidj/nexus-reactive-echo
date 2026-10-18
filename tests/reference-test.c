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
    obs_register_source(&fixture_info);
    test_w=512;test_h=320;
    obs_source_t *s=obs_source_create_private("nexus_test_fixture","Echo fixture",NULL);
    struct fixture *fixture=last_fixture;
    obs_source_t *ref=obs_source_create_private("nexus_test_fixture","Reference",NULL);
    obs_source_t *f=manual_filter("Echo");
    if(!s||!ref||!f) return 6;
    obs_source_filter_add(s,f);

    obs_data_t *settings=obs_source_get_settings(f);
    obs_data_set_bool(settings,"audio_enabled",false);obs_data_set_bool(settings,"silhouette",true);
    obs_data_set_bool(settings,"reference_mode",true);obs_data_set_bool(settings,"trail_color",true);
    obs_data_set_int(settings,"color_mode",1);obs_data_set_double(settings,"amount",1);
    obs_data_set_double(settings,"reference_level",.2);obs_data_set_double(settings,"feedback",.8);
    obs_data_set_double(settings,"zoom",1);obs_data_set_double(settings,"offset_x",30);
    obs_source_update(f,settings);
    upload(fixture,512,320,2);
    for(size_t i=0;i<512*320;i++)fixture->rgba[i*4+3]=fixture->rgba[i*4]?255:0;
    obs_enter_graphics();gs_texture_destroy(fixture->texture);const uint8_t *pixels=fixture->rgba;
    fixture->texture=gs_texture_create(512,320,GS_RGBA,1,&pixels,0);obs_leave_graphics();
    uint8_t *out=frame(s,512,320);size_t center=((size_t)160*512+128)*4;
    check(!memcmp(out,fixture->rgba,512*320*4),"reference_first_frame_keeps_original_without_base_LUT");free(out);
    out=frame(s,512,320);size_t inner=center+8*4,outer=center+24*4;
    check(abs((int)out[inner+3]-78)<=2,"reference_opacity_and_feedback_match_copy_alpha");
    check(out[inner]>out[inner+2],"reference_injects_current_warm_colour");free(out);
    obs_data_set_double(settings,"reference_level",0);obs_source_update(f,settings);
    out=frame(s,512,320);free(out);out=frame(s,512,320);
    check(out[inner+2]>out[inner] && out[outer]>out[outer+2],"history_keeps_old_colour_when_new_colour_changes");
    check(out[center]==255 && out[center+3]==255,"reference_foreground_is_separate_from_colour_history");
    unsigned spill=0,premul=0;
    for(size_t i=0;i<512*320;i++){
        spill+=!out[i*4+3] && (out[i*4]||out[i*4+1]||out[i*4+2]);
        for(int c=0;c<3;c++)premul+=out[i*4+c]>out[i*4+3];
    }
    check(!spill && !premul,"reference_RGBA_is_premultiplied_and_transparent_RGB_zero");free(out);
    obs_data_set_string(settings,"base_lut_path",argv[3]);obs_source_update(f,settings);
    out=frame(s,512,320);
    check(out[center]<2 && out[center+1]>253 && out[center+2]>253,"base_LUT_grades_foreground_separately");free(out);
    obs_data_set_bool(settings,"base_lut_enabled",false);obs_source_update(f,settings);out=frame(s,512,320);
    check(out[center]==255 && out[center+1]==0,"base_LUT_toggle_disables_without_losing_path");free(out);
    check(!strcmp(obs_data_get_string(settings,"base_lut_path"),argv[3]),"base_LUT_toggle_preserves_path");
    obs_data_set_bool(settings,"base_lut_enabled",true);obs_source_update(f,settings);out=frame(s,512,320);
    check(out[center]<2 && out[center+1]>253,"base_LUT_toggle_reenables");free(out);
    obs_data_set_string(settings,"base_lut_path","");obs_source_update(f,settings);
    out=frame(s,512,320);check(out[center]==255 && out[center+1]==0,"clearing_base_LUT_restores_original_foreground");free(out);
    obs_data_set_int(settings,"color_mode",0);obs_data_set_string(settings,"reactive_lut_path",argv[3]);
    obs_data_set_double(settings,"reference_level",.25);obs_source_update(f,settings);
    out=frame(s,512,320);free(out);out=frame(s,512,320);
    check(out[inner+1]>out[inner+2],"reactive_LUT_is_applied_before_recursion");free(out);
    obs_data_set_bool(settings,"reactive_lut_enabled",false);obs_source_update(f,settings);
    out=frame(s,512,320);free(out);out=frame(s,512,320);
    check(out[inner+2]>out[inner+1],"reactive_LUT_toggle_disables_without_clearing_path");free(out);
    obs_data_set_bool(settings,"reactive_lut_enabled",true);
    obs_data_set_string(settings,"reactive_lut_path","missing-reference-LUT.cube");obs_source_update(f,settings);
    out=frame(s,512,320);free(out);out=frame(s,512,320);
    check(out[inner+2]>out[inner+1],"invalid_reactive_LUT_does_not_leave_previous_texture_active");free(out);
    obs_data_set_double(settings,"amount",0);obs_source_update(f,settings);out=frame(s,512,320);
    check(!memcmp(out,fixture->rgba,512*320*4),"reference_zero_amount_exact_bypass");free(out);
    for(int mode=3;mode<=3;mode++){
        obs_data_set_double(settings,"amount",1);obs_data_set_int(settings,"recursion_mode",mode);
        obs_data_set_double(settings,"zoom",1.14);obs_data_set_double(settings,"mode_spread",55);
        obs_source_update(f,settings);
        obs_properties_t *props=obs_source_properties(f);
        obs_property_button_clicked(obs_properties_get(props,"reset"),f);obs_properties_destroy(props);
        for(int k=0;k<12;k++){out=frame(s,512,320);if(k<11)free(out);}
        unsigned external=0,bad=0;
        for(size_t i=0;i<512*320;i++){
            external+=out[i*4+3]>0 && fixture->rgba[i*4+3]==0;
            for(int c=0;c<3;c++)bad+=out[i*4+c]>out[i*4+3];
        }
        check(external>0 && bad==0,"new_mode_external_trails_and_premultiplied_alpha");
        check(out[center]==255 && out[center+3]==255,"new_mode_preserves_foreground");free(out);
        obs_data_set_double(settings,"amount",0);obs_source_update(f,settings);out=frame(s,512,320);
        check(!memcmp(out,fixture->rgba,512*320*4),"new_mode_zero_exact_bypass");free(out);
    }
    obs_data_set_int(settings,"recursion_mode",4);obs_source_update(f,settings);out=frame(s,512,320);free(out);
    obs_properties_t *migration_props=obs_source_properties(f);
    check(obs_property_list_item_count(obs_properties_get(migration_props,"recursion_mode"))==2,"only_approved_and_zoom_in_mode_selector");
    obs_properties_destroy(migration_props);
    check(obs_data_get_int(settings,"recursion_mode")==0,"saved_grid_falls_back_to_approved_mode");
    for(int removed=1;removed<=2;removed++){
        obs_data_set_int(settings,"recursion_mode",removed);obs_source_update(f,settings);out=frame(s,512,320);free(out);
        check(obs_data_get_int(settings,"recursion_mode")==0,"removed_radial_spiral_fall_back_to_approved_mode");
    }
    obs_data_release(settings);obs_source_filter_remove(s,f);obs_source_release(f);obs_source_release(s);obs_source_release(ref);
    obs_shutdown();printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}

