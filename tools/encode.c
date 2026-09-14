/* Offline LDAC encoder prototype. Does not send Bluetooth audio. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ldacBT.h"

/* Diagnostic records: LE u16 byte count, LE u16 frame count, encoded bytes.
   These are NOT AVDTP packets or a playable media container. */
static int record(FILE *f, unsigned char *data, int size, int frames) {
    unsigned char h[4];
    if (size <= 0 || size > 1024 || frames <= 0 || frames > 65535) return -1;
    h[0]=(unsigned char)size; h[1]=(unsigned char)(size>>8);
    h[2]=(unsigned char)frames; h[3]=(unsigned char)(frames>>8);
    return fwrite(h,1,4,f)==4 && fwrite(data,1,(size_t)size,f)==(size_t)size ? 0 : -1;
}
int main(int argc, char **argv) {
    FILE *input=NULL, *output=NULL;
    HANDLE_LDAC_BT encoder=NULL;
    unsigned char pcm[LDACBT_ENC_LSU*2*2], encoded[1024];
    int result=1, quality, used, size, frames, done=0, flushes=0;
    size_t bytes, total=0;
    if (argc!=4) {
        fprintf(stderr,"Usage: %s INPUT.s16le OUTPUT.ldacdiag hq|sq|mq\n"
            "Input: raw 48000 Hz stereo signed 16-bit little-endian PCM.\n"
            "Output: diagnostic LDAC records, not a playable media file.\n",argv[0]);
        return 2;
    }
    if (!strcmp(argv[3],"hq")) quality=LDACBT_EQMID_HQ;
    else if (!strcmp(argv[3],"sq")) quality=LDACBT_EQMID_SQ;
    else if (!strcmp(argv[3],"mq")) quality=LDACBT_EQMID_MQ;
    else { fprintf(stderr,"Invalid quality.\n"); return 2; }
    if (!strcmp(argv[1],argv[2])) {
        fprintf(stderr,"Input and output must be different files.\n"); return 2;
    }
    input=fopen(argv[1],"rb");
    if (!input) { perror("Input"); goto cleanup; }
    /* Exclusive creation also protects input aliases and existing outputs. */
    output=fopen(argv[2],"wbx");
    if (!output) { perror("Output (must not already exist)"); goto cleanup; }
    encoder=ldacBT_get_handle();
    if (!encoder) { fprintf(stderr,"Allocation failed.\n"); goto cleanup; }
    /* Test MTU only. Real streaming must use the negotiated transport MTU. */
    if (ldacBT_init_handle_encode(encoder,679,quality,LDACBT_CHANNEL_MODE_STEREO,
                                 LDACBT_SMPL_FMT_S16,48000)!=0) goto codec_error;
    for (;;) {
        void *block=NULL;
        if (!done) {
            bytes=fread(pcm,1,sizeof(pcm),input);
            if (ferror(input)) { perror("Read"); goto cleanup; }
            if (bytes%4) {
                fprintf(stderr,"Incomplete stereo sample at end of input.\n"); goto cleanup;
            }
            if (bytes) {
                memset(pcm+bytes,0,sizeof(pcm)-bytes);
                block=pcm; total+=bytes;
            }
            if (bytes<sizeof(pcm)) done=1;
        }
        used=size=frames=0;
        if (ldacBT_encode(encoder,block,&used,encoded,&size,&frames)!=0) goto codec_error;
        if (block && used!=(int)sizeof(pcm)) {
            fprintf(stderr,"Unexpected PCM consumption: %d.\n",used); goto cleanup;
        }
        if (size && record(output,encoded,size,frames)) {
            fprintf(stderr,"Write failed or invalid encoder output.\n"); goto cleanup;
        }
        if (!block) {
            if (!size) break;
            if (++flushes>32) { fprintf(stderr,"Encoder failed to drain.\n"); goto cleanup; }
        }
    }
    fprintf(stderr,"Encoded %zu input bytes; reported bitrate %d kbps.\n",
            total,ldacBT_get_bitrate(encoder));
    result=0;
    goto cleanup;
codec_error:
    fprintf(stderr,"LDAC error: 0x%x\n",ldacBT_get_error_code(encoder));
cleanup:
    if (encoder) ldacBT_free_handle(encoder);
    if (input) fclose(input);
    if (output && fclose(output)) result=1;
    if (output && result) remove(argv[2]);
    return result;
}
