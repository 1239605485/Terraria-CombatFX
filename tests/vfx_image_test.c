#include <assert.h>
#include <stdio.h>
#include "vfx_image.h"
int main(int argc,char **argv) {
    assert(argc == 2);
    const char *names[]={"slash-tbbk.png"};
    size_t total=0;
    for(int i=0;i<1;i++) {
        char path[2048]; snprintf(path,sizeof(path),"%s/%s",argv[1],names[i]);
        FxImage image; assert(fx_image_load(path,256,&image));
        assert(image.width==192 && image.height==141);
        assert(image.size==(size_t)image.width*image.height*4);
        int clear=0,visible=0;
        for(size_t j=0;j<image.size;j+=4) {
            unsigned char *p=image.pixels+j;
            assert(p[0]<=p[3] && p[1]<=p[3] && p[2]<=p[3]);
            clear += p[3]==0; visible += p[3]>50;
        }
        assert(clear && visible); total+=image.size;
        fx_image_free(&image);
    }
    assert(total<512*1024);
    FxImage missing; assert(!fx_image_load("/no-such-combatfx-texture.png",256,&missing));
    return 0;
}
