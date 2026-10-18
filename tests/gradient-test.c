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

    const unsigned char palettes[13][3][3]={
        {{255,184,51},{232,72,85},{84,32,112}},
        {{0,239,255},{212,32,255},{109,20,64}},
        {{255,234,128},{255,128,0},{120,22,12}},
        {{214,237,255},{48,168,232},{22,42,102}},
        {{255,0,0},{0,255,0},{0,0,255}},
        {{255,60,172},{120,75,160},{43,134,197}},
        {{208,255,250},{0,184,217},{19,42,114}},
        {{246,255,0},{65,230,110},{67,16,160}},
        {{255,240,120},{255,54,0},{81,0,24}},
        {{255,241,184},{255,122,198},{113,61,206}},
        {{108,255,176},{37,188,210},{99,44,230}},
        {{255,255,255},{146,146,146},{22,22,22}},
        {{255,228,155},{192,132,50},{0,61,73}}
    };
    int max_error=0;
    for(int test=0;test<19;test++) {
        int palette=test>=11?test-6:test<5?test:4;
        float middle=test==5?.05f:test==6?.95f:.5f;
        float amount=test==7?.5f:1;
        int bands=test>=8 && test<=10?(test==8?3:test==9?6:12):0;
        obs_data_t *settings=obs_source_get_settings(f);
        obs_data_set_bool(settings,"silhouette",true);obs_data_set_bool(settings,"trail_color",false);
        obs_data_set_double(settings,"amount",amount);obs_data_set_double(settings,"feedback",.8);
        obs_data_set_double(settings,"zoom",1);obs_data_set_double(settings,"offset_x",8);
        obs_source_update(f,settings);
        obs_data_t *copy=obs_data_create_from_json(obs_data_get_json(settings));obs_data_release(settings);settings=copy;
        obs_data_set_int(settings,"gradient_bands",bands);
        obs_data_set_bool(settings,"trail_color",true);obs_data_set_int(settings,"color_mode",1);
        obs_data_set_int(settings,"gradient_palette",palette);obs_data_set_double(settings,"gradient_middle",middle);
        obs_data_set_int(settings,"gradient_near",0x000000FF);obs_data_set_int(settings,"gradient_mid",0x0000FF00);
        obs_data_set_int(settings,"gradient_far",0x00FF0000);
        char lut[2048];snprintf(lut,sizeof(lut),"%s/inverse.cube",argv[3]);obs_data_set_string(settings,"lut_path",lut);
        obs_data_set_double(settings,"exposure",4);obs_data_set_double(settings,"saturation",0);
        obs_source_update(graded,settings);obs_data_release(settings);os_sleep_ms(70);
        for(int step=0;step<6;step++) {
            uint8_t *normal=frame(s,512,320),*out=frame(ref,512,320);
            int errors=0,alpha=0;
            for(size_t i=0;i<512*320;i++) {
                size_t at=i*4;alpha+=normal[at+3]!=out[at+3];
                if(i==(size_t)160*512+128) {errors+=memcmp(normal+at,out+at,4)!=0;continue;}
                float a=normal[at+3]/255.0f;
                float t=1-fminf(1,a/amount);
                if(bands>1) t=fminf(1,floorf(t*bands)/(bands-1));
                float blend=t<middle?t/middle:(t-middle)/(1-middle);
                int left=t<middle?0:1;
                for(int c=0;c<3;c++) {
                    float mapped=palettes[palette][left][c]*(1-blend)+palettes[palette][left+1][c]*blend;
                    int expected=(int)lroundf(normal[at+c]*(1-amount)+mapped*a*amount);
                    int error=abs(out[at+c]-expected);if(error>max_error) max_error=error;
                    if(error>3) errors++;
                }
            }
            char label[128];snprintf(label,sizeof(label),"gradient_case_%d_step_%d_foreground_RGB_alpha",test,step);
            check(!errors && !alpha,label);free(normal);free(out);
        }
    }
    obs_data_t *custom=obs_source_get_settings(graded);obs_data_set_int(custom,"gradient_palette",4);obs_source_update(graded,custom);obs_data_release(custom);
    obs_properties_t *p=obs_source_properties(graded);
    obs_data_t *settings=obs_source_get_settings(graded);obs_properties_apply_settings(p,settings);
    check(obs_property_visible(obs_properties_get(p,"gradient_controls")) &&
          !obs_property_visible(obs_properties_get(p,"lut_controls")),"gradient_hides_LUT_controls");
    check(obs_property_visible(obs_properties_get(p,"gradient_near")),"custom_gradient_exposes_colour_buttons");
    check(strstr(obs_property_description(obs_properties_get(p,"gradient_preview")),"bgcolor=\"#FF0000\"")!=NULL,
          "gradient_preview_contains_start_colour");
    char *saved_lut=bstrdup(obs_data_get_string(settings,"lut_path"));
    obs_data_set_int(settings,"color_mode",0);obs_source_update(graded,settings);obs_properties_apply_settings(p,settings);
    check(obs_property_visible(obs_properties_get(p,"lut_controls")) &&
          !obs_property_visible(obs_properties_get(p,"gradient_controls")),"LUT_selection_hides_gradient_controls");
    check(!strcmp(saved_lut,obs_data_get_string(settings,"lut_path")),"mode_switch_keeps_saved_LUT");bfree(saved_lut);
    obs_data_set_int(settings,"color_mode",1);obs_data_set_double(settings,"amount",0);obs_source_update(graded,settings);
    obs_data_release(settings);uint8_t *out=frame(ref,512,320);
    check(same(out,fixture->rgba,512*320*4),"gradient_zero_amount_exact_RGBA_identity");free(out);
    obs_data_t *saved=obs_save_source(graded);obs_source_t *restored=obs_load_source(saved);obs_data_release(saved);
    check(restored!=NULL,"gradient_filter_saved_and_loaded");
    settings=obs_source_get_settings(restored);
    check(obs_data_get_int(settings,"color_mode")==1 && obs_data_get_int(settings,"gradient_palette")==4 &&
          obs_data_get_int(settings,"gradient_far")==0x00FF0000,"gradient_settings_survive_save_load");
    obs_data_release(settings);obs_source_release(restored);obs_properties_destroy(p);
    obs_source_filter_remove(s,f);obs_source_filter_remove(ref,graded);
    obs_source_release(f);obs_source_release(graded);obs_source_release(s);obs_source_release(ref);
    printf("Maximum RGB error: %d/255\n",max_error);obs_shutdown();
    printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
