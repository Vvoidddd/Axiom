#include "picture.h"
#include "vfs.h"
#include "memory.h"
static uint32_t u32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static unsigned u16(const uint8_t*p){return p[0]|(unsigned)p[1]<<8;}
static bool read_exact(int fd,void*buffer,size_t bytes){uint8_t*out=buffer;while(bytes){long n=vfs_read(fd,out,bytes);if(n<=0)return false;out+=n;bytes-=(size_t)n;}return true;}
void picture_close(struct picture*p){if(p){kfree(p->pixels);*p=(struct picture){0};}}
int picture_load(const char*path,struct picture*p){
    if(!p)return VFS_EINVAL;
    struct vfs_stat st;int error=vfs_stat_path(path,&st);if(error)return error;
    if(st.directory)return VFS_EISDIR;
    if(st.size<54||st.size>1024*1024)return VFS_EINVAL;
    int fd=vfs_open(path,VFS_READ);if(fd<0)return fd;
    uint8_t header[54],*row=0;struct picture next={0};error=VFS_EINVAL;
    if(!read_exact(fd,header,sizeof(header)))goto done;
    uint32_t size=u32(header+2),offset=u32(header+10),width=u32(header+18),raw_height=u32(header+22);
    bool top_down=(raw_height&0x80000000u)!=0;uint32_t height=top_down?0u-raw_height:raw_height;
    if(header[0]!='B'||header[1]!='M'||u32(header+14)!=40||u16(header+26)!=1||u16(header+28)!=24||u32(header+30)||!width||width>2048||!height||height>2048||width>262144/height)goto done;
    uint32_t stride=(width*3+3)&~3u;
    if(offset<54||offset>st.size||size>st.size||size<offset||stride*height>size-offset)goto done;
    next=(struct picture){.width=width,.height=height,.pixels=kmalloc((size_t)width*height*4)};row=kmalloc(stride);
    if(!next.pixels||!row){error=VFS_ENOSPC;goto done;}
    if(vfs_seek(fd,offset)<0){error=VFS_EIO;goto done;}
    for(uint32_t y=0;y<height;y++){
        if(!read_exact(fd,row,stride)){error=VFS_EIO;goto done;}
        uint32_t*to=next.pixels+(top_down?y:height-1-y)*width;
        for(uint32_t x=0;x<width;x++)to[x]=(uint32_t)row[x*3+2]<<16|(uint32_t)row[x*3+1]<<8|row[x*3];
    }
    picture_close(p);*p=next;next.pixels=0;error=0;
done:
    vfs_close(fd);kfree(row);picture_close(&next);return error;
}
