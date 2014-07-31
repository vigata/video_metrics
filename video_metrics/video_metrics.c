/*
 * Copyright (c) 2014, Alberto Vigata ( http://github.com/concalma )
 * All rights reserved.
 *
 * The BSD License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * - Neither the name of the tdistler.com nor the names of its contributors may
 *   be used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <inttypes.h>
#include "iqa/iqa.h"


typedef uint8_t pixel8;

typedef struct {
    int w;
    int h;
    int stride; // stride in bytes
    int planesize;
    pixel8 *buf;
} plane;

typedef struct {
    int w;
    int h;
    plane *planes[3];
    int format;
    int size; // size in disk of pic
} picture_t;

typedef struct {
    char *name;
    double ( *metric_fun)(pixel8 *ref, pixel8 *rec, int w, int h, int stride);
    double accum;
    double val; // last value
    double avg; // average
    int frames;
    int used;
} metric_t;

typedef struct {
    metric_t *metrics[10];
    int metric_count;
} metrics_t;

static double psnr_metric(pixel8 *ref, pixel8 *rec, int w, int h, int stride) {
    return iqa_psnr((unsigned char *)ref, (unsigned char *)rec, w, h, stride );
}

static double ssim_metric(pixel8 *ref, pixel8 *rec, int w, int h, int stride) {
    return iqa_ssim((unsigned char *)ref, (unsigned char *)rec, w, h, stride, 1, NULL );
}

static double ms_ssim_metric(pixel8 *ref, pixel8 *rec, int w, int h, int stride) {
    return iqa_ms_ssim((unsigned char *)ref, (unsigned char *)rec, w, h, stride, NULL );
}

static double mse_metric(pixel8 *ref, pixel8 *rec, int w, int h, int stride) {
    return iqa_mse((unsigned char *)ref, (unsigned char *)rec, w, h, stride );
}

#define YUV420 1
#define YUV422 2

static picture_t *picture_alloc( int format, int w, int h ) {
    int i;
    picture_t *ret;
    switch(format) {
        case YUV420:
            ret = calloc(sizeof (picture_t), 1);
            ret->w = w;
            ret->h = h;
            ret->format = format;
            ret->size = 0;
            for( i=0; i<3; i++ ) {
                plane *p = ret->planes[i] = calloc( sizeof(plane), 1);
                if(i==0) {
                    // luma plane
                    p->w = w;
                    p->h = h;
                    p->stride = w;
                    p->buf = calloc(w*h,1);

                } else {
                    // chrome plane
                    p->w = w/2;
                    p->h = h/2;
                    p->stride = w/2;
                    p->buf = calloc(w*h/4,1);
                }
                p->planesize = p->w*p->h;
                ret->size += p->planesize;
            }
            break;
        default:
            break;
    }
    
    return ret;
}

static void picture_free( picture_t *pic) {
    if(!pic) return;
    
    for(int i=0; i<3; i++) {
        free( pic->planes[i]->buf );
        free(pic->planes[i] );
    }
    free(pic);
}

// returns 1 on success. 0 on failure
static int picture_read( FILE *f, picture_t *pic ) {
    if(!pic || !f) return 0;
    int planeread = 0;
    
    switch(pic->format) {
        case YUV420:
            planeread += fread( pic->planes[0]->buf, pic->planes[0]->planesize, 1, f);
            planeread += fread( pic->planes[1]->buf, pic->planes[1]->planesize, 1, f);
            planeread += fread( pic->planes[2]->buf, pic->planes[2]->planesize, 1, f);
            break;
    }
    
    return planeread >=3 ? 1 : 0;
}

// calculates psnr between two pictures
static void picture_metric( metrics_t *metrics, picture_t *a, picture_t *b, int verbose ) {
    for( int i=0; i<metrics->metric_count; i++ ) {
        metrics->metrics[i]->val = 0;
    }

    double m;
    switch (a->format) {
        case YUV420:
            for(int j=0; j<metrics->metric_count; j++ ) {
                if( metrics->metrics[j]->used ) {
                    double marr[3];
                    for(int i=0; i<3; i++) {
                        marr[i]= metrics->metrics[j]->metric_fun(a->planes[i]->buf, b->planes[i]->buf, a->planes[i]->w, a->planes[i]->h, a->planes[i]->stride );
                        metrics->metrics[j]->val += marr[i] * a->planes[i]->planesize;
                    }
                    metrics->metrics[j]->val /= a->size;
                    metrics->metrics[j]->accum += metrics->metrics[j]->val;
                    metrics->metrics[j]->frames++;
                    
                    if(verbose) printf("%d(%s): %.6f  Y:%.6f U:%.6f V:%.6f\n", metrics->metrics[j]->frames, metrics->metrics[j]->name, metrics->metrics[j]->val, marr[0],marr[1], marr[2] );
                }
            }

            break;
        default:
            break;
    }
    return;
}

static void picture_metric_finalize( metrics_t *metrics ) {
    for(int j=0; j<metrics->metric_count; j++ ) {
        for(int i=0; i<3; i++) {
            if( metrics->metrics[j]->used ) {
                metrics->metrics[j]->avg = metrics->metrics[j]->accum / (double) metrics->metrics[j]->frames;
            }
        }
        if( metrics->metrics[j]->used )
            printf("%s: %.6f\n", metrics->metrics[j]->name, metrics->metrics[j]->avg );
    }
}


void print_usage(metrics_t *metrics) {
    printf("video_metrics -a ref.yuv -b rec.yuv -m [psnr|ssim|ms_ssim|psnr,ssim|psnr,ssim,ms_ssim|...] -w width -h height -f format\n\n");
    printf("Available metrics:\n");
    for( int i=0; i<metrics->metric_count; i++ ) {
        printf("\t%s\n", metrics->metrics[i]->name );
    }
}


int main(int argc, char * argv[])
{
    int c;

    // a: input a
    // b: input b
    // m: metrics to use 'ssim' or/and 'psnr'
    // w: width
    // h: height
    // f: format of yuv
    // v: verbose
    // x: abort at nnnn frames
    opterr = 0;
    
    char *fname_a=NULL, *fname_b=NULL;
  
    int width=0, height=0;
    int format=YUV420;
    int verbose = 0;
    int ismet = 0;
    int max_frames = 9999999;
    int frame = 0;
    
    metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics_t));
    
    
    // available metrics are defined here
    metrics.metrics[0] = &((metric_t){"psnr",psnr_metric,0,0,0,0,0});
    metrics.metrics[1] = &((metric_t){"ssim",ssim_metric,0,0,0,0,0});
    metrics.metrics[2] = &((metric_t){"ms_ssim",ms_ssim_metric,0,0,0,0,0});
    metrics.metrics[3] = &((metric_t){"mse",mse_metric,0,0,0,0,0});
    metrics.metric_count = 4;
 
 
    
    while ((c = getopt (argc, argv, "a:b:m:w:h:f:vx:")) != -1)
        switch (c)
        {
            case 'a':
                fname_a = optarg;
                break;
            case 'b':
                fname_b = optarg;
                break;
            case 'm':
                for( int i=0; i<metrics.metric_count; i++ ) {
                    if(strstr(optarg, metrics.metrics[i]->name)) {
                        metrics.metrics[i]->used = 1;
                        ismet = 1;
                    }
                }
                break;
            case 'w':
                width = atoi(optarg);
                break;
            case 'h':
                height = atoi(optarg);
                break;
            case 'f':
                format = atoi(optarg);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'x':
                max_frames = atoi(optarg);
                break;
            case '?':
                if (isprint (optopt))
                    fprintf (stderr, "There is a problem with option '-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character '\\x%x'.\n", optopt);
            default:
                print_usage(&metrics);
                exit(1);
        }
    
    if( !width || !height || !format ) {
        print_usage(&metrics);
        fprintf(stderr, "There is a problem with one of the parameters");
        return 1;
    }

    if( !ismet ) {
        fprintf(stderr, "You must define at least one metric to use");
        return 1;
    }
    
    
    FILE *fa = fopen(fname_a, "rb");
    FILE *fb = fopen(fname_b, "rb");
    if( !fa || !fb ) {
        fprintf(stderr,"couldn't open files");
        return 1;
    }
    
    picture_t *pica= picture_alloc(format, width, height );
    picture_t *picb= picture_alloc(format, width, height );
    
    frame = 0;
    while( picture_read(fa, pica) && picture_read(fb, picb) && frame<max_frames) {
        picture_metric(&metrics, pica, picb, verbose);
        frame++;
    }
    picture_metric_finalize(&metrics);
    
    picture_free(pica);
    picture_free(picb);
    
    
    fclose(fa);
    fclose(fb);

    
    
    
    return 0;
}

