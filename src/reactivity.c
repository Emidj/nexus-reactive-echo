/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "reactivity.h"
#include <obs-module.h>
#include <sys/stat.h>
#include <util/platform.h>
#include <util/bmem.h>
#include <math.h>
#include <string.h>

static float bounded(obs_data_t *s,const char *key,float lo,float hi,float fallback)
{
    double v=obs_data_get_double(s,key);
    return isfinite(v)?(float)fmin(hi,fmax(lo,v)):fallback;
}
static void levels(void *data,const float magnitude[MAX_AUDIO_CHANNELS],
                   const float peak[MAX_AUDIO_CHANNELS],const float input_peak[MAX_AUDIO_CHANNELS])
{
    UNUSED_PARAMETER(input_peak);
    struct reactivity *a=data;
    float db=-120,mag=-120;
    for(unsigned i=0;i<MAX_AUDIO_CHANNELS;i++) if(isfinite(peak[i]) && peak[i]>db) db=peak[i];
    for(unsigned i=0;i<MAX_AUDIO_CHANNELS;i++) if(isfinite(magnitude[i]) && magnitude[i]>mag)mag=magnitude[i];
    atomic_store(&a->magnitude_mdb,(int)lroundf(fminf(24,mag)*1000));
    /* Audio callback only publishes bounded scalars: no graphics, IO or allocation. */
    atomic_store(&a->peak_mdb,(int)lroundf(fminf(24,db)*1000));
    atomic_store_explicit(&a->sample_time,os_gettime_ns(),memory_order_release);
}
static void disconnect_source(struct reactivity *a)
{
    if(a->meter) {
        /* Removal waits for any callback already in progress before a can be freed. */
        obs_volmeter_remove_callback(a->meter,levels,a);
        obs_volmeter_destroy(a->meter);a->meter=NULL;
    }
    if(a->weak) {obs_weak_source_release(a->weak);a->weak=NULL;}
    atomic_store(&a->peak_mdb,-120000);atomic_store(&a->magnitude_mdb,-120000);atomic_store(&a->sample_time,0);
}
static void connect_source(struct reactivity *a,uint64_t now)
{
    a->retry_time=now+1000000000ULL;
    if(!a->enabled || !a->uuid || !*a->uuid) return;
    obs_source_t *source=obs_get_source_by_uuid(a->uuid);
    if(source && !obs_source_removed(source) && (obs_source_get_output_flags(source)&OBS_SOURCE_AUDIO)) {
        a->meter=obs_volmeter_create(OBS_FADER_LOG);
        if(a->meter) {
            obs_volmeter_add_callback(a->meter,levels,a);
            if(obs_volmeter_attach_source(a->meter,source)) a->weak=obs_source_get_weak_source(source);
            else disconnect_source(a);
        }
    }
    obs_source_release(source);
}
void reactivity_init(struct reactivity *a)
{
    memset(a,0,sizeof(*a));atomic_init(&a->peak_mdb,-120000);atomic_init(&a->magnitude_mdb,-120000);atomic_init(&a->sample_time,0);
}
void reactivity_defaults(obs_data_t *s)
{
    obs_data_set_default_bool(s,"audio_enabled",true);
    obs_data_set_default_string(s,"audio_source","");
    obs_data_set_default_double(s,"audio_depth",1);
    obs_data_set_default_double(s,"audio_gain",0);
    obs_data_set_default_double(s,"audio_threshold",-48);
    obs_data_set_default_double(s,"audio_attack",30);
    obs_data_set_default_double(s,"audio_release",250);
}
void reactivity_update(struct reactivity *a,obs_data_t *s)
{
    bool enabled=obs_data_get_bool(s,"audio_enabled");
    const char *uuid=obs_data_get_string(s,"audio_source");
    bool changed=enabled!=a->enabled || !a->uuid || strcmp(uuid,a->uuid);
    if(changed) {disconnect_source(a);a->envelope=0;a->linear_envelope=0;bfree(a->uuid);a->uuid=bstrdup(uuid);a->retry_time=0;}
    a->enabled=enabled;
    a->depth=bounded(s,"audio_depth",0,1,1);
    a->gain=bounded(s,"audio_gain",-24,24,0);
    a->threshold=bounded(s,"audio_threshold",-80,-6,-48);
    a->attack=bounded(s,"audio_attack",1,1000,30)*.001f;
    a->release=bounded(s,"audio_release",1,3000,250)*.001f;
    if(changed && enabled) connect_source(a,os_gettime_ns());
}
float reactivity_tick(struct reactivity *a,float seconds)
{
    if(!a->enabled) {a->envelope=0;a->linear_envelope=0;return 1;}
    uint64_t now=os_gettime_ns();
    obs_source_t *source=a->weak?obs_weak_source_get_source(a->weak):NULL;
    if(a->weak && (!source || obs_source_removed(source))) {
        disconnect_source(a);obs_source_release(source);source=NULL;a->retry_time=0;
    }
    if(!a->meter && now>=a->retry_time) {
        connect_source(a,now);
        source=a->weak?obs_weak_source_get_source(a->weak):NULL;
    }
    uint64_t sample=atomic_load_explicit(&a->sample_time,memory_order_acquire);
    float target=0,linear_target=0;
    if(source && obs_source_enabled(source) && !obs_source_muted(source) && sample && now-sample<250000000ULL) {
        float db=atomic_load(&a->peak_mdb)*.001f+a->gain;
        target=fminf(1,fmaxf(0,(db-a->threshold)/-a->threshold));
        float magnitude_db=atomic_load(&a->magnitude_mdb)*.001f+a->gain;
        if(magnitude_db>a->threshold)linear_target=fminf(1,powf(10,magnitude_db/20));
    }
    obs_source_release(source);
    float dt=isfinite(seconds)?fminf(1,fmaxf(0,seconds)):0;
    float tau=target>a->envelope?a->attack:a->release;
    a->envelope=target+(a->envelope-target)*expf(-dt/tau);
    float linear_tau=linear_target>a->linear_envelope?a->attack:a->release;
    a->linear_envelope=linear_target+(a->linear_envelope-linear_target)*expf(-dt/linear_tau);
    if(a->linear_envelope<0.0001f && linear_target==0)a->linear_envelope=0;
    if(a->envelope<0.0001f && target==0) a->envelope=0;
    return a->depth*a->envelope;
}
void reactivity_destroy(struct reactivity *a) {disconnect_source(a);bfree(a->uuid);a->uuid=NULL;}
static bool enumerate(void *data,obs_source_t *source)
{
    if(!obs_source_removed(source) && (obs_source_get_output_flags(source)&OBS_SOURCE_AUDIO))
        obs_property_list_add_string(data,obs_source_get_name(source),obs_source_get_uuid(source));
    return true;
}
static void fill_sources(obs_property_t *list,obs_data_t *s)
{
    obs_property_list_clear(list);obs_property_list_add_string(list,obs_module_text("T089"),"");
    obs_enum_sources(enumerate,list);
    const char *uuid=obs_data_get_string(s,"audio_source");
    if(*uuid) {
        bool found=false;
        for(size_t i=0;i<obs_property_list_item_count(list);i++)
            if(!strcmp(uuid,obs_property_list_item_string(list,i))) found=true;
        if(!found) obs_property_list_add_string(list,obs_module_text("T090"),uuid);
    }
}
static bool refresh(obs_properties_t *p,obs_property_t *prop,void *data)
{
    UNUSED_PARAMETER(prop);
    if(!data) return false;
    obs_data_t *s=obs_source_get_settings(data);
    fill_sources(obs_properties_get(p,"audio_source"),s);obs_data_release(s);return true;
}
void reactivity_properties(obs_properties_t *p,obs_data_t *s,obs_source_t *owner)
{
    obs_properties_add_bool(p,"audio_enabled",obs_module_text("T091"));
    obs_property_t *list=obs_properties_add_list(p,"audio_source",obs_module_text("T092"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_STRING);
    if(s) fill_sources(list,s);else obs_property_list_add_string(list,obs_module_text("T089"),"");
    obs_properties_add_button2(p,"audio_refresh",obs_module_text("T093"),refresh,owner);
    obs_properties_add_float_slider(p,"audio_depth",obs_module_text("T094"),0,1,.01);
    obs_properties_add_float_slider(p,"audio_gain",obs_module_text("T095"),-24,24,.5);
    obs_properties_add_float_slider(p,"audio_threshold",obs_module_text("T096"),-80,-6,1);
    obs_properties_add_float_slider(p,"audio_attack",obs_module_text("T097"),1,1000,1);
    obs_properties_add_float_slider(p,"audio_release",obs_module_text("T098"),1,3000,1);
    obs_properties_add_text(p,"audio_info",obs_module_text("T099"),OBS_TEXT_INFO);
}
