/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../src/cube.h"
#include <stdio.h>
#include <locale.h>
#include <string.h>
static int failures;
static void test(const char *directory,const char *file,bool expected,unsigned size)
{
    char path[2048];snprintf(path,sizeof(path),"%s/%s",directory,file);
    struct cube_lut lut;const char *error;
    bool ok=cube_load(path,&lut,&error);
    bool pass=ok==expected && (!ok || lut.size==size) && (ok || (!lut.rgba && !lut.size));
    printf("%s: %s\n",file,pass?"PASS":"FAIL");failures+=!pass;cube_free(&lut);
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    test(argv[1],"identity.cube",true,2);test(argv[1],"identity17.cube",true,17);
    test(argv[1],"identity65.cube",true,65);test(argv[1],"domain.cube",true,2);
    test(argv[1],"comments.cube",true,2);test(argv[1],"identidad áé.cube",true,2);
    const char *invalid[]={"missing.cube","size1.cube","size66.cube","fractional.cube","near-integer.cube","duplicate.cube",
        "incomplete.cube","extra.cube","nan.cube","infinity.cube","hex.cube","huge-number.cube",
        "extra-number.cube","bad-domain.cube","same-domain.cube","late-domain.cube","duplicate-domain.cube",
        "one-dimensional.cube","unknown.cube","empty.cube","long-line.cube","bad-title.cube","comma.cube",
        "shaper.cube","oversize.cube"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) test(argv[1],invalid[i],false,0);
    /* Decimal points remain C-locale numbers even in a Spanish application. */
    if(setlocale(LC_NUMERIC,"Spanish_Spain.1252")) test(argv[1],"square.cube",true,3);
    else { puts("Spanish locale unavailable"); failures++; }
    printf("RESULT: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
