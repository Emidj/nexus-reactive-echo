#define main phase1_main
#include "passthrough-test.c"
#undef main
static int failures;
static const char *fixture_dir;
static void check(bool ok,const char *name) { printf("%s: %s\n",name,ok?"PASS":"FAIL");failures+=!ok; }
static float clamp(float x) { return fminf(1,fmaxf(0,x)); }
static bool copy_fixture(const char *from,const char *to)
{
    char a[2048],b[2048];snprintf(a,sizeof(a),"%s/%s",fixture_dir,from);snprintf(b,sizeof(b),"%s/%s",fixture_dir,to);
    FILE *input=os_fopen(a,"rb"), *output=os_fopen(b,"wb");
    if(!input || !output) {if(input) fclose(input);if(output) fclose(output);return false;}
    char bytes[4096];size_t count;bool ok=true;
    while((count=fread(bytes,1,sizeof(bytes),input))) if(fwrite(bytes,1,count,output)!=count) {ok=false;break;}
    ok=ok && !ferror(input);fclose(input);if(fclose(output)) ok=false;return ok;
}
static void configure(obs_source_t *f,float amount,float feedback,float exposure,float contrast,float saturation,float hue,const char *file,float strength)
{
    obs_data_t *d=obs_source_get_settings(f);char path[2048]="";
    if(*file) snprintf(path,sizeof(path),"%s/%s",fixture_dir,file);
    obs_data_set_double(d,"amount",amount);obs_data_set_double(d,"feedback",feedback);
    obs_data_set_double(d,"zoom",1);obs_data_set_double(d,"rotation",0);
    obs_data_set_double(d,"exposure",exposure);obs_data_set_double(d,"contrast",contrast);
    obs_data_set_double(d,"saturation",saturation);obs_data_set_double(d,"hue",hue);
    obs_data_set_string(d,"lut_path",path);obs_data_set_double(d,"lut_strength",strength);
    obs_source_update(f,d);obs_data_release(d);os_sleep_ms(80);
}
static void verify(obs_source_t *s,const uint8_t *base,float amount,float exposure,float contrast,float saturation,float hue,int lut,float strength,const char *label)
{
    os_sleep_ms(40);uint8_t *out=render(s,512,320);
    int max_error=0,alpha=0,spill=0;
    for(size_t i=0;i<512*320;i++) {
        float a=base[i*4+3]/255.0f;
        float rgb[3],graded[3];
        for(int c=0;c<3;c++) { rgb[c]=a>0?base[i*4+c]/255.0f/a:0;graded[c]=(rgb[c]*exp2f(exposure)-.5f)*contrast+.5f; }
        float luma=.2126f*graded[0]+.7152f*graded[1]+.0722f*graded[2];
        for(int c=0;c<3;c++) graded[c]=luma+(graded[c]-luma)*saturation;
        if(hue) {
            float yy=.299f*graded[0]+.587f*graded[1]+.114f*graded[2];
            float ii=.595716f*graded[0]-.274453f*graded[1]-.321263f*graded[2];
            float qq=.211456f*graded[0]-.522591f*graded[1]+.311135f*graded[2];
            float angle=hue*3.14159265358979323846f/180;
            float ir=ii*cosf(angle)-qq*sinf(angle),qr=ii*sinf(angle)+qq*cosf(angle);
            graded[0]=yy+.9563f*ir+.6210f*qr;graded[1]=yy-.2721f*ir-.6474f*qr;graded[2]=yy-1.1070f*ir+1.7046f*qr;
        }
        for(int c=0;c<3;c++) graded[c]=clamp(graded[c]);
        float mapped[3];for(int c=0;c<3;c++) mapped[c]=graded[c];
        if(lut==1) for(int c=0;c<3;c++) mapped[c]=1-graded[c];
        if(lut==2) {mapped[0]=graded[2];mapped[1]=graded[0];mapped[2]=graded[1];}
        if(lut==3) for(int c=0;c<3;c++) mapped[c]=graded[c]<=.5f?graded[c]*.5f:.25f+(graded[c]-.5f)*1.5f;
        if(lut==4) for(int c=0;c<3;c++) mapped[c]=clamp((graded[c]-(.1f+.1f*c))/(.8f-.2f*c));
        for(int c=0;c<3;c++) {
            float v=graded[c]+(mapped[c]-graded[c])*strength;
            int expected=(int)lroundf((rgb[c]+(v-rgb[c])*amount)*a*255);
            int error=abs((int)out[i*4+c]-expected);if(error>max_error) max_error=error;
        }
        alpha+=out[i*4+3]!=base[i*4+3];
        if(!out[i*4+3] && (out[i*4]||out[i*4+1]||out[i*4+2])) spill++;
    }
    printf("%s max_RGB_error=%d alpha_errors=%d spill=%d\n",label,max_error,alpha,spill);
    check(max_error<=2 && !alpha && !spill,label);free(out);
}
int main(int argc,char **argv)
{
    if(argc!=4) return 2;fixture_dir=argv[3];
    char data[2048],graphics[2048];
    snprintf(data,sizeof(data),"%s/data/libobs/",argv[1]);
    snprintf(graphics,sizeof(graphics),"%s/bin/64bit/libobs-d3d11.dll",argv[1]);
    if(!obs_startup("es-ES",NULL,NULL)) return 3;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    obs_add_data_path(data);
#pragma clang diagnostic pop
    struct obs_video_info vi={.graphics_module=graphics,.fps_num=60,.fps_den=1,
        .base_width=512,.base_height=320,.output_width=512,.output_height=320,.output_format=VIDEO_FORMAT_RGBA,
        .adapter=0,.gpu_conversion=false,.colorspace=VIDEO_CS_709,.range=VIDEO_RANGE_FULL,.scale_type=OBS_SCALE_BILINEAR};
    if(obs_reset_video(&vi)!=OBS_VIDEO_SUCCESS) return 4;
    obs_module_t *module=NULL;
    if(obs_open_module(&module,argv[2],"./plugin")!=MODULE_SUCCESS || !obs_init_module(module)) return 5;
    obs_register_source(&fixture_info);
    obs_source_t *s=obs_source_create_private("nexus_test_fixture","Color fixture",NULL);
    obs_source_t *f=manual_filter("Color");
    if(!s || !f) return 6;
    uint8_t *base=render(s,512,320);obs_source_filter_add(s,f);
    configure(f,1,0,0,1,1,0,"identity.cube",1);verify(s,base,1,0,1,1,0,0,1,"identity_LUT_2");
    configure(f,1,0,0,1,1,0,"identity17.cube",1);verify(s,base,1,0,1,1,0,0,1,"identity_LUT_17");
    configure(f,1,0,0,1,1,0,"identity65.cube",1);verify(s,base,1,0,1,1,0,0,1,"identity_LUT_65");
    configure(f,1,0,0,1,1,0,"identidad áé.cube",1);verify(s,base,1,0,1,1,0,0,1,"UTF8_path_and_BOM");
    configure(f,1,0,0,1,1,0,"inverse.cube",1);verify(s,base,1,0,1,1,0,1,1,"inverse_LUT");
    configure(f,1,0,0,1,1,0,"swap.cube",1);verify(s,base,1,0,1,1,0,2,1,"red_fastest_channel_order");
    configure(f,1,0,0,1,1,0,"square.cube",1);verify(s,base,1,0,1,1,0,3,1,"trilinear_interpolation");
    configure(f,1,0,0,1,1,0,"domain.cube",1);verify(s,base,1,0,1,1,0,4,1,"DOMAIN_MIN_MAX");
    configure(f,1,0,0,1,1,0,"inverse.cube",.35f);verify(s,base,1,0,1,1,0,1,.35f,"LUT_partial_strength");
    configure(f,.5f,0,0,1,1,0,"inverse.cube",1);verify(s,base,.5f,0,1,1,0,1,1,"master_partial_strength");
    configure(f,1,0,.5f,1,1,0,"",0);verify(s,base,1,.5f,1,1,0,0,0,"exposure");
    configure(f,1,0,0,1.3f,1,0,"",0);verify(s,base,1,0,1.3f,1,0,0,0,"contrast");
    configure(f,1,0,0,1,0,0,"",0);verify(s,base,1,0,1,0,0,0,0,"monochrome_saturation");
    configure(f,1,0,0,1,1,70,"",0);verify(s,base,1,0,1,1,70,0,0,"hue_rotation");
    configure(f,1,0,.3f,1.15f,.7f,30,"swap.cube",.6f);verify(s,base,1,.3f,1.15f,.7f,30,2,.6f,"controls_and_LUT_combined");
    configure(f,0,.8f,2,1.5f,0,90,"inverse.cube",1);
    uint8_t *out=render(s,512,320);check(!memcmp(base,out,512*320*4),"master_zero_exact_bypass");free(out);
    configure(f,1,0,0,1,1,0,"inverse.cube",0);
    out=render(s,512,320);check(!memcmp(base,out,512*320*4),"LUT_zero_exact_bypass");free(out);
    configure(f,1,0,0,1,1,0,"incomplete.cube",1);
    out=render(s,512,320);check(!memcmp(base,out,512*320*4),"invalid_LUT_disables_previous_LUT");free(out);
    obs_properties_t *props=obs_source_properties(f);
    check(strstr(obs_property_description(obs_properties_get(props,"lut_status")),"incompleta")!=NULL,"invalid_LUT_visible_status");
    obs_properties_destroy(props);
    configure(f,1,0,0,1,1,0,"missing.cube",1);out=render(s,512,320);
    check(!memcmp(base,out,512*320*4),"missing_LUT_safe_bypass");free(out);
    configure(f,1,0,0,1,1,0,"",1);out=render(s,512,320);
    check(!memcmp(base,out,512*320*4),"clearing_LUT_path_restores_identity");free(out);
    check(copy_fixture("identity.cube","reload.cube"),"prepare_reload_fixture");
    configure(f,1,0,0,1,1,0,"reload.cube",1);
    verify(s,base,1,0,1,1,0,0,1,"reload_initial_identity");
    check(copy_fixture("inverse.cube","reload.cube"),"replace_LUT_at_same_path");
    props=obs_source_properties(f);
    obs_property_button_clicked(obs_properties_get(props,"lut_reload"),f);
    os_sleep_ms(80);verify(s,base,1,0,1,1,0,1,1,"reload_button_updates_GPU_texture");
    obs_properties_destroy(props);check(copy_fixture("identity.cube","reload.cube"),"restore_reload_fixture");
    obs_source_t *second=obs_source_create_private("nexus_test_fixture","Independent fixture",NULL);
    obs_source_t *second_filter=manual_filter("Independent LUT");
    if(!second || !second_filter) return 7;
    obs_source_filter_add(second,second_filter);
    configure(f,1,0,0,1,1,0,"inverse.cube",1);
    configure(second_filter,1,0,0,1,1,0,"swap.cube",1);
    verify(s,base,1,0,1,1,0,1,1,"first_instance_keeps_inverse_LUT");
    obs_data_t *toggle=obs_source_get_settings(f);obs_data_set_bool(toggle,"lut_enabled",false);obs_source_update(f,toggle);
    os_sleep_ms(80);verify(s,base,1,0,1,1,0,0,1,"LUT_checkbox_disables_mapping");
    check(strstr(obs_data_get_string(toggle,"lut_path"),"inverse.cube")!=NULL,"LUT_checkbox_preserves_path");
    obs_data_set_bool(toggle,"lut_enabled",true);obs_source_update(f,toggle);obs_data_release(toggle);
    os_sleep_ms(80);verify(s,base,1,0,1,1,0,1,1,"LUT_checkbox_reenables_mapping");

    verify(second,base,1,0,1,1,0,2,1,"second_instance_has_own_LUT");
    obs_source_filter_remove(second,second_filter);obs_source_release(second_filter);obs_source_release(second);
    configure(f,1,.8f,.5f,1,1,0,"",0);
    for(int i=0;i<6;i++) verify(s,base,1,.5f,1,1,0,0,0,"color_not_accumulated_in_feedback");
    free(base);obs_source_filter_remove(s,f);obs_source_release(f);obs_source_release(s);obs_shutdown();
    printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
