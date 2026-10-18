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
    obs_data_t *s=obs_source_get_settings(f);
    obs_data_set_double(s,"amount",amount); obs_data_set_double(s,"feedback",feedback);
    obs_data_set_double(s,"zoom",zoom); obs_data_set_double(s,"rotation",rotation);
    obs_data_set_double(s,"offset_x",x); obs_source_update(f,s); obs_data_release(s);
    os_sleep_ms(70);
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
    struct fixture *reference=last_fixture;
    obs_source_t *f=manual_filter("Echo");
    if(!s||!ref||!f) return 6;
    obs_source_filter_add(s,f);


    upload(fixture,512,320,2);
    for(size_t i=0;i<512*320;i++) fixture->rgba[i*4+3]=fixture->rgba[i*4]?255:0;
    obs_enter_graphics();gs_texture_destroy(fixture->texture);const uint8_t *pixels=fixture->rgba;
    fixture->texture=gs_texture_create(512,320,GS_RGBA,1,&pixels,0);
    memcpy(reference->rgba,fixture->rgba,512*320*4);gs_texture_destroy(reference->texture);pixels=reference->rgba;
    reference->texture=gs_texture_create(512,320,GS_RGBA,1,&pixels,0);obs_leave_graphics();
    obs_source_t *graded=manual_filter("Trail colour");
    obs_source_filter_add(ref,graded);
    const char *files[]={"inverse.cube","inverse.cube","inverse.cube","identity.cube","incomplete.cube","","","inverse.cube"};
    double strengths[]={1,0,.5,1,1,0,0,1};
    double amounts[]={1,1,1,1,1,1,1,.5};
    for(int test=0;test<8;test++) {
        obs_data_t *settings=obs_source_get_settings(f);
        obs_data_set_bool(settings,"silhouette",true);obs_data_set_bool(settings,"trail_color",true);
        obs_data_set_double(settings,"echo_hue_step",0);
        obs_data_set_double(settings,"saturation",1);obs_data_set_double(settings,"exposure",0);
        obs_data_set_string(settings,"lut_path","");obs_data_set_double(settings,"amount",amounts[test]);
        obs_data_set_double(settings,"feedback",.8);obs_data_set_double(settings,"zoom",1);
        obs_data_set_double(settings,"offset_x",8);obs_source_update(f,settings);
        /* Keep the control source's live settings separate from the graded source. */
        obs_data_t *copy=obs_data_create_from_json(obs_data_get_json(settings));
        obs_data_release(settings);settings=copy;
        char path[2048]="";if(*files[test]) snprintf(path,sizeof(path),"%s/%s",argv[3],files[test]);
        obs_data_set_string(settings,"lut_path",path);obs_data_set_double(settings,"lut_strength",strengths[test]);
        if(test==5) obs_data_set_double(settings,"saturation",0);
        if(test==6) obs_data_set_double(settings,"exposure",-1);
        obs_source_update(graded,settings);obs_data_release(settings);os_sleep_ms(70);
        for(int step=0;step<4;step++) {
            uint8_t *normal=frame(s,512,320),*out=frame(ref,512,320);
            int errors=0,alpha=0;size_t foreground=((size_t)160*512+128)*4;
            for(size_t i=0;i<512*320;i++) {
                size_t at=i*4;alpha+=normal[at+3]!=out[at+3];
                if(at==foreground) {errors+=memcmp(normal+at,out+at,4)!=0;continue;}
                float a=normal[at+3]/255.0f;
                float rgb[3],mapped[3];
                for(int c=0;c<3;c++) rgb[c]=a>0?normal[at+c]/255.0f/a:0;
                float luma=.2126f*rgb[0]+.7152f*rgb[1]+.0722f*rgb[2];
                for(int c=0;c<3;c++) {
                    mapped[c]=test==5?luma:test==6?rgb[c]*.5f:rgb[c];
                    if(test==0||test==1||test==2||test==7) mapped[c]=rgb[c]+(1-2*rgb[c])*(float)strengths[test];
                    int expected=(int)lroundf((rgb[c]+(mapped[c]-rgb[c])*(float)amounts[test])*a*255);
                    if(abs(out[at+c]-expected)>2) {
                        if(!errors) printf("case=%d step=%d pixel=%zu channel=%d actual=%u expected=%d normal=%u alpha=%u\n",test,step,i,c,out[at+c],expected,normal[at+c],normal[at+3]);
                        errors++;
                    }
                }
            }
            char label[128];snprintf(label,sizeof(label),"trail_colour_case_%d_step_%d_RGB_and_alpha",test,step);
            check(!errors && !alpha,label);
            if(test==0 && step==3) check(out[foreground]==255 && out[foreground+1]==0,"LUT_does_not_recolour_foreground");
            if(test==3 && step==3) check(same(normal,out,512*320*4),"identity_LUT_exact_trail_identity");
            free(normal);free(out);
        }
    }
    obs_data_t *off=obs_source_get_settings(graded);obs_data_set_bool(off,"trail_color",false);
    obs_source_update(graded,off);obs_data_release(off);
    set_settings(f,.5,.8,1,0,8);
    uint8_t *out=NULL,*control=NULL;
    for(int step=0;step<4;step++) {free(out);free(control);control=frame(s,512,320);out=frame(ref,512,320);}
    check(same(control,out,512*320*4),"disabled_trail_colour_ignores_saved_LUT");free(control);free(out);
    off=obs_source_get_settings(graded);obs_data_set_bool(off,"trail_color",true);
    obs_source_update(graded,off);obs_data_release(off);
    set_settings(graded,0,.8,1,0,8);out=frame(ref,512,320);
    check(same(out,fixture->rgba,512*320*4),"zero_amount_bypass_even_with_trail_LUT");free(out);
    obs_properties_t *p=obs_source_properties(graded);
    check(obs_property_enabled(obs_properties_get(p,"lut_path")),"trail_LUT_control_enabled");obs_properties_destroy(p);
    obs_source_filter_remove(s,f);obs_source_filter_remove(ref,graded);
    obs_source_release(f);obs_source_release(graded);obs_source_release(s);obs_source_release(ref);
    obs_shutdown();printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
