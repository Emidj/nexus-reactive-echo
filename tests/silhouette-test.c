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
static bool same(const uint8_t *a,const uint8_t *b,size_t n) { return memcmp(a,b,n)==0; }
static void set_settings(obs_source_t *f, double amount, double feedback, double zoom, double rotation, double x)
{
    clear_history(f); /* Independent test case, not a production update. */
    obs_data_t *s=obs_source_get_settings(f);
    obs_data_set_double(s,"amount",amount); obs_data_set_double(s,"feedback",feedback);
    obs_data_set_double(s,"zoom",zoom); obs_data_set_double(s,"rotation",rotation);
    obs_data_set_double(s,"offset_x",x); obs_source_update(f,s); obs_data_release(s);
    os_sleep_ms(70);
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
    obs_data_set_bool(settings,"silhouette",true);
    obs_data_set_double(settings,"echo_hue_step",-30);
    obs_source_update(f,settings);obs_data_release(settings);
    set_settings(f,1,.8,1,0,8);
    /* Isolated opaque red pixel on genuinely transparent input, as after chroma. */
    upload(fixture,512,320,2);
    for(size_t i=0;i<512*320;i++) fixture->rgba[i*4+3]=fixture->rgba[i*4]?255:0;
    obs_enter_graphics();gs_texture_destroy(fixture->texture);const uint8_t *pixels=fixture->rgba;
    fixture->texture=gs_texture_create(512,320,GS_RGBA,1,&pixels,0);obs_leave_graphics();
    size_t original=((size_t)160*512+128)*4, shifted=original+8*4;
    uint8_t *out=frame(s,512,320);
    check(out[original]==255 && out[original+1]==0 && out[original+2]==0 && out[original+3]==255,"opaque_foreground_unchanged");
    check(out[shifted+3]==0,"initial_history_empty");free(out);
    out=frame(s,512,320);
    check(abs((int)out[shifted+3]-204)<=1,"trail_outside_current_alpha");
    check(out[shifted]>180 && out[shifted+1]<100,"trail_is_coloured_not_camera_RGB");
    check(out[original]==255 && out[original+1]==0,"foreground_stays_sharp_with_history");free(out);
    out=frame(s,512,320);
    check(abs((int)out[shifted+32+3]-163)<=2,"second_recursive_step_decay");
    check(out[shifted+32+2]>out[shifted+2],"successive_trails_change_hue");
    int spill=0,premul=0;
    for(size_t i=0;i<512*320;i++) {
        for(int c=0;c<3;c++) if(out[i*4+c]>out[i*4+3]) premul++;
        if(!out[i*4+3] && (out[i*4]||out[i*4+1]||out[i*4+2])) spill++;
    }
    check(!spill,"zero_alpha_has_no_hidden_RGB");check(!premul,"trail_RGB_premultiplied");free(out);
    obs_enter_graphics();uint8_t *one=render(s,512,320),*two=render(s,512,320);
    check(same(one,two,512*320*4),"multiple_views_share_same_frame");free(one);free(two);obs_leave_graphics();
    obs_properties_t *props=obs_source_properties(f);
    settings=obs_source_get_settings(f);obs_properties_apply_settings(props,settings);obs_data_release(settings);
    check(!obs_property_enabled(obs_properties_get(props,"lut_path")) &&
          obs_property_enabled(obs_properties_get(props,"echo_hue_step")),"mode_controls_match_active_processing");
    obs_property_button_clicked(obs_properties_get(props,"reset"),f);
    out=frame(s,512,320);check(out[shifted+3]==0,"reset_removes_external_trail");free(out);
    set_settings(f,0,.8,1,0,8);out=frame(s,512,320);
    check(out[original]==255 && out[shifted+3]==0,"zero_amount_removes_trails");free(out);
    set_settings(f,1,0,1,0,8);out=frame(s,512,320);
    check(out[original]==255 && out[shifted+3]==0,"zero_feedback_bypass");free(out);
    set_settings(f,1,.8,1,0,8);out=frame(s,512,320);free(out);out=frame(s,512,320);free(out);
    /* Transparent input releases the prior silhouette rather than cutting it off. */
    memset(fixture->rgba,0,512*320*4);
    obs_enter_graphics();gs_texture_destroy(fixture->texture);pixels=fixture->rgba;
    fixture->texture=gs_texture_create(512,320,GS_RGBA,1,&pixels,0);obs_leave_graphics();
    out=frame(s,512,320);check(out[shifted+3]>150,"removed_foreground_leaves_decaying_trail");free(out);
    for(int i=0;i<40;i++) {out=frame(s,512,320);free(out);}
    out=frame(s,512,320);int remaining=0;for(size_t i=0;i<512*320*4;i++) remaining+=out[i];
    check(remaining==0,"empty_input_history_eventually_clears");free(out);
    /* Real partial-alpha fixture and enlarged coloured bands for visual inspection. */
    obs_source_filter_remove(s,f);obs_source_release(s);
    s=obs_source_create_private("nexus_test_fixture","Chroma-like silhouette",NULL);fixture=last_fixture;
    obs_source_filter_add(s,f);set_settings(f,1,.94,1.065,0,0);
    for(int i=0;i<18;i++) {
        out=frame(s,512,320);
        if(i==0 || i==17) {
            FILE *fp=fopen(i==0?"build/silhouette-first.rgba":"build/silhouette-bands.rgba","wb");
            if(!fp) return 9;
            fwrite(out,1,512*320*4,fp);fclose(fp);
        }
        free(out);
    }
    out=frame(s,512,320);uint8_t *base=render(ref,512,320);int mismatch=0;
    for(size_t i=0;i<512*320;i++) if(base[i*4+3]==255 && memcmp(base+i*4,out+i*4,4)) mismatch++;
    check(mismatch==0,"all_opaque_foreground_pixels_exact");free(out);
    set_settings(f,0,.94,1.065,0,0);out=frame(s,512,320);
    check(same(base,out,512*320*4),"zero_amount_all_partial_alpha_pixels_exact");free(out);
    set_settings(f,1,0,1.065,0,0);out=frame(s,512,320);
    check(same(base,out,512*320*4),"zero_feedback_all_partial_alpha_pixels_exact");free(out);free(base);
    set_settings(f,1,.94,1.065,0,0);
    upload(fixture,257,73,3);out=frame(s,257,73);
    check(out!=NULL,"silhouette_resize_recreates_capture");free(out);
    obs_properties_destroy(props);
    obs_source_filter_remove(s,f);obs_source_release(f);obs_source_release(s);obs_source_release(ref);
    obs_shutdown();printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
