/* Offline LDAC encoder prototype. Does not send Bluetooth audio. */
#include <stdio.h>
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
static void usage(FILE *stream, const char *name) {
    fprintf(stream,"Usage: %s INPUT.pcm OUTPUT.ldacdiag hq|sq|mq [--rate 44100|48000|88200|96000] [--format s16|s24|s32]\n"
        "Input: raw stereo signed little-endian PCM (default: 48000 Hz s16).\n"
        "s24 samples use packed 3-byte little-endian values.\n"
        "Output: diagnostic LDAC records, not a playable media file.\n",name);
}
int main(int argc, char **argv) {
    FILE *input=NULL, *output=NULL;
    HANDLE_LDAC_BT encoder=NULL;
    unsigned char pcm[LDACBT_ENC_LSU*2*4], encoded[1024];
    int result=1, quality, used, size, frames, done=0, flushes=0;
    int sample_rate=48000, sample_bytes=2, rate_seen=0, format_seen=0;
    LDACBT_SMPL_FMT_T sample_format=LDACBT_SMPL_FMT_S16;
    size_t bytes, total=0, block_bytes, frame_bytes;
    if (argc==2 && !strcmp(argv[1],"--help")) { usage(stdout,argv[0]); return 0; }
    if (argc<4 || argc>8 || (argc-4)%2) {
        usage(stderr,argv[0]);
        return 2;
    }
    if (!strcmp(argv[3],"hq")) quality=LDACBT_EQMID_HQ;
    else if (!strcmp(argv[3],"sq")) quality=LDACBT_EQMID_SQ;
    else if (!strcmp(argv[3],"mq")) quality=LDACBT_EQMID_MQ;
    else { fprintf(stderr,"Invalid quality.\n"); return 2; }
    for (int i=4;i<argc;i+=2) {
        if (!strcmp(argv[i],"--rate") && !rate_seen) {
            if (!strcmp(argv[i+1],"44100")) sample_rate=44100;
            else if (!strcmp(argv[i+1],"48000")) sample_rate=48000;
            else if (!strcmp(argv[i+1],"88200")) sample_rate=88200;
            else if (!strcmp(argv[i+1],"96000")) sample_rate=96000;
            else { fprintf(stderr,"Unsupported sample rate.\n"); return 2; }
            rate_seen=1;
        } else if (!strcmp(argv[i],"--format") && !format_seen) {
            if (!strcmp(argv[i+1],"s16")) { sample_bytes=2;sample_format=LDACBT_SMPL_FMT_S16; }
            else if (!strcmp(argv[i+1],"s24")) { sample_bytes=3;sample_format=LDACBT_SMPL_FMT_S24; }
            else if (!strcmp(argv[i+1],"s32")) { sample_bytes=4;sample_format=LDACBT_SMPL_FMT_S32; }
            else { fprintf(stderr,"Unsupported PCM format.\n"); return 2; }
            format_seen=1;
        } else { fprintf(stderr,"Unknown or duplicate option.\n"); return 2; }
    }
    frame_bytes=(size_t)sample_bytes*2;
    block_bytes=LDACBT_ENC_LSU*frame_bytes;
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
                                 sample_format,sample_rate)!=0) goto codec_error;
    for (;;) {
        void *block=NULL;
        if (!done) {
            bytes=fread(pcm,1,block_bytes,input);
            if (ferror(input)) { perror("Read"); goto cleanup; }
            if (bytes%frame_bytes) {
                fprintf(stderr,"Incomplete stereo sample at end of input.\n"); goto cleanup;
            }
            if (bytes) {
                memset(pcm+bytes,0,block_bytes-bytes);
                block=pcm; total+=bytes;
            }
            if (bytes<block_bytes) done=1;
        }
        used=size=frames=0;
        if (ldacBT_encode(encoder,block,&used,encoded,&size,&frames)!=0) goto codec_error;
        if (block && used!=(int)block_bytes) {
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
