#include <windows.h>
/* Real DLL presets, persistence and GPU rendering, no UI mocks. */
#define main phase1_main
#include "passthrough-test.c"
#undef main
static int failures;
static void check(bool ok,const char *label) {printf("%s: %s\n",label,ok?"PASS":"FAIL");failures+=!ok;}
static obs_data_t *clone(obs_data_t *s) {return obs_data_create_from_json(obs_data_get_json(s));}
static bool style_equal(obs_data_t *a,obs_data_t *b)
{
    const char *floats[]={"amount","feedback","zoom","rotation","offset_x","offset_y","echo_hue_step","audio_zoom",
        "exposure","contrast","saturation","hue","lut_strength","gradient_middle"};
    const char *ints[]={"color_mode","gradient_palette","gradient_bands","gradient_near","gradient_mid","gradient_far"};
    for(size_t i=0;i<sizeof(floats)/sizeof(floats[0]);i++) if(obs_data_get_double(a,floats[i])!=obs_data_get_double(b,floats[i])) return false;
    for(size_t i=0;i<sizeof(ints)/sizeof(ints[0]);i++) if(obs_data_get_int(a,ints[i])!=obs_data_get_int(b,ints[i])) return false;
    return obs_data_get_bool(a,"silhouette")==obs_data_get_bool(b,"silhouette") &&
        obs_data_get_bool(a,"trail_color")==obs_data_get_bool(b,"trail_color") &&
        !strcmp(obs_data_get_string(a,"lut_path"),obs_data_get_string(b,"lut_path"));
}
static bool audio_kept(obs_data_t *s)
{
    return obs_data_get_bool(s,"audio_enabled") && !strcmp(obs_data_get_string(s,"audio_source"),"12345678-1234-4567-8901-010203040506") &&
        fabs(obs_data_get_double(s,"audio_depth")-.3)<.0001 && obs_data_get_double(s,"audio_gain")==3 &&
        obs_data_get_double(s,"audio_threshold")==-40 && obs_data_get_double(s,"audio_attack")==37 && obs_data_get_double(s,"audio_release")==777;
}
static void click(obs_properties_t *p,obs_source_t *f,const char *name)
{
    if(!obs_property_button_clicked(obs_properties_get(p,name),f)) {printf("button %s failed\n",name);failures++;}
    os_sleep_ms(80);
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

    obs_source_t *ref=obs_source_create_private("nexus_test_fixture","Reference",NULL);
    obs_source_t *f=manual_filter("Echo");
    if(!s||!ref||!f) return 6;
    obs_source_filter_add(s,f);


    obs_properties_t *p=obs_source_properties(f);
    check(obs_property_list_item_count(obs_properties_get(p,"preset_selection"))==7,"six_presets_and_placeholder");
    check(!obs_property_enabled(obs_properties_get(p,"preset_apply")) &&
          !obs_property_enabled(obs_properties_get(p,"style_restore")) &&
          !obs_property_enabled(obs_properties_get(p,"style_undo")),"no_apply_restore_or_undo_without_selection_or_snapshot");
    obs_data_t *settings=obs_source_get_settings(f);
    obs_data_set_bool(settings,"audio_enabled",true);obs_data_set_string(settings,"audio_source","12345678-1234-4567-8901-010203040506");
    obs_data_set_double(settings,"audio_depth",.3);obs_data_set_double(settings,"audio_gain",3);
    obs_data_set_double(settings,"audio_threshold",-40);obs_data_set_double(settings,"audio_attack",37);obs_data_set_double(settings,"audio_release",777);
    obs_data_set_double(settings,"amount",.7);obs_data_set_double(settings,"feedback",.8);obs_data_set_double(settings,"zoom",1.02);
    obs_data_set_double(settings,"rotation",1);obs_data_set_double(settings,"offset_x",2);obs_data_set_double(settings,"offset_y",-3);
    obs_data_set_double(settings,"echo_hue_step",-8);obs_data_set_double(settings,"audio_zoom",.02);
    obs_data_set_double(settings,"exposure",.15);obs_data_set_double(settings,"contrast",1);
    obs_data_set_double(settings,"saturation",1);obs_data_set_double(settings,"hue",10);obs_data_set_double(settings,"lut_strength",.7);
    obs_data_set_bool(settings,"silhouette",true);obs_data_set_bool(settings,"trail_color",true);
    obs_data_set_int(settings,"color_mode",1);obs_data_set_int(settings,"gradient_palette",4);
    obs_data_set_int(settings,"gradient_near",0x00ff0011);obs_data_set_int(settings,"gradient_mid",0x0011ff22);
    obs_data_set_int(settings,"gradient_bands",6);obs_data_set_int(settings,"gradient_far",0x001122ff);obs_data_set_double(settings,"gradient_middle",.3);
    char lut[2048];snprintf(lut,sizeof(lut),"%s/identity.cube",argv[3]);obs_data_set_string(settings,"lut_path",lut);
    obs_source_update(f,settings);os_sleep_ms(80);
    obs_data_t *personal=clone(settings);obs_data_release(settings);
    click(p,f,"style_save");check(obs_property_enabled(obs_properties_get(p,"style_restore")),"saving_enables_personal_restore");
    settings=obs_source_get_settings(f);obs_data_t *saved=obs_data_get_obj(settings,"saved_style");
    check(style_equal(saved,personal),"personal_slot_captures_all_visual_settings_and_LUT");
    check(!obs_data_has_user_value(saved,"audio_source") && !obs_data_has_user_value(saved,"audio_enabled"),"personal_style_excludes_audio_config");
    obs_data_release(saved);
    obs_data_set_int(settings,"preset_selection",1);obs_source_update(f,settings);obs_properties_apply_settings(p,settings);
    check(style_equal(settings,personal),"selecting_preset_does_not_change_visual_parameters");obs_data_release(settings);
    const double amounts[]={1,1,1,.65,0},zooms[]={1.015,1.02,1.008,1.008,1.008};
    for(int id=1;id<=5;id++) {
        settings=obs_source_get_settings(f);obs_data_t *before=clone(settings);
        obs_data_set_int(settings,"preset_selection",id);obs_source_update(f,settings);obs_data_release(settings);
        click(p,f,"preset_apply");settings=obs_source_get_settings(f);
        char label[100];snprintf(label,sizeof(label),"preset_%d_expected_amount_and_geometry",id);
        check(fabs(obs_data_get_double(settings,"amount")-amounts[id-1])<.0001 &&
              fabs(obs_data_get_double(settings,"zoom")-zooms[id-1])<.0001,label);
        snprintf(label,sizeof(label),"preset_%d_preserves_audio_selection_and_detector",id);check(audio_kept(settings),label);
        snprintf(label,sizeof(label),"preset_%d_keeps_LUT_path_and_custom_colours",id);
        check(!strcmp(obs_data_get_string(settings,"lut_path"),lut) && obs_data_get_int(settings,"gradient_near")==0x00ff0011,label);
        obs_data_release(settings);click(p,f,"style_undo");settings=obs_source_get_settings(f);
        snprintf(label,sizeof(label),"preset_%d_undo_restores_previous_visual_style",id);check(style_equal(settings,before),label);
        obs_data_release(settings);obs_data_release(before);
        /* Apply again to exercise the next preset from a different previous style. */
        click(p,f,"preset_apply");
    }
    click(p,f,"style_restore");settings=obs_source_get_settings(f);
    check(style_equal(settings,personal) && audio_kept(settings),"personal_restore_exact_style_without_audio_changes");
    obs_data_set_int(settings,"preset_selection",999);obs_source_update(f,settings);obs_data_release(settings);
    click(p,f,"preset_apply");settings=obs_source_get_settings(f);
    check(style_equal(settings,personal),"unknown_preset_keeps_existing_style");
    obs_data_set_bool(settings,"audio_enabled",false);obs_source_update(f,settings);obs_data_release(settings);
    /* Save full OBS filter then really destroy it before loading the same UUID. */
    saved=obs_save_source(f);obs_properties_destroy(p);obs_source_filter_remove(s,f);obs_source_release(f);os_sleep_ms(100);
    f=obs_load_source(saved);obs_data_release(saved);check(f!=NULL,"preset_filter_save_load");if(!f) return 7;
    obs_source_filter_add(s,f);p=obs_source_properties(f);
    check(obs_property_enabled(obs_properties_get(p,"style_restore")),"personal_snapshot_survives_filter_reload");
    click(p,f,"style_restore");settings=obs_source_get_settings(f);
    check(style_equal(settings,personal) && !obs_data_get_bool(settings,"audio_enabled"),"reloaded_personal_restore_keeps_current_audio_state");
    obs_data_set_int(settings,"preset_selection",5);obs_source_update(f,settings);obs_data_release(settings);click(p,f,"preset_apply");
    uint8_t *out=render(s,512,320),*original=render(ref,512,320);
    check(!memcmp(out,original,512*320*4),"rest_preset_exact_GPU_RGBA_identity");free(out);
    settings=obs_source_get_settings(f);obs_data_set_int(settings,"preset_selection",1);obs_source_update(f,settings);obs_data_release(settings);
    click(p,f,"preset_apply");
    for(int step=0;step<12;step++) {os_sleep_ms(40);out=render(s,512,320);if(step<11) free(out);}
    unsigned opaque_changes=0,external=0;
    for(size_t i=0;i<512*320;i++) {opaque_changes+=original[i*4+3]==255 && memcmp(original+i*4,out+i*4,4)!=0;
        external+=original[i*4+3]==0 && out[i*4+3]>0;}
    check(!opaque_changes && external>0,"warm_preset_renders_external_gradient_and_preserves_foreground");free(out);free(original);
    /* Saving replaces one slot and never recursively stores previous snapshots. */
    settings=obs_source_get_settings(f);obs_data_set_double(settings,"amount",.42);obs_source_update(f,settings);obs_data_release(settings);
    click(p,f,"style_save");settings=obs_source_get_settings(f);saved=obs_data_get_obj(settings,"saved_style");
    check(fabs(obs_data_get_double(saved,"amount")-.42)<.0001 && !obs_data_has_user_value(saved,"saved_style") &&
          !obs_data_has_user_value(saved,"previous_style"),"personal_slot_replaced_without_recursive_growth");
    obs_data_release(saved);obs_data_release(settings);
    settings=obs_source_get_settings(f);obs_data_set_int(settings,"preset_selection",6);obs_source_update(f,settings);obs_data_release(settings);
    obs_properties_t *reference_props=obs_source_properties(f);
    check(obs_property_enabled(obs_properties_get(reference_props,"preset_apply")),"reference_preset_apply_button_enabled");obs_properties_destroy(reference_props);
    click(p,f,"preset_apply");settings=obs_source_get_settings(f);
    check(obs_data_get_bool(settings,"reference_mode") && fabs(obs_data_get_double(settings,"zoom")-1.14)<.0001 &&
        fabs(obs_data_get_double(settings,"feedback")-.8)<.0001,"reference_preset_uses_scene_geometry_and_feedback");
    obs_data_set_string(settings,"base_lut_path",lut);obs_data_set_string(settings,"reactive_lut_path",lut);
    obs_data_set_double(settings,"reference_opacity",.42);obs_source_update(f,settings);obs_data_release(settings);
    click(p,f,"style_save");settings=obs_source_get_settings(f);obs_data_set_bool(settings,"reference_mode",false);
    obs_data_set_string(settings,"base_lut_path","");obs_data_set_string(settings,"reactive_lut_path","");obs_source_update(f,settings);obs_data_release(settings);
    click(p,f,"style_restore");settings=obs_source_get_settings(f);
    check(obs_data_get_bool(settings,"reference_mode") && !strcmp(obs_data_get_string(settings,"base_lut_path"),lut) &&
        !strcmp(obs_data_get_string(settings,"reactive_lut_path"),lut) && fabs(obs_data_get_double(settings,"reference_opacity")-.42)<.0001,
        "reference_personal_style_restores_both_LUT_paths_and_parameters");
    HMODULE native=GetModuleHandleA("nexus-reactive-echo.dll");
    typedef bool (*lookup_fn)(const char *,const char **);
    FARPROC address=GetProcAddress(native,"obs_module_get_string");lookup_fn lookup=NULL;memcpy(&lookup,&address,sizeof(lookup));
    check(lookup!=NULL,"OBS_native_locale_lookup_export");
    const char *languages[]={"es-ES","en-US","de-DE","fr-FR","it-IT","ja-JP"};
    const char *expected[]={"Activar LUT","Enable LUT","LUT aktivieren","Activer la LUT","Attiva LUT","Activar LUT"};
    for(int language=0;language<6;language++){
        obs_set_locale(languages[language]);
        obs_properties_t *localized=obs_source_properties(f);
        check(!strcmp(obs_property_description(obs_properties_get(localized,"lut_enabled")),expected[language]),"native_properties_follow_OBS_language_or_fallback");
        obs_property_t *credit=obs_properties_get(localized,"nexus_author_credit");
        const char *credit_text=credit?obs_property_description(credit):"";
        check(credit && obs_property_get_type(credit)==OBS_PROPERTY_TEXT && strstr(credit_text,"a plugin from <a href=\"https://discord.gg/dhcqZPDYh\">DjEmiMas</a>"),"author_credit_and_discord_link_in_every_locale");
        obs_property_t *last=obs_properties_first(localized);while(last){obs_property_t *next=last;if(!obs_property_next(&next))break;last=next;}
        check(last==credit,"author_credit_is_last_settings_property");
        obs_properties_destroy(localized);
        bool complete=true;
        for(int key=0;key<123;key++){char name[16];snprintf(name,sizeof(name),"T%03d",key);const char *value=NULL;if(!lookup || !lookup(name,&value) || !value || !*value)complete=false;}
        check(complete,"all_123_locale_messages_load_in_OBS");
    }
    obs_set_locale("es-ES");
    obs_data_t *legacy_settings=obs_data_create();obs_data_set_bool(legacy_settings,"audio_enabled",false);
    obs_data_set_bool(legacy_settings,"target_offset",true);obs_data_set_double(legacy_settings,"target_offset_amount",-37);
    obs_source_t *legacy=obs_source_create_private("nexus_reactive_echo","Legacy offset migration",legacy_settings);
    obs_data_t *migrated=obs_source_get_settings(legacy);
    check(obs_data_get_bool(migrated,"target_offset_x") && obs_data_get_bool(migrated,"target_offset_y") &&
        obs_data_get_double(migrated,"target_offset_x_amount")==-37 && obs_data_get_double(migrated,"target_offset_y_amount")==-37,"legacy_combined_offset_migrates_to_both_axes");
    obs_properties_t *axis_props=obs_source_properties(legacy);
    obs_property_t *x_slider=obs_properties_get(axis_props,"target_offset_x_amount"),*y_slider=obs_properties_get(axis_props,"target_offset_y_amount");
    check(x_slider && y_slider && obs_property_float_min(x_slider)==-200 && obs_property_float_max(x_slider)==200 &&
        obs_property_float_min(y_slider)==-200 && obs_property_float_max(y_slider)==200 && !obs_properties_get(axis_props,"target_offset"),"two_signed_symmetric_sliders_replace_combined_control");
    obs_properties_destroy(axis_props);obs_data_release(migrated);obs_data_release(legacy_settings);obs_source_release(legacy);
    obs_data_release(personal);obs_data_release(settings);obs_properties_destroy(p);
    obs_source_filter_remove(s,f);obs_source_release(f);obs_source_release(s);obs_source_release(ref);
    obs_shutdown();printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
