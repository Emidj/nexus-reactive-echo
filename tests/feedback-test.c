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
    struct fixture *reference=last_fixture;
    obs_source_t *f=manual_filter("Echo");
    if(!s||!ref||!f) return 6;
    obs_source_filter_add(s,f);
    set_settings(f,1,.8,1,0,0);
    upload(fixture,512,320,1);
    uint8_t *red=frame(s,512,320);
    check(red[0]==255 && red[3]==255,"first_frame_identity");
    upload(fixture,512,320,0);
    uint8_t *out=frame(s,512,320);
    printf("red_to_black_first_echo=%u expected=204\n",out[0]);
    check(abs((int)out[0]-204)<=1 && out[3]==255,"previous_frame_feedback");
    obs_enter_graphics();
    uint8_t *cached1=render(s,512,320), *cached2=render(s,512,320);
    check(same(cached1,cached2,512*320*4),"multiple_previews_do_not_advance_recursion");
    free(cached1);free(cached2);obs_leave_graphics();
    int previous=out[0]; free(out);
    for(int i=0;i<8;i++) {
        out=frame(s,512,320);
        check(out[0]<previous,"recursive_decay"); previous=out[0]; free(out);
    }
    obs_properties_t *props=obs_source_properties(f);
    check(props && obs_properties_get(props,"reset"),"native_reset_control");
    if(props) obs_property_button_clicked(obs_properties_get(props,"reset"),f);
    out=frame(s,512,320);check(out[0]==0,"reset_erases_history");free(out);
    set_settings(f,1,.8,1,0,8);
    upload(fixture,512,320,2);out=frame(s,512,320);free(out);
    upload(fixture,512,320,0);out=frame(s,512,320);
    size_t shifted=((size_t)160*512+136)*4;
    size_t original=((size_t)160*512+128)*4;
    check(out[shifted]>190 && out[original]==0,"history_translation_8px");free(out);
    set_settings(f,1,.8,1.05,3,0);
    upload(fixture,512,320,2);out=frame(s,512,320);free(out);
    upload(fixture,512,320,0);out=frame(s,512,320);
    double total=0,cx=0,cy=0;
    for(unsigned y=0;y<320;y++) for(unsigned x=0;x<512;x++) {
        double value=out[((size_t)y*512+x)*4];
        total+=value;cx+=(x+.5)*value;cy+=(y+.5)*value;
    }
    double angle=3.0*3.141592653589793/180.0;
    double expected_x=256+1.05*(-127.5*cos(angle)-.5*sin(angle));
    double expected_y=160+1.05*(-127.5*sin(angle)+.5*cos(angle));
    check(total>20 && fabs(cx/total-expected_x)<1.5 && fabs(cy/total-expected_y)<1.5,"zoom_and_rotation_geometry");free(out);
    os_sleep_ms(350);out=render(s,512,320);
    check(out[0]==0 && out[((size_t)148*512+122)*4]==0,"resume_after_gap_clears_history");free(out);
    set_settings(f,1,.8,1.05,3,0);
    upload(fixture,512,320,3); upload(reference,512,320,3);
    out=frame(s,512,320);free(out);out=frame(s,512,320);
    uint8_t *base=render(ref,512,320);
    int alpha=0,spill=0,premul=0;
    for(size_t i=0;i<512*320;i++) {
        if(base[i*4+3]!=out[i*4+3]) alpha++;
        if(!out[i*4+3] && (out[i*4]||out[i*4+1]||out[i*4+2])) spill++;
        for(int c=0;c<3;c++) if(out[i*4+c]>out[i*4+3]) premul++;
    }
    check(alpha==0,"all_256_alpha_levels_preserved");
    check(spill==0,"transparent_pixels_clear");check(premul==0,"premultiplied_RGB_bounded");
    free(out);
    set_settings(f,0,.8,1.05,3,0);out=frame(s,512,320);
    check(same(base,out,512*320*4),"zero_amount_exact_identity");free(out);free(base);
    set_settings(f,1,0,1,0,0);out=frame(s,512,320);base=render(ref,512,320);
    check(same(base,out,512*320*4),"zero_feedback_exact_identity");free(out);free(base);
    set_settings(f,1,.8,1,0,0);upload(fixture,512,320,1);out=frame(s,512,320);free(out);
    obs_source_set_enabled(f,false);os_sleep_ms(70);upload(fixture,512,320,0);
    obs_source_set_enabled(f,true);out=frame(s,512,320);
    check(out[0]==0,"reenable_clears_history");free(out);
    upload(fixture,257,73,3);upload(reference,257,73,3);
    out=frame(s,257,73);base=render(ref,257,73);
    check(same(base,out,257*73*4),"resize_clears_history_and_preserves_RGBA");free(out);free(base);
    upload(fixture,1,1,1);out=frame(s,1,1);
    check(out[0]==255 && out[3]==255,"resize_to_1x1");free(out);
    obs_source_filter_remove(s,f);obs_source_filter_add(s,f);upload(fixture,1,1,0);
    out=frame(s,1,1);check(out[0]==0,"reattach_clears_history");free(out);
    free(red);obs_properties_destroy(props);
    obs_source_filter_remove(s,f);obs_source_release(f);obs_source_release(s);obs_source_release(ref);
    obs_shutdown();printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}

