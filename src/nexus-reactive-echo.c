/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <obs-module.h>
#include <util/bmem.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <graphics/vec3.h>
#include <sys/stat.h>
#include <util/platform.h>
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include "cube.h"
#include "reactivity.h"
#include "echo-shader.h"
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("nexus-reactive-echo","es-ES")
MODULE_EXPORT const char *obs_module_name(void) { return "NEXUS Reactive Echo"; }
MODULE_EXPORT const char *obs_module_description(void) { return "Independent GPU recursive echo video filter"; }
MODULE_EXPORT const char *obs_module_author(void) { return "DjEmiMas"; }
struct nexus {
    obs_source_t *source;
    gs_effect_t *effect;
    gs_texrender_t *frames[2];
    gs_texrender_t *foreground;
    bool silhouette, trail_color, gradient, reference;
    bool lut_enabled,base_lut_enabled,reactive_lut_enabled;
    bool target_lut,target_scale,target_feedback,target_color;
    bool offset_targets[2];float offset_amounts[2];
    float target_lut_amount,target_scale_amount,target_feedback_amount,target_color_amount;
    int recursion_mode;
    float center_x, center_y;
    float reference_opacity, reference_factor, reference_level;
    struct lut_slot { char *path; gs_texture_t *texture; struct vec3 scale, offset; } base_lut, reactive_lut;
    struct vec3 gradient_colors[3];
    float gradient_middle;
    int gradient_bands;
    float hue_step, audio_zoom;
    uint32_t width, height;
    unsigned latest;
    bool valid, processed;
    atomic_bool reset_requested;
    uint64_t last_frame;
    float amount, feedback, zoom, angle, x, y;
    float exposure, contrast, saturation, hue, lut_strength;
    bool color_active;
    char *lut_path;
    int64_t lut_revision;
    gs_texture_t *lut_texture;
    struct vec3 lut_scale, lut_offset;
    struct reactivity audio;
    float render_amount;
};
static const char *name(void *unused) { UNUSED_PARAMETER(unused); return "NEXUS Reactive Echo"; }
static float setting(obs_data_t *s, const char *key, float lo, float hi, float fallback)
{
    double v=obs_data_get_double(s,key);
    return isfinite(v) ? (float)fmin(hi,fmax(lo,v)) : fallback;
}
static const char *offset_keys[2]={"target_offset_x","target_offset_y"};
static void migrate_offsets(obs_data_t *s)
{
    bool legacy=obs_data_has_user_value(s,"target_offset");
    bool enabled=legacy && obs_data_get_bool(s,"target_offset");
    double amount=(legacy || obs_data_has_user_value(s,"target_offset_amount"))?obs_data_get_double(s,"target_offset_amount"):0;
    for(int i=0;i<2;i++){
        char key[64];snprintf(key,sizeof(key),"%s_amount",offset_keys[i]);
        if(!obs_data_has_user_value(s,offset_keys[i]))obs_data_set_bool(s,offset_keys[i],enabled);
        if(!obs_data_has_user_value(s,key))obs_data_set_double(s,key,amount);
    }
}
static void defaults(obs_data_t *s)
{
    obs_data_set_default_int(s,"recursion_mode",0);
    obs_data_set_default_double(s,"center_x",0.5);obs_data_set_default_double(s,"center_y",0.5);
    obs_data_set_default_double(s,"amount",0.0);
    obs_data_set_default_bool(s,"silhouette",false);
    obs_data_set_default_bool(s,"reference_mode",false);
    obs_data_set_default_double(s,"reference_opacity",0.3843);
    obs_data_set_default_double(s,"reference_factor",8);
    obs_data_set_default_double(s,"reference_level",0.2);
    obs_data_set_default_bool(s,"lut_enabled",true);
    obs_data_set_default_bool(s,"base_lut_enabled",true);
    obs_data_set_default_bool(s,"reactive_lut_enabled",true);
    obs_data_set_default_string(s,"base_lut_path","");
    obs_data_set_default_string(s,"reactive_lut_path","");
    obs_data_set_default_bool(s,"trail_color",false);
    obs_data_set_default_int(s,"color_mode",0);
    obs_data_set_default_int(s,"gradient_palette",0);
    obs_data_set_default_int(s,"gradient_bands",0);
    obs_data_set_default_int(s,"gradient_near",0x0033B8FF);
    obs_data_set_default_int(s,"gradient_mid",0x005548E8);
    obs_data_set_default_int(s,"gradient_far",0x00702054);
    obs_data_set_default_double(s,"gradient_middle",0.5);
    obs_data_set_default_double(s,"echo_hue_step",-8.0);
    obs_data_set_default_double(s,"audio_zoom",0.0);
    obs_data_set_default_double(s,"feedback",0.85);
    obs_data_set_default_double(s,"zoom",1.01);
    obs_data_set_default_double(s,"rotation",0.0);
    obs_data_set_default_double(s,"offset_x",0.0);
    obs_data_set_default_double(s,"offset_y",0.0);
    obs_data_set_default_double(s,"exposure",0.0);
    obs_data_set_default_double(s,"contrast",1.0);
    obs_data_set_default_double(s,"saturation",1.0);
    obs_data_set_default_double(s,"hue",0.0);
    obs_data_set_default_double(s,"lut_strength",1.0);
    obs_data_set_default_string(s,"lut_path","");
    const char *targets[]={"target_lut","target_scale","target_offset","target_feedback","target_color"};
    const double values[]={1,.05,80,.1,90};
    for(int i=0;i<5;i++){obs_data_set_default_bool(s,targets[i],false);char key[64];snprintf(key,sizeof(key),"%s_amount",targets[i]);obs_data_set_default_double(s,key,values[i]);}
    for(int i=0;i<2;i++){obs_data_set_default_bool(s,offset_keys[i],false);char key[64];snprintf(key,sizeof(key),"%s_amount",offset_keys[i]);obs_data_set_default_double(s,key,0);}
    reactivity_defaults(s);
}
static void load_lut(struct nexus *n, const char *path)
{
    struct cube_lut lut={0}; const char *error=NULL;
    bool loaded=*path && cube_load(path,&lut,&error);
    obs_enter_graphics();
    gs_texture_destroy(n->lut_texture); n->lut_texture=NULL;
    if(loaded) {
        const uint8_t *pixels=(const uint8_t *)lut.rgba;
        n->lut_texture=gs_voltexture_create(lut.size,lut.size,lut.size,GS_RGBA32F,1,&pixels,0);
        for(int c=0;c<3;c++) {
            n->lut_scale.ptr[c]=((float)(lut.size-1)/lut.size)/(lut.domain_max[c]-lut.domain_min[c]);
            n->lut_offset.ptr[c]=0.5f/lut.size-lut.domain_min[c]*n->lut_scale.ptr[c];
        }
        if(!n->lut_texture) blog(LOG_WARNING,"[NEXUS] No se pudo crear la textura LUT GPU");
    }
    obs_leave_graphics(); cube_free(&lut);
    if(*path && !loaded) blog(LOG_WARNING,"[NEXUS] LUT desactivada: %s",error);
    bfree(n->lut_path); n->lut_path=bstrdup(path);
}
static void load_slot(struct lut_slot *slot,const char *path)
{
    if(slot->path && !strcmp(slot->path,path)) return;
    struct cube_lut lut={0};const char *error=NULL;
    bool loaded=*path && cube_load(path,&lut,&error);
    obs_enter_graphics();gs_texture_destroy(slot->texture);slot->texture=NULL;
    if(loaded) {
        for(size_t i=0;i<(size_t)lut.size*lut.size*lut.size;i++)for(int c=0;c<3;c++)
            lut.rgba[i*4+c]=gs_srgb_nonlinear_to_linear(fminf(1,fmaxf(0,lut.rgba[i*4+c])));
        const uint8_t *pixels=(const uint8_t *)lut.rgba;
        slot->texture=gs_voltexture_create(lut.size,lut.size,lut.size,GS_RGBA32F,1,&pixels,0);
        for(int c=0;c<3;c++){
            slot->scale.ptr[c]=((float)(lut.size-1)/lut.size)/(lut.domain_max[c]-lut.domain_min[c]);
            slot->offset.ptr[c]=0.5f/lut.size-lut.domain_min[c]*slot->scale.ptr[c];
        }
    }
    obs_leave_graphics();cube_free(&lut);bfree(slot->path);slot->path=bstrdup(path);
    if(*path && !loaded)blog(LOG_WARNING,"[NEXUS] LUT de referencia desactivada: %s",error);
}
static void gradient_update(struct nexus *n,obs_data_t *s)
{
    static const uint32_t palettes[13][3]={
        {0x0033B8FF,0x005548E8,0x00702054}, /* gold, red, violet */
        {0x00FFEF00,0x00FF20D4,0x0040146D}, /* cyan, magenta, violet */
        {0x0080EAFF,0x000080FF,0x000C1678}, /* fire */
        {0x00FFEDD6,0x00E8A830,0x00662A16}, /* ice */
        {0,0,0}, /* slot 4 is supplied by custom stops */
        {0x00AC3CFF,0x00A04B78,0x00C5862B}, /* Synthwave */
        {0x00FAFFD0,0x00D9B800,0x00722A13}, /* Océano */
        {0x0000FFF6,0x006EE641,0x00A01043}, /* Ácido */
        {0x0078F0FF,0x000036FF,0x00180051}, /* Lava */
        {0x00B8F1FF,0x00C67AFF,0x00CE3D71}, /* Caramelo */
        {0x00B0FF6C,0x00D2BC25,0x00E62C63}, /* Aurora */
        {0x00FFFFFF,0x00929292,0x00161616}, /* Monocromo */
        {0x009BE4FF,0x003284C0,0x00493D00}, /* Oro y petróleo */
    };
    int64_t palette=obs_data_get_int(s,"gradient_palette");
    if(palette<0 || palette>12) palette=0;
    const char *keys[]={"gradient_near","gradient_mid","gradient_far"};
    for(int i=0;i<3;i++) {
        uint32_t c=palette==4?(uint32_t)obs_data_get_int(s,keys[i]):palettes[palette][i];
        vec3_set(&n->gradient_colors[i],(c&255)/255.0f,((c>>8)&255)/255.0f,((c>>16)&255)/255.0f);
    }
    n->gradient=obs_data_get_int(s,"color_mode")==1;
    int64_t bands=obs_data_get_int(s,"gradient_bands");
    n->gradient_bands=bands>=2 && bands<=12?(int)bands:0;
    n->gradient_middle=setting(s,"gradient_middle",0.05f,0.95f,0.5f);
}
static void update(void *data, obs_data_t *s)
{
    struct nexus *n=data;
    bool previous_silhouette=n->silhouette, previous_reference=n->reference;
    n->target_lut=obs_data_get_bool(s,"target_lut");n->target_scale=obs_data_get_bool(s,"target_scale");
    migrate_offsets(s);
    for(int i=0;i<2;i++){char key[64];snprintf(key,sizeof(key),"%s_amount",offset_keys[i]);n->offset_targets[i]=obs_data_get_bool(s,offset_keys[i]);n->offset_amounts[i]=setting(s,key,-200,200,0);}
    n->target_feedback=obs_data_get_bool(s,"target_feedback");n->target_color=obs_data_get_bool(s,"target_color");
    n->target_lut_amount=setting(s,"target_lut_amount",0,1,1);n->target_scale_amount=setting(s,"target_scale_amount",-.2,.2,.05);
    n->target_feedback_amount=setting(s,"target_feedback_amount",-.5,.5,.1);
    n->target_color_amount=setting(s,"target_color_amount",-180,180,90);
    n->recursion_mode=(int)obs_data_get_int(s,"recursion_mode");
    if(n->recursion_mode!=0 && n->recursion_mode!=3){n->recursion_mode=0;obs_data_set_int(s,"recursion_mode",0);}
    n->center_x=setting(s,"center_x",0,1,.5);n->center_y=setting(s,"center_y",0,1,.5);
    n->amount=setting(s,"amount",0,1,0);
    n->silhouette=obs_data_get_bool(s,"silhouette");
    n->reference=obs_data_get_bool(s,"reference_mode");
    n->lut_enabled=obs_data_get_bool(s,"lut_enabled");
    n->base_lut_enabled=obs_data_get_bool(s,"base_lut_enabled");
    n->reactive_lut_enabled=obs_data_get_bool(s,"reactive_lut_enabled");
    n->reference_opacity=setting(s,"reference_opacity",0.01f,1,0.3843f);
    n->reference_factor=setting(s,"reference_factor",0,32,8);
    n->reference_level=setting(s,"reference_level",0,1,0.2f);
    load_slot(&n->base_lut,obs_data_get_string(s,"base_lut_path"));
    load_slot(&n->reactive_lut,obs_data_get_string(s,"reactive_lut_path"));
    n->trail_color=obs_data_get_bool(s,"trail_color");
    gradient_update(n,s);
    n->hue_step=setting(s,"echo_hue_step",-180,180,-8)/360.0f;
    n->audio_zoom=setting(s,"audio_zoom",0,0.1f,0);
    n->feedback=setting(s,"feedback",0,0.98f,0.85f);
    n->zoom=setting(s,"zoom",0.9f,1.3f,1.01f);
    n->angle=setting(s,"rotation",-15,15,0)*0.01745329252f;
    n->x=setting(s,"offset_x",-200,200,0);
    n->y=setting(s,"offset_y",-200,200,0);
    n->exposure=setting(s,"exposure",-4,4,0);
    n->contrast=setting(s,"contrast",0,2,1);
    n->saturation=setting(s,"saturation",0,2,1);
    n->hue=setting(s,"hue",-180,180,0)*0.01745329252f;
    n->lut_strength=setting(s,"lut_strength",0,1,1);
    const char *path=obs_data_get_string(s,"lut_path");
    int64_t revision=obs_data_get_int(s,"lut_revision");
    if(!n->lut_path || strcmp(n->lut_path,path) || revision!=n->lut_revision) {
        load_lut(n,path); n->lut_revision=revision;
    }
    n->color_active=n->exposure!=0 || n->contrast!=1 || n->saturation!=1 || n->hue!=0 ||
                    (n->lut_enabled && n->lut_texture && n->lut_strength>0);
    reactivity_update(&n->audio,s);
    n->render_amount=n->amount*(n->audio.enabled?n->audio.depth*n->audio.envelope:1);
    /* Parameter drags keep feedback alive; only a change of history format resets it. */
    if(previous_silhouette!=n->silhouette || previous_reference!=n->reference) atomic_store(&n->reset_requested,true);
}
static bool reset_button(obs_properties_t *p, obs_property_t *prop, void *data)
{
    UNUSED_PARAMETER(p); UNUSED_PARAMETER(prop);
    if(data) atomic_store(&((struct nexus *)data)->reset_requested,true);
    return false;
}
static const char *localized_lut_status(const char *text) {
    if(!strcmp(text,"Archivo LUT no disponible"))return obs_module_text("T104");
    if(!strcmp(text,"No se pudo preparar el lector LUT"))return obs_module_text("T105");
    if(!strcmp(text,"LUT demasiado grande (máximo 32 MiB)"))return obs_module_text("T106");
    if(!strcmp(text,"Línea LUT demasiado larga"))return obs_module_text("T107");
    if(!strcmp(text,"Cabecera TITLE duplicada o fuera de orden"))return obs_module_text("T108");
    if(!strcmp(text,"TITLE debe ir entre comillas"))return obs_module_text("T109");
    if(!strcmp(text,"TITLE no válido"))return obs_module_text("T110");
    if(!strcmp(text,"Formato no compatible: use LUT 3D con DOMAIN_MIN y DOMAIN_MAX"))return obs_module_text("T111");
    if(!strcmp(text,"LUT_3D_SIZE debe aparecer una vez y estar entre 2 y 65"))return obs_module_text("T112");
    if(!strcmp(text,"Sin memoria para cargar LUT"))return obs_module_text("T113");
    if(!strcmp(text,"Dominio LUT no válido o duplicado"))return obs_module_text("T114");
    if(!strcmp(text,"Datos LUT no válidos o cantidad de filas incorrecta"))return obs_module_text("T115");
    if(!strcmp(text,"Error al leer LUT"))return obs_module_text("T116");
    if(!strcmp(text,"LUT incompleta: se requieren exactamente N³ filas"))return obs_module_text("T117");
    if(!strcmp(text,"DOMAIN_MAX debe ser mayor que DOMAIN_MIN"))return obs_module_text("T118");
    if(!strcmp(text,"Formato válido: LUT 3D"))return obs_module_text("T119");
    return text;
}
static bool lut_path_changed(obs_properties_t *p, obs_property_t *prop, obs_data_t *s)
{
    UNUSED_PARAMETER(prop);
    struct cube_lut lut={0}; const char *error=obs_module_text("T000");
    const char *path=obs_data_get_string(s,"lut_path");
    if(*path) { cube_load(path,&lut,&error); cube_free(&lut); }
    obs_property_set_description(obs_properties_get(p,"lut_status"),localized_lut_status(error));
    return true;
}
static bool reload_lut(obs_properties_t *p, obs_property_t *prop, void *data)
{
    UNUSED_PARAMETER(prop);
    struct nexus *n=data;
    if(!n) return false;
    obs_data_t *s=obs_source_get_settings(n->source);
    obs_data_set_int(s,"lut_revision",(int64_t)os_gettime_ns());
    obs_source_update(n->source,s); lut_path_changed(p,NULL,s); obs_data_release(s);
    return true;
}
static bool silhouette_changed(obs_properties_t *p, obs_property_t *prop, obs_data_t *s)
{
    UNUSED_PARAMETER(prop);
    bool enabled=obs_data_get_bool(s,"silhouette");
    bool reference=enabled && obs_data_get_bool(s,"reference_mode");
    obs_property_set_visible(obs_properties_get(p,"reference_controls"),reference);
    obs_property_set_enabled(obs_properties_get(p,"echo_hue_step"),enabled && !reference);
    bool color_enabled=!enabled || obs_data_get_bool(s,"trail_color");
    bool gradient=enabled && obs_data_get_int(s,"color_mode")==1;
    const char *keys[]={"exposure","contrast","saturation","hue","lut_path","lut_strength","lut_reload"};
    for(size_t i=0;i<sizeof(keys)/sizeof(keys[0]);i++)
        obs_property_set_enabled(obs_properties_get(p,keys[i]),color_enabled && !gradient);
    obs_property_set_enabled(obs_properties_get(p,"color_mode"),enabled && color_enabled);
    obs_property_set_visible(obs_properties_get(p,"lut_controls"),!gradient && !reference);
    obs_property_set_visible(obs_properties_get(p,"reactive_lut_path"),!gradient);
    obs_property_set_visible(obs_properties_get(p,"gradient_controls"),gradient);
    obs_property_set_enabled(obs_properties_get(p,"gradient_controls"),color_enabled);
    bool custom=obs_data_get_int(s,"gradient_palette")==4;
    const char *stops[]={"gradient_near","gradient_mid","gradient_far"};
    for(int i=0;i<3;i++) obs_property_set_visible(obs_properties_get(p,stops[i]),custom);
    /* Native OBS rich text: a real colour strip without a browser or Qt module. */
    struct nexus preview={0};gradient_update(&preview,s);
    char html[4096];size_t used=(size_t)snprintf(html,sizeof(html),"<table cellspacing=\"0\" cellpadding=\"0\"><tr>");
    for(int i=0;i<48;i++) {
        float t=i/47.0f,m=preview.gradient_middle;
        if(preview.gradient_bands>1) t=fminf(1.0f,floorf(t*preview.gradient_bands)/(preview.gradient_bands-1));
        int left=t<m?0:1;float f=t<m?t/m:(t-m)/(1-m);
        unsigned rgb[3];
        for(int c=0;c<3;c++) rgb[c]=(unsigned)lroundf(255*(preview.gradient_colors[left].ptr[c]*(1-f)+preview.gradient_colors[left+1].ptr[c]*f));
        used+=(size_t)snprintf(html+used,sizeof(html)-used,"<td bgcolor=\"#%02X%02X%02X\" width=\"6\">&nbsp;</td>",rgb[0],rgb[1],rgb[2]);
    }
    snprintf(html+used,sizeof(html)-used,"</tr></table>");
    obs_property_set_description(obs_properties_get(p,"gradient_preview"),html);
    obs_property_set_enabled(obs_properties_get(p,"trail_color"),enabled);
    obs_property_set_enabled(obs_properties_get(p,"echo_hue_step"),enabled && !reference);
    obs_property_set_enabled(obs_properties_get(p,"audio_zoom"),enabled);
    obs_property_set_description(obs_properties_get(p,"color_scope"),enabled?
        obs_module_text("T100"):
        obs_module_text("T101"));
    return true;
}

/* Presets own only visual settings; audio source, gain and timing stay untouched. */
static const char *style_floats[]={"amount","feedback","zoom","rotation","offset_x","offset_y",
    "target_lut_amount","target_scale_amount","target_offset_x_amount","target_offset_y_amount","target_feedback_amount","target_color_amount","center_x","center_y","echo_hue_step","audio_zoom","exposure","contrast","saturation","hue","lut_strength","gradient_middle","reference_opacity","reference_factor","reference_level"};
static const char *style_ints[]={"recursion_mode","color_mode","gradient_palette","gradient_bands","gradient_near","gradient_mid","gradient_far"};
static const char *style_bools[]={"silhouette","trail_color","reference_mode","target_lut","target_scale","target_offset_x","target_offset_y","target_feedback","target_color","lut_enabled","base_lut_enabled","reactive_lut_enabled"};
static void copy_style(obs_data_t *to,obs_data_t *from)
{
    migrate_offsets(from);
    for(size_t i=0;i<sizeof(style_floats)/sizeof(style_floats[0]);i++)
        obs_data_set_double(to,style_floats[i],obs_data_get_double(from,style_floats[i]));
    for(size_t i=0;i<sizeof(style_ints)/sizeof(style_ints[0]);i++)
        obs_data_set_int(to,style_ints[i],obs_data_get_int(from,style_ints[i]));
    for(size_t i=0;i<sizeof(style_bools)/sizeof(style_bools[0]);i++)
        obs_data_set_bool(to,style_bools[i],obs_data_get_bool(from,style_bools[i]));
    obs_data_set_string(to,"lut_path",obs_data_get_string(from,"lut_path"));
    obs_data_set_string(to,"base_lut_path",obs_data_get_string(from,"base_lut_path"));
    obs_data_set_string(to,"reactive_lut_path",obs_data_get_string(from,"reactive_lut_path"));
}
static obs_data_t *snapshot_style(obs_data_t *s)
{
    obs_data_t *snapshot=obs_data_create();copy_style(snapshot,s);
    obs_data_set_int(snapshot,"style_version",1);return snapshot;
}
static bool has_style(obs_data_t *s,const char *key)
{
    obs_data_t *style=obs_data_get_obj(s,key);
    bool valid=style && obs_data_get_int(style,"style_version")==1;
    obs_data_release(style);return valid;
}
static bool preset_changed(obs_properties_t *p,obs_property_t *prop,obs_data_t *s)
{
    UNUSED_PARAMETER(prop);
    int64_t id=obs_data_get_int(s,"preset_selection");
    obs_property_set_enabled(obs_properties_get(p,"preset_apply"),id>=1 && id<=6);
    obs_property_set_enabled(obs_properties_get(p,"style_restore"),has_style(s,"saved_style"));
    obs_property_set_enabled(obs_properties_get(p,"style_undo"),has_style(s,"previous_style"));
    obs_property_set_description(obs_properties_get(p,"style_status"),has_style(s,"saved_style")?
        obs_module_text("T013"):obs_module_text("T014"));
    return true;
}
static bool preset_action(obs_properties_t *p,obs_property_t *prop,void *data)
{
    struct nexus *n=data;if(!n) return false;
    obs_data_t *s=obs_source_get_settings(n->source);
    const char *action=obs_property_name(prop);
    if(!strcmp(action,"style_save")) {
        obs_data_t *snapshot=snapshot_style(s);obs_data_set_obj(s,"saved_style",snapshot);obs_data_release(snapshot);
    } else {
        obs_data_t *before=snapshot_style(s);
        bool applied=false;
        if(!strcmp(action,"style_restore") || !strcmp(action,"style_undo")) {
            const char *key=!strcmp(action,"style_restore")?"saved_style":"previous_style";
            if(has_style(s,key)) {
                obs_data_t *style=obs_data_get_obj(s,key);copy_style(s,style);obs_data_release(style);applied=true;
            }
        } else if(!strcmp(action,"preset_apply")) {
            int64_t id=obs_data_get_int(s,"preset_selection");
            if(id>=1 && id<=6) {
                applied=true;
                if(id==6) {obs_data_set_int(s,"recursion_mode",0);
                    obs_data_set_bool(s,"reference_mode",true);obs_data_set_bool(s,"silhouette",true);obs_data_set_bool(s,"trail_color",true);
                    obs_data_set_double(s,"amount",1);obs_data_set_double(s,"feedback",.8);obs_data_set_double(s,"zoom",1.14);
                    obs_data_set_double(s,"rotation",0);obs_data_set_double(s,"offset_x",-1.6);obs_data_set_double(s,"offset_y",5.6);
                    obs_data_set_double(s,"reference_opacity",.3843);obs_data_set_double(s,"reference_factor",8);
                    obs_data_set_double(s,"audio_zoom",0);obs_data_set_int(s,"color_mode",1);
                    obs_data_set_int(s,"gradient_palette",0);obs_data_set_int(s,"gradient_bands",0);
                } else if(id==5) obs_data_set_double(s,"amount",0);
                else {
                    obs_data_set_double(s,"amount",id==4?.65:1);
                    obs_data_set_bool(s,"silhouette",id!=4);obs_data_set_bool(s,"trail_color",id!=4);
                    obs_data_set_int(s,"color_mode",id==4?0:1);
                    obs_data_set_bool(s,"reference_mode",false);obs_data_set_int(s,"gradient_bands",0);obs_data_set_int(s,"gradient_palette",id==2?1:0);obs_data_set_double(s,"gradient_middle",.5);
                    obs_data_set_double(s,"feedback",id==1?.94:id==2?.92:id==3?.93:.86);
                    obs_data_set_double(s,"zoom",id==1?1.015:id==2?1.02:1.008);
                    obs_data_set_double(s,"rotation",id==3?.45:0);
                    obs_data_set_double(s,"offset_x",id==3?1:0);obs_data_set_double(s,"offset_y",0);
                    obs_data_set_double(s,"echo_hue_step",-5);obs_data_set_double(s,"audio_zoom",id==4?0:.025);
                    obs_data_set_double(s,"exposure",0);obs_data_set_double(s,"contrast",1);
                    obs_data_set_double(s,"saturation",1);obs_data_set_double(s,"hue",0);
                    obs_data_set_double(s,"lut_strength",0);
                }
            }
        }
        if(applied) obs_data_set_obj(s,"previous_style",before);
        obs_data_release(before);
    }
    if(strcmp(action,"style_save")) obs_source_update(n->source,s);
    silhouette_changed(p,NULL,s);preset_changed(p,NULL,s);lut_path_changed(p,NULL,s);
    obs_data_release(s);return true;
}

static obs_properties_t *properties(void *data)
{
    obs_properties_t *p=obs_properties_create();
    obs_properties_add_text(p,"usage",obs_module_text("T016"),OBS_TEXT_INFO);
    obs_properties_t *presets=obs_properties_create();
    obs_property_t *selection=obs_properties_add_list(presets,"preset_selection","Preset",OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(selection,obs_module_text("T002"),0);
    obs_property_list_add_int(selection,obs_module_text("T003"),1);
    obs_property_list_add_int(selection,obs_module_text("T004"),2);
    obs_property_list_add_int(selection,obs_module_text("T005"),3);
    obs_property_list_add_int(selection,obs_module_text("T006"),4);
    obs_property_list_add_int(selection,obs_module_text("T007"),5);
    obs_property_list_add_int(selection,obs_module_text("T008"),6);
    obs_property_set_modified_callback(selection,preset_changed);
    obs_properties_add_button2(presets,"preset_apply",obs_module_text("T009"),preset_action,data);
    obs_properties_add_button2(presets,"style_undo",obs_module_text("T010"),preset_action,data);
    obs_properties_add_button2(presets,"style_save",obs_module_text("T011"),preset_action,data);
    obs_properties_add_button2(presets,"style_restore",obs_module_text("T012"),preset_action,data);
    obs_properties_add_text(presets,"style_status","",OBS_TEXT_INFO);
    obs_properties_add_text(presets,"preset_info",obs_module_text("T015"),OBS_TEXT_INFO);
    obs_properties_add_group(p,"preset_controls",obs_module_text("T001"),OBS_GROUP_NORMAL,presets);
    obs_properties_t *echo=obs_properties_create();
    obs_property_t *mode=obs_properties_add_bool(echo,"silhouette",obs_module_text("T017"));
    obs_property_set_modified_callback(mode,silhouette_changed);
    obs_property_t *rec=obs_properties_add_list(echo,"recursion_mode",obs_module_text("T018"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(rec,obs_module_text("T019"),0);
    obs_property_list_add_int(rec,obs_module_text("T103"),3);
    obs_properties_add_float_slider(echo,"center_x",obs_module_text("T020"),0,1,.01);
    obs_properties_add_float_slider(echo,"center_y",obs_module_text("T021"),0,1,.01);

    obs_property_set_modified_callback(obs_properties_add_bool(echo,"reference_mode",obs_module_text("T022")),silhouette_changed);
    obs_properties_add_float_slider(echo,"echo_hue_step",obs_module_text("T023"),-180,180,1);
    obs_properties_add_float_slider(echo,"audio_zoom",obs_module_text("T024"),0,0.1,0.001);
    obs_properties_add_float_slider(echo,"amount",obs_module_text("T025"),0,1,0.01);
    obs_properties_add_float_slider(echo,"feedback",obs_module_text("T026"),0,0.98,0.01);
    obs_properties_add_float_slider(echo,"zoom",obs_module_text("T027"),0.9,1.3,0.001);
    obs_properties_add_float_slider(echo,"rotation",obs_module_text("T028"),-15,15,0.1);
    obs_properties_add_float_slider(echo,"offset_x",obs_module_text("T029"),-200,200,1);
    obs_properties_add_float_slider(echo,"offset_y",obs_module_text("T030"),-200,200,1);
    obs_properties_add_button2(echo,"reset",obs_module_text("T031"),reset_button,data);
    obs_properties_add_group(p,"echo_controls",obs_module_text("T032"),OBS_GROUP_NORMAL,echo);
    obs_properties_t *color=obs_properties_create();
    obs_properties_add_text(color,"color_scope",obs_module_text("T034"),OBS_TEXT_INFO);
    obs_property_t *trail=obs_properties_add_bool(color,"trail_color",obs_module_text("T035"));
    obs_property_set_modified_callback(trail,silhouette_changed);
    obs_property_t *choice=obs_properties_add_list(color,"color_mode",obs_module_text("T036"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(choice,obs_module_text("T037"),0);
    obs_property_list_add_int(choice,obs_module_text("T038"),1);
    obs_property_set_modified_callback(choice,silhouette_changed);
    obs_properties_t *gradient=obs_properties_create();
    obs_property_t *palette=obs_properties_add_list(gradient,"gradient_palette",obs_module_text("T038"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(palette,obs_module_text("T039"),0);
    obs_property_list_add_int(palette,obs_module_text("T040"),1);
    obs_property_list_add_int(palette,obs_module_text("T041"),2);
    obs_property_list_add_int(palette,obs_module_text("T042"),3);
    obs_property_list_add_int(palette,obs_module_text("T043"),4);
    obs_property_list_add_int(palette,obs_module_text("T044"),5);
    obs_property_list_add_int(palette,obs_module_text("T045"),6);
    obs_property_list_add_int(palette,obs_module_text("T046"),7);
    obs_property_list_add_int(palette,obs_module_text("T047"),8);
    obs_property_list_add_int(palette,obs_module_text("T048"),9);
    obs_property_list_add_int(palette,obs_module_text("T049"),10);
    obs_property_list_add_int(palette,obs_module_text("T050"),11);
    obs_property_list_add_int(palette,obs_module_text("T051"),12);
    obs_property_set_modified_callback(palette,silhouette_changed);
    obs_property_t *bands=obs_properties_add_list(gradient,"gradient_bands",obs_module_text("T052"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(bands,obs_module_text("T053"),0);
    obs_property_list_add_int(bands,obs_module_text("T054"),3);
    obs_property_list_add_int(bands,obs_module_text("T055"),4);
    obs_property_list_add_int(bands,obs_module_text("T056"),6);
    obs_property_list_add_int(bands,obs_module_text("T057"),8);
    obs_property_list_add_int(bands,obs_module_text("T058"),12);
    obs_property_set_modified_callback(bands,silhouette_changed);
    obs_properties_add_text(gradient,"gradient_preview","",OBS_TEXT_INFO);
    obs_property_set_modified_callback(obs_properties_add_color(gradient,"gradient_near",obs_module_text("T059")),silhouette_changed);
    obs_property_set_modified_callback(obs_properties_add_color(gradient,"gradient_mid",obs_module_text("T060")),silhouette_changed);
    obs_property_set_modified_callback(obs_properties_add_color(gradient,"gradient_far",obs_module_text("T061")),silhouette_changed);
    obs_properties_add_float_slider(gradient,"gradient_middle",obs_module_text("T062"),0.05,0.95,0.01);
    obs_properties_add_text(gradient,"gradient_info",obs_module_text("T063"),OBS_TEXT_INFO);
    obs_properties_add_group(color,"gradient_controls",obs_module_text("T064"),OBS_GROUP_NORMAL,gradient);
    obs_properties_t *lut=obs_properties_create();
    obs_properties_add_float_slider(lut,"exposure",obs_module_text("T065"),-4,4,0.05);
    obs_properties_add_float_slider(lut,"contrast",obs_module_text("T066"),0,2,0.01);
    obs_properties_add_float_slider(lut,"saturation",obs_module_text("T067"),0,2,0.01);
    obs_properties_add_float_slider(lut,"hue",obs_module_text("T068"),-180,180,1);
    obs_properties_add_bool(lut,"lut_enabled",obs_module_text("T069"));
    obs_property_t *path=obs_properties_add_path(lut,"lut_path",obs_module_text("T070"),OBS_PATH_FILE,"LUT 3D (*.cube)",NULL);
    obs_property_set_modified_callback(path,lut_path_changed);
    obs_properties_add_float_slider(lut,"lut_strength",obs_module_text("T071"),0,1,0.01);
    obs_properties_add_text(lut,"lut_status",obs_module_text("T000"),OBS_TEXT_INFO);
    obs_properties_add_button2(lut,"lut_reload",obs_module_text("T072"),reload_lut,data);
    obs_properties_add_group(color,"lut_controls",obs_module_text("T037"),OBS_GROUP_NORMAL,lut);
    obs_properties_t *reference=obs_properties_create();
    obs_properties_add_bool(reference,"base_lut_enabled",obs_module_text("T073"));
    obs_properties_add_path(reference,"base_lut_path",obs_module_text("T074"),OBS_PATH_FILE,"LUT 3D (*.cube)",NULL);
    obs_properties_add_bool(reference,"reactive_lut_enabled",obs_module_text("T075"));
    obs_properties_add_path(reference,"reactive_lut_path",obs_module_text("T076"),OBS_PATH_FILE,"LUT 3D (*.cube)",NULL);
    obs_properties_add_float_slider(reference,"reference_opacity",obs_module_text("T077"),0.01,1,0.01);
    obs_properties_add_float_slider(reference,"reference_factor",obs_module_text("T078"),0,32,0.1);
    obs_properties_add_float_slider(reference,"reference_level",obs_module_text("T079"),0,1,0.01);
    obs_properties_add_text(reference,"reference_info",obs_module_text("T080"),OBS_TEXT_INFO);
    obs_properties_add_group(color,"reference_controls",obs_module_text("T081"),OBS_GROUP_NORMAL,reference);
    obs_properties_add_group(p,"color_controls",obs_module_text("T033"),OBS_GROUP_NORMAL,color);
    obs_properties_t *audio=obs_properties_create();
    if(data) {
        obs_data_t *s=obs_source_get_settings(((struct nexus *)data)->source);
        reactivity_properties(audio,s,((struct nexus *)data)->source);
        obs_properties_add_group(p,"audio_controls",obs_module_text("T102"),OBS_GROUP_NORMAL,audio);
        lut_path_changed(p,NULL,s);silhouette_changed(p,NULL,s);preset_changed(p,NULL,s);obs_data_release(s);
    } else {
        reactivity_properties(audio,NULL,NULL);
        obs_properties_add_group(p,"audio_controls",obs_module_text("T102"),OBS_GROUP_NORMAL,audio);
        obs_data_t *s=obs_data_create();defaults(s);silhouette_changed(p,NULL,s);preset_changed(p,NULL,s);obs_data_release(s);
    }
    obs_properties_t *targets=obs_properties_create();
    obs_properties_add_group(p,"audio_targets",obs_module_text("T082"),OBS_GROUP_NORMAL,targets);
    const char *keys[]={"target_lut","target_scale","target_offset_x","target_offset_y","target_feedback","target_color"};
    const char *labels[]={obs_module_text("T083"),obs_module_text("T084"),obs_module_text("T120"),obs_module_text("T121"),obs_module_text("T086"),obs_module_text("T087")};
    const double lows[]={0,-.2,-200,-200,-.5,-180}, highs[]={1,.2,200,200,.5,180},steps[]={.01,.001,1,1,.01,1};
    for(int i=0;i<6;i++){obs_properties_add_bool(targets,keys[i],labels[i]);char key[64],label[128];snprintf(key,sizeof(key),"%s_amount",keys[i]);snprintf(label,sizeof(label),obs_module_text("T088"),labels[i]);obs_properties_add_float_slider(targets,key,label,lows[i],highs[i],steps[i]);}
    obs_properties_add_text(targets,"offset_targets_info",obs_module_text("T122"),OBS_TEXT_INFO);
    obs_properties_add_text(p,"nexus_author_credit",
        "<p>a plugin from <a href=\"https://discord.gg/dhcqZPDYh\">DjEmiMas</a></p>",OBS_TEXT_INFO);
    return p;
}
static void destroy(void *data)
{
    struct nexus *n=data;
    reactivity_destroy(&n->audio);
    obs_enter_graphics();
    gs_effect_destroy(n->effect);
    gs_texrender_destroy(n->foreground);
    gs_texture_destroy(n->lut_texture);
    gs_texture_destroy(n->base_lut.texture);gs_texture_destroy(n->reactive_lut.texture);
    bfree(n->base_lut.path);bfree(n->reactive_lut.path);
    for(int i=0;i<2;i++) gs_texrender_destroy(n->frames[i]);
    obs_leave_graphics(); bfree(n->lut_path); bfree(n);
}
static void *create(obs_data_t *s, obs_source_t *source)
{
    struct nexus *n=bzalloc(sizeof(*n)); n->source=source;
    reactivity_init(&n->audio);
    atomic_init(&n->reset_requested,true);
    obs_enter_graphics();
    n->effect=gs_effect_create(echo_shader,"NEXUS Echo",NULL);
    n->foreground=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
    for(int i=0;i<2;i++) n->frames[i]=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
    obs_leave_graphics();
    if(!n->effect || !n->foreground || !n->frames[0] || !n->frames[1]) { destroy(n); return NULL; }
    update(n,s); return n;
}
static void tick(void *data, float seconds)
{
    struct nexus *n=data; n->processed=false;
    n->render_amount=n->amount*reactivity_tick(&n->audio,seconds);
    if(!obs_source_enabled(n->source)) {atomic_store(&n->reset_requested,true);n->audio.envelope=0;}
}
static void hide(void *data) { atomic_store(&((struct nexus *)data)->reset_requested,true); }
static void remove_filter(void *data, obs_source_t *parent) { UNUSED_PARAMETER(parent); hide(data); }
static float target_level(struct nexus *n){return n->audio.enabled?n->audio.envelope*n->audio.depth:0;}
static float target_lut_mix(struct nexus *n){return n->target_lut && n->audio.enabled?1-n->target_lut_amount+n->target_lut_amount*target_level(n):1;}
static void set_color_parameters(struct nexus *n)
{
    gs_effect_t *effect=n->effect;
    gs_effect_set_float(gs_effect_get_param_by_name(effect,"audio_color"),n->target_color?n->target_color_amount*0.01745329252f*target_level(n):0);
    if(n->color_active) {
        gs_effect_set_float(gs_effect_get_param_by_name(effect,"amount"),n->render_amount);
        gs_effect_set_float(gs_effect_get_param_by_name(effect,"exposure"),n->exposure);
        gs_effect_set_float(gs_effect_get_param_by_name(effect,"contrast"),n->contrast);
        gs_effect_set_float(gs_effect_get_param_by_name(effect,"saturation"),n->saturation);
        gs_effect_set_float(gs_effect_get_param_by_name(effect,"hue"),n->hue);
        gs_effect_set_float(gs_effect_get_param_by_name(effect,"lut_strength"),n->lut_enabled && n->lut_texture?n->lut_strength*target_lut_mix(n):0);
        gs_effect_set_texture(gs_effect_get_param_by_name(effect,"color_lut"),n->lut_texture);
        gs_effect_set_vec3(gs_effect_get_param_by_name(effect,"lut_scale"),&n->lut_scale);
        gs_effect_set_vec3(gs_effect_get_param_by_name(effect,"lut_offset"),&n->lut_offset);
    }
}
static void set_reference_parameters(struct nexus *n)
{
    gs_effect_t *e=n->effect;
    const char *colors[]={"gradient_near","gradient_mid","gradient_far"};
    for(int i=0;i<3;i++)gs_effect_set_vec3(gs_effect_get_param_by_name(e,colors[i]),&n->gradient_colors[i]);
    gs_effect_set_float(gs_effect_get_param_by_name(e,"gradient_middle"),n->gradient_middle);
    gs_effect_set_float(gs_effect_get_param_by_name(e,"gradient_bands"),(float)n->gradient_bands);
    gs_effect_set_float(gs_effect_get_param_by_name(e,"reference_opacity"),n->reference_opacity);
    float level=n->audio.enabled?n->audio.linear_envelope:n->reference_level;
    gs_effect_set_float(gs_effect_get_param_by_name(e,"reference_level"),level*n->reference_factor);
    gs_effect_set_float(gs_effect_get_param_by_name(e,"reactive_mix"),target_lut_mix(n));
    gs_effect_set_float(gs_effect_get_param_by_name(e,"audio_color"),n->target_color?n->target_color_amount*0.01745329252f*target_level(n):0);
    gs_effect_set_float(gs_effect_get_param_by_name(e,"reference_gradient"),n->gradient && n->trail_color?1:0);
    struct lut_slot *slots[]={&n->base_lut,&n->reactive_lut};
    const char *texture[]={"base_lut","reactive_lut"},*enabled[]={"base_lut_enabled","reactive_lut_enabled"};
    const char *scale[]={"base_lut_scale","reactive_lut_scale"},*offset[]={"base_lut_offset","reactive_lut_offset"};
    for(int i=0;i<2;i++){
        gs_effect_set_texture(gs_effect_get_param_by_name(e,texture[i]),slots[i]->texture);
        gs_effect_set_float(gs_effect_get_param_by_name(e,enabled[i]),slots[i]->texture && (i==0?n->base_lut_enabled:n->reactive_lut_enabled)?1:0);
        gs_effect_set_vec3(gs_effect_get_param_by_name(e,scale[i]),&slots[i]->scale);
        gs_effect_set_vec3(gs_effect_get_param_by_name(e,offset[i]),&slots[i]->offset);
    }
}
static void draw_result(struct nexus *n)
{
    gs_texture_t *tex=gs_texrender_get_texture(n->frames[n->latest]);
    if(n->silhouette) {
        gs_blend_state_push(); gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
        gs_effect_set_texture(gs_effect_get_param_by_name(n->effect,"image"),tex);
        gs_effect_set_texture(gs_effect_get_param_by_name(n->effect,"foreground_image"),gs_texrender_get_texture(n->foreground));
        gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"amount"),n->render_amount);
        set_color_parameters(n);
        if(n->reference)set_reference_parameters(n);
        if((n->trail_color && n->gradient) || n->reference) {
            const char *names[]={"gradient_near","gradient_mid","gradient_far"};
            for(int i=0;i<3;i++) gs_effect_set_vec3(gs_effect_get_param_by_name(n->effect,names[i]),&n->gradient_colors[i]);
            gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"gradient_middle"),n->gradient_middle);
            gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"gradient_bands"),(float)n->gradient_bands);
        }
        const char *tech=n->reference?"ReferenceComposite":n->trail_color && n->gradient?"CompositeGradient":n->trail_color && n->color_active?"CompositeColor":"Composite";
        while(gs_effect_loop(n->effect,tech)) gs_draw_sprite(tex,0,n->width,n->height);
        gs_blend_state_pop(); return;
    }
    gs_effect_t *effect=n->color_active?n->effect:obs_get_base_effect(OBS_EFFECT_DEFAULT);
    gs_effect_set_texture(gs_effect_get_param_by_name(effect,"image"),tex);
    set_color_parameters(n);
    gs_blend_state_push(); gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
    while(gs_effect_loop(effect,n->color_active?obs_module_text("T033"):"Draw")) gs_draw_sprite(tex,0,n->width,n->height);
    gs_blend_state_pop();
}
static void render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(effect);
    struct nexus *n=data;
    obs_source_t *target=obs_filter_get_target(n->source);
    uint32_t w=target?obs_source_get_base_width(target):0;
    uint32_t h=target?obs_source_get_base_height(target):0;
    const enum gs_color_space spaces[]={GS_CS_SRGB,GS_CS_SRGB_16F,GS_CS_709_EXTENDED};
    enum gs_color_space space=target?obs_source_get_color_space(target,3,spaces):GS_CS_SRGB;
    if(!w || !h || n->amount<=0 || (n->render_amount<=0 && !(n->reference && n->silhouette)) || (n->feedback<=0 && (n->silhouette || !n->color_active)) || space!=GS_CS_SRGB) {
        n->valid=false; n->processed=false;
        obs_source_skip_video_filter(n->source); return;
    }
    uint64_t now=obs_get_video_frame_time();
    if(atomic_exchange(&n->reset_requested,false) || w!=n->width || h!=n->height ||
       (n->last_frame && now-n->last_frame>250000000ULL)) { n->valid=false; n->processed=false; }
    n->width=w; n->height=h;
    if(n->processed) { draw_result(n); return; }
    unsigned next=n->latest^1u;
    gs_texrender_t *rt=n->frames[next]; gs_texrender_reset(rt);
    if(!obs_source_process_filter_begin(n->source,GS_RGBA,OBS_NO_DIRECT_RENDERING)) { n->valid=false; return; }
    if(n->silhouette) {
        gs_texrender_reset(n->foreground);
        if(!gs_texrender_begin(n->foreground,w,h)) { n->valid=false; obs_source_skip_video_filter(n->source); return; }
        struct vec4 zero; vec4_zero(&zero);
        gs_clear(GS_CLEAR_COLOR,&zero,0,0); gs_ortho(0,(float)w,0,(float)h,-100,100);
        gs_blend_state_push(); gs_blend_function(GS_BLEND_ONE,GS_BLEND_ZERO);
        obs_source_process_filter_tech_end(n->source,n->effect,w,h,"Capture");
        gs_blend_state_pop(); gs_texrender_end(n->foreground);
    }
    if(!gs_texrender_begin(rt,w,h)) { n->valid=false; obs_source_skip_video_filter(n->source); return; }
    struct vec4 clear; vec4_zero(&clear);
    gs_clear(GS_CLEAR_COLOR,&clear,0,0); gs_ortho(0,(float)w,0,(float)h,-100,100);
    struct vec2 size, shift;
    vec2_set(&size,(float)w,(float)h); vec2_set(&shift,n->reference?n->x*w/1920.0f:n->x,n->reference?n->y*h/1080.0f:n->y);
    gs_effect_set_vec2(gs_effect_get_param_by_name(n->effect,"dimensions"),&size);
    struct vec2 center;vec2_set(&center,n->center_x,n->center_y);
    gs_effect_set_vec2(gs_effect_get_param_by_name(n->effect,"recursion_center"),&center);
    gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"recursion_mode"),(float)n->recursion_mode);

    float audio_level=target_level(n);
    for(int i=0;i<2;i++)if(n->offset_targets[i]){if(i==0)shift.x+=n->offset_amounts[i]*audio_level;else shift.y+=n->offset_amounts[i]*audio_level;}
    gs_effect_set_vec2(gs_effect_get_param_by_name(n->effect,"offset"),&shift);
    /* Geometry follows the same smoothed envelope, without scaling the foreground. */
    float zoom=n->zoom+(n->target_scale?n->target_scale_amount*target_level(n):0);
    if(n->silhouette && n->audio.enabled)
        zoom+=n->audio_zoom*n->audio.depth*n->audio.envelope;
    gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"zoom"),fmaxf(.5f,zoom));
    set_reference_parameters(n);
    gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"angle"),n->angle);
    gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"amount"),n->valid?n->render_amount:0);
    gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"feedback"),fminf(.98f,fmaxf(0,n->feedback+(n->target_feedback?n->target_feedback_amount*target_level(n):0))));
    /* Read and write use different GPU textures. First frame has zero weight. */
    gs_effect_set_texture(gs_effect_get_param_by_name(n->effect,"history"),
                         n->valid?gs_texrender_get_texture(n->frames[n->latest]):NULL);
    gs_blend_state_push(); gs_blend_function(GS_BLEND_ONE,GS_BLEND_ZERO);
    if(n->silhouette) {
        gs_texture_t *front=gs_texrender_get_texture(n->foreground);
        gs_effect_set_texture(gs_effect_get_param_by_name(n->effect,"image"),front);
        gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"history_valid"),n->valid?1:0);
        gs_effect_set_float(gs_effect_get_param_by_name(n->effect,"hue_step"),n->hue_step);
        while(gs_effect_loop(n->effect,n->reference?"Reference":"Silhouette")) gs_draw_sprite(front,0,w,h);
    } else obs_source_process_filter_end(n->source,n->effect,w,h);
    gs_blend_state_pop(); gs_texrender_end(rt);
    n->latest=next; n->valid=true; n->processed=true; n->last_frame=now;
    draw_result(n);
}
static struct obs_source_info info={
    .id="nexus_reactive_echo",.type=OBS_SOURCE_TYPE_FILTER,.output_flags=OBS_SOURCE_VIDEO,
    .get_name=name,.create=create,.destroy=destroy,.video_render=render,
    .get_defaults=defaults,.get_properties=properties,.update=update,.video_tick=tick,
    .hide=hide,.filter_remove=remove_filter,
};
bool obs_module_load(void) { obs_register_source(&info); return true; }

