/* Integration test: the actual native DLL + OBS 32.2.2 + Direct3D 11. */
#include <obs.h>
#include <graphics/vec4.h>
#include <sys/stat.h>
#include <util/platform.h>
#include <util/bmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


static uint32_t test_w = 512, test_h = 320;
static const char *input_rgba = NULL;

struct fixture { gs_texture_t *texture; uint32_t w, h; uint8_t *rgba; };
static struct fixture *last_fixture;

static const char *fixture_name(void *p) { UNUSED_PARAMETER(p); return "NEXUS test fixture"; }
static void *fixture_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings); UNUSED_PARAMETER(source);
    struct fixture *f = bzalloc(sizeof(*f));
    f->w = test_w; f->h = test_h;
    f->rgba = bmalloc(f->w*f->h*4);
    for (uint32_t y = 0; y < f->h; y++) for (uint32_t x = 0; x < f->w; x++) {
        size_t i = ((size_t)y*f->w+x)*4;
        float nx = ((float)x/f->w - .5f)*2.f, ny = ((float)y/f->h - .5f)*2.f;
        float r = sqrtf(nx*nx + ny*ny);
        float shade = fmaxf(0.f, 1.f-r*.8f);
        f->rgba[i] = (uint8_t)(40 + 205*shade);
        f->rgba[i+1] = (uint8_t)(35 + 175*shade*shade);
        f->rgba[i+2] = (uint8_t)(30 + 60*shade);
        unsigned a = (f->w == 1 && f->h == 1) ? 255 : r < .72f ? 255 : r > .92f ? 0 : (unsigned)(255*(.92f-r)/.2f);
        if (y > f->h*3/4) a = x % 256; // all 256 alpha levels
        if (y > f->h/3 && y < f->h/2 && x > f->w/3 && x < f->w*2/3) {
            f->rgba[i] = 25; f->rgba[i+1] = 80; f->rgba[i+2] = 170;
        }
        f->rgba[i+3] = (uint8_t)a;
    }
    if (input_rgba) {
        FILE *file = fopen(input_rgba,"rb");
        if (!file || fread(f->rgba,1,(size_t)f->w*f->h*4,file) != (size_t)f->w*f->h*4) exit(13);
        fclose(file);
    }
    obs_enter_graphics();
    const uint8_t *pixels = f->rgba;
    f->texture = gs_texture_create(f->w, f->h, GS_RGBA, 1, &pixels, 0);
    obs_leave_graphics();

    last_fixture = f;
    return f;
}
static void fixture_destroy(void *p)
{
    struct fixture *f = p;
    obs_enter_graphics(); gs_texture_destroy(f->texture); obs_leave_graphics();
    bfree(f->rgba); bfree(f);
}
static uint32_t fixture_width(void *p) { return ((struct fixture *)p)->w; }
static uint32_t fixture_height(void *p) { return ((struct fixture *)p)->h; }
static void fixture_render(void *p, gs_effect_t *effect)
{
    struct fixture *f = p;
    gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), f->texture);
    gs_draw_sprite(f->texture, 0, f->w, f->h);
}
static struct obs_source_info fixture_info = {
    .id = "nexus_test_fixture", .type = OBS_SOURCE_TYPE_INPUT, .output_flags = OBS_SOURCE_VIDEO,
    .get_name = fixture_name, .create = fixture_create, .destroy = fixture_destroy,
    .get_width = fixture_width, .get_height = fixture_height, .video_render = fixture_render
};

static uint8_t *render(obs_source_t *source, uint32_t w, uint32_t h)
{
    uint8_t *result = malloc(w*h*4);
    obs_enter_graphics();
    gs_texrender_t *rt = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
    gs_stagesurf_t *stage = gs_stagesurface_create(w,h,GS_RGBA);
    if (!gs_texrender_begin(rt,w,h)) exit(10);
    struct vec4 zero; vec4_zero(&zero);
    gs_clear(GS_CLEAR_COLOR,&zero,0,0);
    gs_ortho(0,(float)w,0,(float)h,-100,100);
    gs_blend_state_push();
    gs_enable_blending(true);
    gs_enable_depth_test(false);
    gs_set_cull_mode(GS_NEITHER);
    gs_set_linear_srgb(false);
    gs_enable_framebuffer_srgb(false);
    gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
    obs_source_video_render(source);
    gs_blend_state_pop();
    gs_texrender_end(rt);
    gs_stage_texture(stage,gs_texrender_get_texture(rt));
    uint8_t *mapped = NULL; uint32_t stride = 0;
    if (!gs_stagesurface_map(stage,&mapped,&stride)) exit(11);
    for (uint32_t y = 0; y < h; y++) memcpy(result+y*w*4,mapped+y*stride,w*4);
    gs_stagesurface_unmap(stage);
    gs_stagesurface_destroy(stage); gs_texrender_destroy(rt);
    obs_leave_graphics();
    return result;
}

void clear_history(obs_source_t *filter)
{
    obs_properties_t *props=obs_source_properties(filter);
    obs_property_button_clicked(obs_properties_get(props,"reset"),filter);
    obs_properties_destroy(props);
}

static obs_source_t *manual_filter(const char *label)
{
    obs_data_t *settings=obs_data_create();
    obs_data_set_bool(settings,"audio_enabled",false);
    obs_source_t *filter=obs_source_create_private("nexus_reactive_echo",label,settings);
    obs_data_release(settings);return filter;
}

int main(int argc, char **argv)
{
    if(argc!=3) return 2;
    char data[2048], graphics[2048];
    snprintf(data,sizeof(data),"%s/data/libobs/",argv[1]);
    snprintf(graphics,sizeof(graphics),"%s/bin/64bit/libobs-d3d11.dll",argv[1]);
    if(!obs_startup("es-ES",NULL,NULL)) return 3;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    obs_add_data_path(data);
#pragma clang diagnostic pop
    struct obs_video_info vi = {
        .graphics_module=graphics,.fps_num=30,.fps_den=1,
        .base_width=512,.base_height=320,.output_width=512,.output_height=320,
        .output_format=VIDEO_FORMAT_RGBA,.adapter=0,.gpu_conversion=false,
        .colorspace=VIDEO_CS_709,.range=VIDEO_RANGE_FULL,.scale_type=OBS_SCALE_BILINEAR
    };
    if(obs_reset_video(&vi)!=OBS_VIDEO_SUCCESS) return 4;
    obs_module_t *module=NULL;
    if(obs_open_module(&module,argv[2],"./plugin")!=MODULE_SUCCESS || !obs_init_module(module)) return 5;
    obs_register_source(&fixture_info);
    int failures=0;
    for(int size=0;size<3;size++) {
        test_w=size==0?512:size==1?257:1; test_h=size==0?320:size==1?73:1;
        obs_source_t *source=obs_source_create_private("nexus_test_fixture","Fixture",NULL);
        obs_source_t *filter=manual_filter("NEXUS");
        if(!source || !filter) return 6;
        uint8_t *base=render(source,test_w,test_h);
        unsigned nonzero=0;
        for(size_t i=0;i<(size_t)test_w*test_h*4;i++) nonzero+=base[i]!=0;
        if(!nonzero) failures++;
        obs_source_filter_add(source,filter);
        for(int state=0;state<4;state++) {
            obs_source_set_enabled(filter,state!=1);
            if(state==3) { obs_source_filter_remove(source,filter); obs_source_filter_add(source,filter); }
            uint8_t *out=render(source,test_w,test_h);
            int mismatch=memcmp(base,out,(size_t)test_w*test_h*4)!=0;
            printf("size=%ux%u state=%d RGBA_identical=%s\n",test_w,test_h,state,mismatch?"false":"true");
            failures+=mismatch; free(out);
        }
        if(obs_source_get_width(source)!=test_w || obs_source_get_height(source)!=test_h) failures++;
        free(base); obs_source_filter_remove(source,filter);
        obs_source_release(filter); obs_source_release(source);
    }
    obs_shutdown();
    printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}


