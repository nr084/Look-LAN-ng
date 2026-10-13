#ifndef LAN_TEXT_H
#define LAN_TEXT_H
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
/* Reject oversized text rather than silently truncating an identifier. */
static inline bool lan_text_copy(char *out,size_t capacity,const char *in){
    if(!out || !in || !capacity)return false;
    size_t length=0;while(length<capacity && in[length])++length;
    if(length==capacity)return false;
    memmove(out,in,length+1);return true;
}
#endif
