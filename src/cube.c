/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "cube.h"
#include <sys/stat.h>
#include <util/platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>

void cube_free(struct cube_lut *lut) { free(lut->rgba); memset(lut,0,sizeof(*lut)); }
static char *space(char *p) { while(isspace((unsigned char)*p)) p++; return p; }
static bool numbers(char *p, float *values, unsigned count, _locale_t locale)
{
    for(unsigned i=0;i<count;i++) {
        p=space(p);
        /* Restrict to decimal CUBE syntax; reject hex, NaN and infinity. */
        char *start=p;
        if(*p=='+' || *p=='-') p++;
        bool digits=false;
        while(isdigit((unsigned char)*p)) { digits=true; p++; }
        if(*p=='.') { p++; while(isdigit((unsigned char)*p)) { digits=true; p++; } }
        if(!digits) return false;
        if(*p=='e' || *p=='E') {
            p++; if(*p=='+' || *p=='-') p++;
            if(!isdigit((unsigned char)*p)) return false;
            while(isdigit((unsigned char)*p)) p++;
        }
        if(*p && !isspace((unsigned char)*p)) return false;
        char *end=NULL;
        double v=_strtod_l(start,&end,locale);
        if(end!=p || !isfinite(v) || fabs(v)>65504.0) return false;
        values[i]=(float)v;
    }
    return *space(p)==0;
}
bool cube_load(const char *path, struct cube_lut *out, const char **error)
{
    memset(out,0,sizeof(*out));
    *error="Archivo LUT no disponible";
    FILE *file=os_fopen(path,"rb");
    if(!file) return false;
    struct cube_lut lut={.domain_min={0,0,0},.domain_max={1,1,1}};
    _locale_t locale=_create_locale(LC_NUMERIC,"C");
    bool ok=false, seen_min=false, seen_max=false, seen_title=false;
    size_t count=0,total=0,line_number=0;
    char line[1024];
    if(!locale) { *error="No se pudo preparar el lector LUT"; goto done; }
    if(fseek(file,0,SEEK_END)!=0) goto done;
    long length=ftell(file);
    if(length<0 || length>32*1024*1024) { *error="LUT demasiado grande (máximo 32 MiB)"; goto done; }
    rewind(file);
    while(fgets(line,sizeof(line),file)) {
        line_number++;
        if(!strchr(line,'\n') && !feof(file)) { *error="Línea LUT demasiado larga"; goto done; }
        char *p=line;
        if(line_number==1 && strlen(p)>=3 && (unsigned char)p[0]==0xef &&
           (unsigned char)p[1]==0xbb && (unsigned char)p[2]==0xbf) p+=3;
        p=space(p);
        if(!*p || *p=='#') continue;
        if(!strncmp(p,"TITLE",5) && isspace((unsigned char)p[5])) {
            if(count || seen_title) { *error="Cabecera TITLE duplicada o fuera de orden"; goto done; }
            char *title=space(p+5);
            if(*title!='"') { *error="TITLE debe ir entre comillas"; goto done; }
            char *last=strrchr(title+1,'"');
            if(!last || (*space(last+1) && *space(last+1)!='#')) { *error="TITLE no válido"; goto done; }
            seen_title=true; continue;
        }
        char *comment=strchr(p,'#'); if(comment) *comment=0;
        if(!strncmp(p,"LUT_1D_SIZE",11) || !strncmp(p,"LUT_1D_INPUT_RANGE",18) ||
           !strncmp(p,"LUT_3D_INPUT_RANGE",18)) {
            *error="Formato no compatible: use LUT 3D con DOMAIN_MIN y DOMAIN_MAX"; goto done;
        }
        if(!strncmp(p,"LUT_3D_SIZE",11) && isspace((unsigned char)p[11])) {
            char *digits=space(p+11); unsigned size=0;
            bool integer=isdigit((unsigned char)*digits)!=0;
            while(isdigit((unsigned char)*digits)) {
                size=size*10+(unsigned)(*digits++-'0');
                if(size>65) { integer=false; break; }
            }
            if(lut.size || count || !integer || *space(digits) || size<2 || size>65) {
                *error="LUT_3D_SIZE debe aparecer una vez y estar entre 2 y 65"; goto done;
            }
            lut.size=size; total=(size_t)lut.size*lut.size*lut.size;
            lut.rgba=calloc(total*4,sizeof(float));
            if(!lut.rgba) { *error="Sin memoria para cargar LUT"; goto done; }
            continue;
        }
        bool is_min=!strncmp(p,"DOMAIN_MIN",10) && isspace((unsigned char)p[10]);
        bool is_max=!strncmp(p,"DOMAIN_MAX",10) && isspace((unsigned char)p[10]);
        if(is_min || is_max) {
            if(count || (is_min?seen_min:seen_max) ||
               !numbers(p+10,is_min?lut.domain_min:lut.domain_max,3,locale)) {
                *error="Dominio LUT no válido o duplicado"; goto done;
            }
            if(is_min) seen_min=true; else seen_max=true;
            continue;
        }
        if(!lut.size || count>=total || !numbers(p,lut.rgba+count*4,3,locale)) {
            *error="Datos LUT no válidos o cantidad de filas incorrecta"; goto done;
        }
        lut.rgba[count*4+3]=1; count++;
    }
    if(ferror(file)) { *error="Error al leer LUT"; goto done; }
    if(!lut.size || count!=total) { *error="LUT incompleta: se requieren exactamente N³ filas"; goto done; }
    for(int c=0;c<3;c++) {
        if(lut.domain_max[c]-lut.domain_min[c]<0.000001f) {
            *error="DOMAIN_MAX debe ser mayor que DOMAIN_MIN"; goto done;
        }
    }
    *out=lut; memset(&lut,0,sizeof(lut)); *error="Formato válido: LUT 3D"; ok=true;
done:
    cube_free(&lut); if(locale) _free_locale(locale); fclose(file); return ok;
}

