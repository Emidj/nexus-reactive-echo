/* Real OBS audio callbacks + real DLL + D3D11. No microphone or sound device needed. */
#define main phase1_main
#include "passthrough-test.c"
#undef main
#include <windows.h>
#include <stdatomic.h>
static int failures;
static double grey,red;
static size_t probe=((size_t)130*512+256)*4;
static atomic_int observed_peak;
static void check(bool ok,const char *label) {printf("%s: %s\n",label,ok?"PASS":"FAIL");failures+=!ok;}
static const char *audio_name(void *data) {UNUSED_PARAMETER(data);return "NEXUS synthetic audio";}
static void *audio_create(obs_data_t *s,obs_source_t *source) {UNUSED_PARAMETER(s);UNUSED_PARAMETER(source);return bzalloc(1);}
static struct obs_source_info audio_info={.id="nexus_test_audio",.type=OBS_SOURCE_TYPE_INPUT,.output_flags=OBS_SOURCE_AUDIO,
    .get_name=audio_name,.create=audio_create,.destroy=bfree};
static void emit(obs_source_t *source,float left,float right)
{
    float channels[2][1024];
    for(int i=0;i<1024;i++) {float wave=sinf(2*3.14159265358979323846f*440*i/48000);channels[0][i]=left*wave;channels[1][i]=right*wave;}
    struct obs_source_audio audio={.data={(const uint8_t *)channels[0],(const uint8_t *)channels[1]},
        .frames=1024,.speakers=SPEAKERS_STEREO,.format=AUDIO_FORMAT_FLOAT_PLANAR,.samples_per_sec=48000,.timestamp=os_gettime_ns()};
    obs_source_output_audio(source,&audio);
}
static void pump(obs_source_t *a,float amplitude,obs_source_t *b,float other,int blocks)
{
    for(int i=0;i<blocks;i++) {if(a) emit(a,amplitude,amplitude);if(b) emit(b,other,other);os_sleep_ms(22);}
}
static void capture(void *data,obs_source_t *source,const struct audio_data *audio,bool muted)
{
    UNUSED_PARAMETER(data);UNUSED_PARAMETER(source);UNUSED_PARAMETER(muted);float peak=0;
    const float *samples=(const float *)audio->data[0];
    if(samples) for(unsigned i=0;i<audio->frames;i++) peak=fmaxf(peak,fabsf(samples[i]));
    atomic_store(&observed_peak,(int)lroundf(peak*1000000));
}
static float strength(obs_source_t *s)
{
    uint8_t *out=render(s,512,320);float v=(float)((out[probe]-red)/(grey-red));free(out);return v;
}
static void expect_strength(obs_source_t *s,float expected,const char *label)
{
    float value=strength(s);printf("%s measured=%.3f expected=%.3f\n",label,value,expected);
    check(fabsf(value-expected)<.065f,label);
}
static void settings(obs_source_t *f,bool enabled,const char *uuid,float depth,float gain,float threshold)
{
    obs_data_t *s=obs_source_get_settings(f);
    obs_data_set_double(s,"amount",1);obs_data_set_double(s,"feedback",0);obs_data_set_double(s,"saturation",0);
    obs_data_set_bool(s,"audio_enabled",enabled);obs_data_set_string(s,"audio_source",uuid);
    obs_data_set_double(s,"audio_depth",depth);obs_data_set_double(s,"audio_gain",gain);
    obs_data_set_double(s,"audio_threshold",threshold);obs_data_set_double(s,"audio_attack",10);obs_data_set_double(s,"audio_release",40);
    obs_source_update(f,s);obs_data_release(s);os_sleep_ms(70);
}
static bool listed(obs_source_t *f,const char *uuid,const char *label)
{
    obs_properties_t *p=obs_source_properties(f);obs_property_t *list=obs_properties_get(p,"audio_source");bool found=false;
    for(size_t i=0;i<obs_property_list_item_count(list);i++) if(!strcmp(uuid,obs_property_list_item_string(list,i)) &&
        (!label || !strcmp(label,obs_property_list_item_name(list,i)))) found=true;
    obs_properties_destroy(p);return found;
}
/* Measure transformed silhouette pixels, not a private implementation field. */
static double expansion_probe(obs_source_t *source,obs_source_t *filter,obs_source_t *audio,
                              const char *uuid,float amplitude,bool enabled,float depth,float extra)
{
    obs_data_t *s=obs_source_get_settings(filter);
    obs_data_set_bool(s,"audio_enabled",false);obs_source_update(filter,s);
    obs_data_set_bool(s,"silhouette",true);obs_data_set_double(s,"amount",1);
    obs_data_set_double(s,"feedback",.9);obs_data_set_double(s,"zoom",1.02);
    obs_data_set_double(s,"rotation",0);obs_data_set_double(s,"offset_x",0);
    obs_data_set_double(s,"audio_zoom",extra);obs_data_set_double(s,"audio_depth",depth);
    obs_data_set_double(s,"audio_attack",10);obs_data_set_double(s,"audio_release",40);
    obs_data_set_double(s,"audio_threshold",-48);obs_data_set_double(s,"audio_gain",0);
    obs_data_set_string(s,"audio_source",uuid);obs_data_set_bool(s,"audio_enabled",enabled);
    obs_source_update(filter,s);obs_data_release(s);
    pump(audio,amplitude,NULL,0,22);
    uint8_t *out=render(source,512,320);free(out);
    pump(audio,amplitude,NULL,0,2);out=render(source,512,320);
    double sum=0,cx=0;
    for(unsigned y=0;y<320;y++) for(unsigned x=0;x<512;x++) {
        if(x==128 && y==160) continue;
        double a=out[((size_t)y*512+x)*4+3];sum+=a;cx+=(x+.5)*a;
    }
    size_t center=((size_t)160*512+128)*4;
    check(out[center]==255 && out[center+1]==0 && out[center+2]==0 && out[center+3]==255,
          "reactive_expansion_keeps_foreground_exact");
    free(out);return sum>0?cx/sum:0;
}
static void expansion_tests(obs_source_t *audio,const char *uuid)
{
    obs_source_t *source=obs_source_create_private("nexus_test_fixture","Expansion fixture",NULL);
    struct fixture *fixture=last_fixture;
    memset(fixture->rgba,0,512*320*4);size_t center=((size_t)160*512+128)*4;
    fixture->rgba[center]=255;fixture->rgba[center+3]=255;
    obs_enter_graphics();gs_texture_destroy(fixture->texture);const uint8_t *pixels=fixture->rgba;
    fixture->texture=gs_texture_create(512,320,GS_RGBA,1,&pixels,0);obs_leave_graphics();
    obs_source_t *filter=obs_source_create_private("nexus_reactive_echo","Expansion",NULL);
    obs_source_filter_add(source,filter);
    double manual=expansion_probe(source,filter,audio,uuid,.2511886f,false,1,.08f);
    double quiet=expansion_probe(source,filter,audio,uuid,.2511886f,true,1,.08f);
    double loud=expansion_probe(source,filter,audio,uuid,1,true,1,.08f);
    printf("expansion centroids: manual=%.3f minus12dB=%.3f full=%.3f\n",manual,quiet,loud);
    check(fabs(manual-(256-127.5*1.02))<.7,"disabled_audio_uses_manual_zoom");
    check(fabs(quiet-(256-127.5*1.08))<.7,"minus12dB_expands_by_smoothed_level");
    check(fabs(loud-(256-127.5*1.10))<.7 && loud<quiet,"louder_audio_expands_further");
    double zero=expansion_probe(source,filter,audio,uuid,1,true,1,0);
    check(fabs(zero-manual)<.7,"zero_expansion_preserves_previous_geometry");
    double depth=expansion_probe(source,filter,audio,uuid,1,true,0,.08f);
    check(depth==0,"zero_depth_has_no_external_trail");
    obs_source_set_muted(audio,true);
    double mute=expansion_probe(source,filter,audio,uuid,1,true,1,.08f);
    check(mute==0,"muted_source_has_no_external_trail");obs_source_set_muted(audio,false);
    double partial_silent=expansion_probe(source,filter,audio,uuid,0,true,.5f,.08f);
    check(partial_silent==0,"partial_depth_silence_has_no_silhouettes");
    double silent=expansion_probe(source,filter,audio,uuid,0,true,1,.08f);
    check(silent==0,"silent_source_has_no_external_trail");
    expansion_probe(source,filter,audio,uuid,1,true,1,.08f);
    obs_source_set_muted(audio,true);pump(audio,1,NULL,0,35);
    uint8_t *out=render(source,512,320);
    check(!memcmp(out,fixture->rgba,512*320*4),"mute_after_expansion_reaches_exact_bypass");free(out);
    obs_source_set_muted(audio,false);
    expansion_probe(source,filter,audio,uuid,1,true,1,.08f);
    os_sleep_ms(900);out=render(source,512,320);
    check(!memcmp(out,fixture->rgba,512*320*4),"stale_audio_releases_expansion_and_opacity");free(out);
    obs_source_filter_remove(source,filter);obs_source_release(filter);obs_source_release(source);
}
static double reference_colour_probe(obs_source_t *source,obs_source_t *audio,float amplitude)
{
    uint8_t *out=NULL;
    for(int i=0;i<28;i++){emit(audio,amplitude,amplitude);os_sleep_ms(22);free(out);out=render(source,512,320);}
    double r=0,b=0;unsigned external=0;
    for(size_t i=0;i<512*320;i++)if(i!=(size_t)160*512+128){r+=out[i*4];b+=out[i*4+2];external+=out[i*4+3]>0;}
    check(external>0,"reference_audio_creates_external_silhouettes");
    free(out);return r/fmax(1,b);
}
static void reference_audio_tests(obs_source_t *audio,const char *uuid)
{
    obs_source_t *source=obs_source_create_private("nexus_test_fixture","Reference audio fixture",NULL);
    struct fixture *fixture=last_fixture;memset(fixture->rgba,0,512*320*4);
    size_t center=((size_t)160*512+128)*4;fixture->rgba[center]=255;fixture->rgba[center+3]=255;
    obs_enter_graphics();gs_texture_destroy(fixture->texture);const uint8_t *pixels=fixture->rgba;
    fixture->texture=gs_texture_create(512,320,GS_RGBA,1,&pixels,0);obs_leave_graphics();
    obs_source_t *filter=obs_source_create_private("nexus_reactive_echo","Reference audio",NULL);
    obs_source_filter_add(source,filter);obs_data_t *settings=obs_source_get_settings(filter);
    obs_data_set_bool(settings,"reference_mode",true);obs_data_set_bool(settings,"silhouette",true);
    obs_data_set_bool(settings,"trail_color",true);obs_data_set_int(settings,"color_mode",1);
    obs_data_set_double(settings,"amount",1);obs_data_set_double(settings,"feedback",.8);
    obs_data_set_double(settings,"zoom",1.14);obs_data_set_string(settings,"audio_source",uuid);
    obs_data_set_double(settings,"audio_depth",.5);obs_data_set_double(settings,"audio_attack",10);obs_data_set_double(settings,"audio_release",40);
    obs_source_update(filter,settings);
    double quiet=reference_colour_probe(source,audio,.04f),loud=reference_colour_probe(source,audio,.4f);
    printf("reference colour R/B quiet=%.3f loud=%.3f\n",quiet,loud);
    check(loud>quiet+1,"audio_magnitude_changes_new_silhouette_colour");
    pump(audio,0,NULL,0,30);uint8_t *out=render(source,512,320);
    check(!memcmp(out,fixture->rgba,512*320*4),"reference_partial_depth_silence_exact_identity");free(out);
    reference_colour_probe(source,audio,.4f);obs_source_set_muted(audio,true);pump(audio,.4f,NULL,0,30);out=render(source,512,320);
    check(!memcmp(out,fixture->rgba,512*320*4),"reference_mute_removes_all_silhouettes");free(out);obs_source_set_muted(audio,false);
    const char *targets[]={"target_lut","target_scale","target_offset_x","target_offset_y","target_feedback","target_color"};
    for(int i=2;i<4;i++){char key[64];snprintf(key,sizeof(key),"%s_amount",targets[i]);obs_data_set_double(settings,key,80);}
    obs_data_set_int(settings,"color_mode",0);obs_data_set_string(settings,"reactive_lut_path","build/lut-fixtures/inverse.cube");
    uint64_t baseline=0;
    for(int trial=0;trial<9;trial++){
        for(int j=0;j<6;j++)obs_data_set_bool(settings,targets[j],trial==j+1 || trial>=7);
        obs_data_set_int(settings,"recursion_mode",trial>7?3:0);
        obs_source_update(filter,settings);
        obs_properties_t *props=obs_source_properties(filter);obs_property_button_clicked(obs_properties_get(props,"reset"),filter);obs_properties_destroy(props);
        for(int k=0;k<24;k++){emit(audio,.1f,.1f);os_sleep_ms(22);out=render(source,512,320);if(k<23)free(out);}
        uint64_t hash=1469598103934665603ULL;unsigned bad=0;
        for(size_t i=0;i<512*320;i++)for(int c=0;c<4;c++){hash=(hash^out[i*4+c])*1099511628211ULL;if(c<3)bad+=out[i*4+c]>out[i*4+3];}
        if(trial==0)baseline=hash;else check(hash!=baseline,"audio_target_changes_render_individually_or_combined");
        check(!bad && out[center]==255 && out[center+3]==255,"audio_targets_preserve_alpha_and_foreground");free(out);
    }
    pump(audio,0,NULL,0,40);out=render(source,512,320);
    check(!memcmp(out,fixture->rgba,512*320*4),"combined_audio_targets_silence_identity");free(out);
    const double offsets[7][2]={{80,0},{-80,0},{0,80},{0,-80},{0,0},{80,-80},{80,80}};
    for(int trial=0;trial<7;trial++){
        for(int j=0;j<6;j++)obs_data_set_bool(settings,targets[j],false);
        obs_data_set_bool(settings,"target_offset_x",trial!=6);obs_data_set_bool(settings,"target_offset_y",trial!=6);
        obs_data_set_double(settings,"target_offset_x_amount",offsets[trial][0]);obs_data_set_double(settings,"target_offset_y_amount",offsets[trial][1]);
        obs_data_set_int(settings,"recursion_mode",0);obs_data_set_double(settings,"zoom",1);
        obs_data_set_double(settings,"offset_x",0);obs_data_set_double(settings,"offset_y",0);
        obs_data_set_double(settings,"audio_depth",1);obs_source_update(filter,settings);pump(audio,1,NULL,0,24);
        obs_properties_t *props=obs_source_properties(filter);obs_property_button_clicked(obs_properties_get(props,"reset"),filter);obs_properties_destroy(props);
        out=render(source,512,320);free(out);pump(audio,1,NULL,0,2);out=render(source,512,320);
        double mass=0,cx=0,cy=0;
        for(unsigned y=0;y<320;y++)for(unsigned x=0;x<512;x++)if(x!=128 || y!=160){double a=out[((size_t)y*512+x)*4+3];mass+=a;cx+=x*a;cy+=y*a;}
        if(trial==4 || trial==6)check(mass==0,"zero_or_disabled_axis_offsets_do_not_displace_trails");
        else check(mass>0 && fabs(cx/mass-(128+offsets[trial][0]))<1 && fabs(cy/mass-(160+offsets[trial][1]))<1,"independent_axes_follow_positive_negative_and_diagonal_offsets");
        check(out[center]==255 && out[center+3]==255,"independent_axes_preserve_foreground");free(out);
    }
    obs_data_release(settings);obs_source_filter_remove(source,filter);obs_source_release(filter);obs_source_release(source);
}
static atomic_bool feeding;
static DWORD WINAPI feed_thread(LPVOID context)
{
    obs_source_t *source=context;
    while(atomic_load(&feeding)) {emit(source,.25f,.25f);os_sleep_ms(22);}
    return 0;
}
int main(int argc,char **argv)
{
    if(argc!=3) return 2;
    char data[2048],graphics[2048];snprintf(data,sizeof(data),"%s/data/libobs/",argv[1]);
    snprintf(graphics,sizeof(graphics),"%s/bin/64bit/libobs-d3d11.dll",argv[1]);
    if(!obs_startup("es-ES",NULL,NULL)) return 3;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    obs_add_data_path(data);
#pragma clang diagnostic pop
    struct obs_audio_info ai={.samples_per_sec=48000,.speakers=SPEAKERS_STEREO};if(!obs_reset_audio(&ai)) return 4;
    struct obs_video_info vi={.graphics_module=graphics,.fps_num=60,.fps_den=1,
        .base_width=512,.base_height=320,.output_width=512,.output_height=320,.output_format=VIDEO_FORMAT_RGBA,
        .adapter=0,.gpu_conversion=false,.colorspace=VIDEO_CS_709,.range=VIDEO_RANGE_FULL,.scale_type=OBS_SCALE_BILINEAR};
    if(obs_reset_video(&vi)!=OBS_VIDEO_SUCCESS) return 5;
    obs_module_t *module=NULL;
    if(obs_open_module(&module,argv[2],"./plugin")!=MODULE_SUCCESS || !obs_init_module(module)) return 6;
    obs_register_source(&fixture_info);obs_register_source(&audio_info);
    obs_source_t *a=obs_source_create("nexus_test_audio","Audio A",NULL,NULL);
    obs_source_t *b=obs_source_create("nexus_test_audio","Audio B",NULL,NULL);
    obs_source_t *s=obs_source_create_private("nexus_test_fixture","RGBA",NULL);
    obs_source_t *f=obs_source_create_private("nexus_reactive_echo","Audio reactive",NULL);
    if(!a||!b||!s||!f) return 7;
    char *id_a=bstrdup(obs_source_get_uuid(a)),*id_b=bstrdup(obs_source_get_uuid(b));
    uint8_t *base=render(s,512,320);red=base[probe];grey=.2126*base[probe]+.7152*base[probe+1]+.0722*base[probe+2];
    obs_source_filter_add(s,f);
    obs_data_t *initial=obs_source_get_settings(f);
    check(obs_data_get_bool(initial,"audio_enabled"),"new_filter_defaults_to_reactive_mode");obs_data_release(initial);
    check(listed(f,id_a,"Audio A") && listed(f,id_b,"Audio B"),"audio_source_selector_uses_UUIDs");
    atomic_init(&observed_peak,0);obs_source_add_audio_capture_callback(a,capture,NULL);
    settings(f,false,id_a,1,0,-48);expect_strength(s,1,"audio_disabled_preserves_manual_effect");
    settings(f,true,"",1,0,-48);expect_strength(s,0,"no_audio_selected_returns_original");
    settings(f,true,id_a,1,0,-48);pump(a,.2511886f,NULL,0,20);expect_strength(s,.75f,"tone_minus12dB_drives_GPU_effect");
    int alpha=0,spill=0;uint8_t *out=render(s,512,320);
    for(size_t i=0;i<512*320;i++) {alpha+=out[i*4+3]!=base[i*4+3];if(!out[i*4+3]&&(out[i*4]||out[i*4+1]||out[i*4+2])) spill++;}
    check(!alpha && !spill,"audio_modulation_preserves_all_alpha_levels");free(out);
    check(abs(atomic_load(&observed_peak)-251189)<3,"audio_samples_unchanged");
    pump(a,0,NULL,0,26);out=render(s,512,320);check(!memcmp(out,base,512*320*4),"silence_reaches_exact_identity");free(out);
    settings(f,true,id_a,1,12,-48);pump(a,.2511886f,NULL,0,20);expect_strength(s,1,"sensitivity_gain_clamps_to_one");
    settings(f,true,id_a,1,0,-6);pump(a,.2511886f,NULL,0,20);expect_strength(s,0,"threshold_rejects_quiet_audio");
    settings(f,true,id_a,.5f,0,-48);pump(a,0,NULL,0,25);expect_strength(s,0,"partial_depth_has_no_manual_floor_in_silence");
    pump(a,.2511886f,NULL,0,20);expect_strength(s,.375f,"partial_depth_scales_audio_magnitude");
    settings(f,true,id_a,0,0,-48);expect_strength(s,0,"zero_depth_disables_reactive_effect");
    settings(f,true,id_a,1,0,-48);
    for(int i=0;i<20;i++) {emit(a,0,.2511886f);os_sleep_ms(22);}expect_strength(s,.75f,"right_channel_drives_detector");
    obs_source_set_volume(a,.1f);pump(a,.2511886f,NULL,0,20);expect_strength(s,1.0f/3,"source_volume_is_respected");
    obs_source_set_volume(a,1);obs_source_set_muted(a,true);pump(a,.2511886f,NULL,0,26);expect_strength(s,0,"source_mute_releases_effect");
    obs_source_set_muted(a,false);pump(a,.2511886f,NULL,0,20);expect_strength(s,.75f,"unmute_restores_reactivity");
    obs_source_set_enabled(a,false);pump(a,.2511886f,NULL,0,26);expect_strength(s,0,"disabled_audio_source_releases_effect");
    obs_source_set_enabled(a,true);pump(a,.2511886f,NULL,0,20);expect_strength(s,.75f,"reenabled_audio_source_recovers");
    os_sleep_ms(750);out=render(s,512,320);check(!memcmp(out,base,512*320*4),"stopped_callbacks_cannot_freeze_effect");free(out);
    obs_source_set_name(a,"Audio renombrado");pump(a,.2511886f,NULL,0,20);expect_strength(s,.75f,"source_rename_keeps_connection");
    check(listed(f,id_a,"Audio renombrado"),"renamed_source_keeps_selected_UUID");
    settings(f,true,id_b,1,0,-48);pump(a,.2511886f,b,0,20);expect_strength(s,0,"switch_detaches_previous_audio_source");
    pump(a,0,b,.2511886f,20);expect_strength(s,.75f,"new_audio_source_drives_effect");
    obs_source_remove(b);os_sleep_ms(650);expect_strength(s,0,"removing_selected_source_releases_effect");
    check(listed(f,id_b,"Fuente no disponible"),"missing_source_is_visible_in_selector");
    obs_weak_source_t *weak_b=obs_source_get_weak_source(b);obs_source_release(b);os_sleep_ms(200);
    check(obs_weak_source_expired(weak_b),"plugin_does_not_keep_deleted_audio_source_alive");obs_weak_source_release(weak_b);
    const char *late_id="12d55e0f-64e3-41fb-a71d-010203040506";
    settings(f,true,late_id,1,0,-48);
    obs_properties_t *old_properties=obs_source_properties(f);
    obs_data_t *late=obs_data_create();obs_data_set_string(late,"id","nexus_test_audio");obs_data_set_string(late,"name","Late audio");obs_data_set_string(late,"uuid",late_id);
    obs_source_t *later=obs_load_source(late);obs_data_release(late);if(!later) return 8;
    obs_property_button_clicked(obs_properties_get(old_properties,"audio_refresh"),f);
    obs_property_t *choices=obs_properties_get(old_properties,"audio_source");bool refreshed=false;
    for(size_t i=0;i<obs_property_list_item_count(choices);i++) if(!strcmp(obs_property_list_item_name(choices,i),"Late audio")) refreshed=true;
    check(refreshed,"refresh_button_discovers_new_audio_source");obs_properties_destroy(old_properties);
    check(!strcmp(obs_source_get_uuid(later),late_id),"late_source_restores_saved_UUID");
    pump(later,.2511886f,NULL,0,65);expect_strength(s,.75f,"late_loaded_audio_source_reconnects");
    settings(f,true,id_a,1,0,-48);
    obs_data_t *timing=obs_source_get_settings(f);obs_data_set_double(timing,"audio_attack",300);obs_data_set_double(timing,"audio_release",700);
    obs_source_update(f,timing);obs_data_release(timing);os_sleep_ms(70);
    uint64_t start=os_gettime_ns();pump(a,.2511886f,NULL,0,13);
    float elapsed=(float)(os_gettime_ns()-start)/1e9f;
    float attack_value=strength(s),attack_expected=.75f*(1-expf(-elapsed/.3f));
    printf("attack measured=%.3f expected=%.3f\n",attack_value,attack_expected);
    check(fabsf(attack_value-attack_expected)<.08f && attack_value<.7f,"attack_time_is_smoothed_in_seconds");
    pump(a,.2511886f,NULL,0,30);float before=strength(s);start=os_gettime_ns();pump(a,0,NULL,0,13);
    elapsed=(float)(os_gettime_ns()-start)/1e9f;float release_value=strength(s),release_expected=before*expf(-elapsed/.7f);
    printf("release measured=%.3f expected=%.3f\n",release_value,release_expected);
    check(fabsf(release_value-release_expected)<.08f && release_value>0.1f,"release_time_is_smoothed_in_seconds");
    settings(f,true,id_a,1,0,-48);
    atomic_init(&feeding,true);HANDLE thread=CreateThread(NULL,0,feed_thread,a,0,NULL);if(!thread) return 9;
    os_sleep_ms(120);obs_weak_source_t *weak_filter=obs_source_get_weak_source(f);
    obs_source_filter_remove(s,f);obs_source_release(f);os_sleep_ms(250);
    atomic_store(&feeding,false);WaitForSingleObject(thread,INFINITE);CloseHandle(thread);
    check(obs_weak_source_expired(weak_filter),"destroy_filter_while_audio_is_arriving");obs_weak_source_release(weak_filter);
    expansion_tests(a,id_a);
    reference_audio_tests(a,id_a);
    obs_source_remove_audio_capture_callback(a,capture,NULL);obs_source_release(a);obs_source_release(later);obs_source_release(s);
    bfree(id_a);bfree(id_b);free(base);obs_shutdown();
    printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
