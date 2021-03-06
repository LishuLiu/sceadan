/*****************************************************************
 *
 *
 *Copyright (c) 2012-2013 The University of Texas at San Antonio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Copyright Holder:
 * University of Texas at San Antonio
 * One UTSA Circle 
 * San Antonio, Texas 78209
 */

/*
 *
 * Development History:
 *
 * 2014 - Significant refactoring and updating by:
 * Simson L. Garfinkel, Naval Postgraduate School
 *
 * 2013 - Created by:
 * Dr. Nicole Beebe and Lishu Liu, Department of Information Systems and Cyber Security (nicole.beebe@utsa.edu)
 * Laurence Maddox, Department of Computer Science
 * 
 */

#include "config.h"
#include <assert.h>
#include <ftw.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef HAVE_LINEAR_H
#include <linear.h>
#endif
#ifdef HAVE_LIBLINEAR_LINEAR_H
#include <liblinear/linear.h>
#endif



#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "file_type.h"


/* definitions. Some will be moved out of this file */

#define nbit_unigram (8)                // bits in a unigram
#define nbit_bigram  (16)               /* number of bits in a bigram */
#define n_unigram ((uint32_t) 1 << nbit_unigram) /* number of possible unigrams   = 2 ** 8 (needs at least 9 bits) */
#define n_bigram  ((uint32_t) 1 << nbit_bigram)  /* number of possible bigrams = 2 ** 16 (needs at least 17 bits) */

typedef uint8_t  unigram_t;  // unigram 
typedef uint16_t bigram_t;   /* bigram  - two consecutive unigrams;  TODO check whether endian-ness can be an issue */


/* low  ascii range is  0x00 <= char < 0x20 */
#define ASCII_LO_VAL (0x20)

/* mid  ascii range is  0x20 <= char < 0x80 */

/* high ascii range is   0x80 <= char */
#define ASCII_HI_VAL (0x80)

/* type for accumulators */
typedef unsigned long long sum_t;

/* type for summation/counting followed by floating point ops */
typedef union {
    sum_t  tot;
    double avg;
} cv_e;

/* unigram count vector map unigram to count, then to frequency
   implements the probability distribution function for unigrams TODO
   beware of overflow more implementations */
typedef cv_e ucv_t[n_unigram];

/* bigram count vector map bigram to count, then to frequency
   implements the probability distribution function for bigrams TODO
   beware of overflow more implementations */
typedef cv_e bcv_t[n_unigram][n_unigram];

typedef enum {
    ID_UNDEF = 0,
    ID_CONTAINER,
    ID_BLOCK
} id_e;

/* main feature vector */
typedef struct {
    const char *id_container;
    size_t      id_block;

    unigram_t const_chr[2];

    /* size of item */
    sum_t  uni_sz;
    //sum_t  bi_sz;

    /*  Feature Name: Bi-gram Entropy
        Description : Shannon's entropy
        Formula     : Sum of -P (v) lb (P (v)),
        over all bigram values (v)
        where P (v) = frequency of bigram value (v) */
    double bigram_entropy;

    /* Feature Name: Item Identifier
       Description : Shannon's entropy
       Formula     : Sum of -P (v) lb (P (v)),
       over all byte values (v)
       where P (v) = frequency of byte value (v) */
    double item_entropy;

    /* Feature Name: Hamming Weight
       Description : Total number of 1s divided by total bits in item */
    cv_e hamming_weight;

    /* Feature Name: Mean Byte Value
       Description : Sum of all byte values in item divided by total size in bytes
       Note        : Only go to EOF if item is an allocated file */
    cv_e byte_value;

    /* Feature Name: Standard Deviation of Byte Values
       Description : Typical standard deviation formula */
    cv_e stddev_byte_val;

    /* (from wikipedia:)
       the (average) absolute deviation of an element of a data set is the
       absolute difference between that element and ... a measure of central
       tendency
       TODO use median instead of mean */
    double abs_dev;

    /* (from wikipedia:)
       skewness is a measure of the asymmetry of the probability distribution of
       a real-valued random variable */
    double skewness;

    /* Feature Name: Kurtosis
       Description : Shows peakedness in the byte value distribution graph */
    double kurtosis;

    /* Feature Name: Compressed item length - bzip2
       Description : Uses Burrows-Wheeler algorithm; effective, slow */
    //cv_e bzip2_len;

    /* Feature Name: Compressed item length - LZW
       Description : Lempel-Ziv-Welch algorithm; fast */
    //cv_e lzw_len;

    /* Feature Name: Average contiguity between bytes
       Description : Average distance between consecutive byte values */
    cv_e contiguity;

    /* Feature Name: Max Byte Streak
       Description : Length of longest streak of repeating bytes in item
       TODO normalize ? */
    cv_e  max_byte_streak;

    // TODO longest common subsequence

    /* Feature Name: Low ASCII frequency
       Description : Frequency of bytes 0x00 .. 0x1F,
       normalized by item size */
    cv_e lo_ascii_freq;

    /* Feature Name: ASCII frequency
       Description : Frequency of bytes 0x20 .. 0x7F,
       normalized by item size */
    cv_e med_ascii_freq;

    /* Feature Name: High ASCII frequency
       Description : Frequency of bytes 0x80 .. 0xFF,
       normalized by item size */
    cv_e hi_ascii_freq;

    /* Feature Name: Byte Value Correlation
       Description : Correlation of the values of the n and n+1 bytes */
    double byte_val_correlation;

    /* Feature Name: Byte Value Frequency Correlation
       Description : Correlation of the frequencies of values of the m and m+1 bytes */
    double byte_val_freq_correlation;

    /* Feature Name: Unigram chi square
       Description : Test goodness-of-fit of unigram count vector relative to
       random distribution of byte values to see if distribution
       varies statistically significantly from a random
       distribution */
    double uni_chi_sq;
} mfv_t;

/* END TYPEDEFS FOR I/O */
struct sceadan_type_t {
    int code;
    const char *name;
};

extern struct sceadan_type_t sceadan_types[];
const char *sceadan_name_for_type(int i);

/* FUNCTIONS */
// TODO full path vs relevant path may matter
struct sceadan_vectors {
    ucv_t ucv;
    bcv_t bcv;
    mfv_t mfv;
    sum_t last_cnt;                     // for computing runs of characters
    uint8_t last_val;
    const char *file_name;                  /* if the vectors came from a file, indicate it here */
};
typedef struct sceadan_vectors sceadan_vectors_t;

#include "sceadan.h"


#define MODEL ("model")                 /* default model file */

#define RANDOMNESS_THRESHOLD (.995)     /* ignore things more random than this */
#define UCV_CONST_THRESHOLD  (.5)       /* ignore UCV more than this */
#define BCV_CONST_THRESHOLD  (.5)       /* ignore BCV more than this */


/* one of two master lists of types. This should be auto-generated from a file */
struct sceadan_type_t sceadan_types[] = {
    {0,"unclassified"},
    {A85,"a85"},
    {AES,"aes"},
    {ASPX,"aspx"},
    {AVI,"avi"},
    {B16,"b16"},
    {B64,"b64"},
    {BCV_CONST,"bcv_const"},
    {BMP,"bmp"},
    {BZ2,"bz2"},
    {CSS,"css"},
    {CSV,"csv"},
    {DLL,"dll"},
    {DOC,"doc"},
    {DOCX,"docx"},
    {ELF,"elf"},
    {EXE,"exe"},
    {EXT3,"ext3"},
    {FAT,"fat"},
    {FLV,"flv"},
    {GIF,"gif"},
    {GZ,"gz"},
    {HTML,"html"},
    {JAR,"jar"},
    {JAVA,"java"},
    {JB2,"jb2"},
    {JPG,"jpg"},
    {JS,"js"},
    {JSON,"json"},
    {LOG,"log"},
    {M4A,"m4a"},
    {MOV,"mov"},
    {MP3,"mp3"},
    {MP4,"mp4"},
    {NTFS,"ntfs"},
    {PDF,"pdf"},
    {PNG,"png"},
    {PPS,"pps"},
    {PPT,"ppt"},
    {PPTX,"pptx"},
    {PS,"ps"},
    {PST,"pst"},
    {RAND,"rand"},
    {RPM,"rpm"},
    {RTF,"rtf"},
    {SWF,"swf"},
    {TBIRD,"tbird"},
    {TCV_CONST,"tcv_const"},
    {TEXT,"txt"},
    {TIF,"tif"},
    {UCV_CONST,"ucv_const"},
    {URL,"url"},
    {WAV,"wav"},
    {WMA,"wma"},
    {WMV,"wmv"},
    {XLS,"xls"},
    {XLSX,"xlsx"},
    {XML,"xml"},
    {ZIP,"zip"},
    {0,""}
};



const char *sceadan_name_for_type(int code)
{
    for(int i=0;sceadan_types[i].name!=0;i++){
        if(sceadan_types[i].code==code) return sceadan_types[i].name;
    }
    return(0);
}

static sum_t max ( const sum_t a, const sum_t b ) {
    return a > b ? a : b;
}


/* FUNCTIONS FOR VECTORS */
static void vectors_update (const uint8_t buf[], const size_t sz, sceadan_vectors_t *v)
{
    const int sz_mod = v->mfv.uni_sz % 2;
    for (int ndx = 0; ndx < sz; ndx++) {

        /* Compute the unigrams */
        const unigram_t unigram = buf[ndx];
        v->ucv[unigram].tot++;

        {
            unigram_t prev;
            unigram_t next;

            if (ndx == 0 && sz_mod != 0) { 
                prev = v->last_val;
                next = unigram;

                v->bcv[prev][next].tot++;
                v->mfv.contiguity.tot += abs (next - prev);
            } else if (ndx + 1 < sz) {
                prev = unigram;
                next = buf[ndx + 1];
                v->mfv.contiguity.tot += abs (next - prev);
                if (ndx % 2 == sz_mod) v->bcv[prev][next].tot++;
            }
        }

        // total count of set bits (for hamming weight)
        // this is wierd
        v->mfv.hamming_weight.tot += (nbit_unigram - __builtin_popcount (unigram));

        // byte value sum
        v->mfv.byte_value.tot += unigram;

        // standard deviation of byte values
        v->mfv.stddev_byte_val.tot += __builtin_powi ((double) unigram, 2);

        if (v->last_cnt != 0 && v->last_val == unigram) {
            (v->last_cnt)++;
        }
        else {
            v->last_cnt = 1;
            v->last_val = unigram;
        }
        v->mfv.max_byte_streak.tot = max (v->last_cnt, v->mfv.max_byte_streak.tot);

        // count of low ascii values
        if       (unigram < ASCII_LO_VAL) v->mfv.lo_ascii_freq.tot++;

        // count of medium ascii values
        else if (unigram < ASCII_HI_VAL) v->mfv.med_ascii_freq.tot++;

        // count of high ascii values
        else {
            v->mfv.hi_ascii_freq.tot++;
        }
    }
    v->mfv.uni_sz += sz;
}

static void vectors_finalize ( sceadan_vectors_t *v)
{
    // hamming weight
    v->mfv.hamming_weight.avg = (double) v->mfv.hamming_weight.tot / (v->mfv.uni_sz * nbit_unigram);

    // mean byte value
    v->mfv.byte_value.avg = (double) v->mfv.byte_value.tot / v->mfv.uni_sz;

    // average contiguity between bytes
    v->mfv.contiguity.avg = (double) v->mfv.contiguity.tot / v->mfv.uni_sz;

    // max byte streak
    //v->mfv.max_byte_streak = max_cnt;
    v->mfv.max_byte_streak.avg = (double) v->mfv.max_byte_streak.tot / v->mfv.uni_sz;

    // TODO skewness ?
    double expectancy_x3 = 0;
    double expectancy_x4 = 0;

    const double central_tendency = v->mfv.byte_value.avg;
    for (int i = 0; i < n_unigram; i++) {

        v->mfv.abs_dev += v->ucv[i].tot * fabs (i - central_tendency);

        // unigram frequency
        v->ucv[i].avg = (double) v->ucv[i].tot / v->mfv.uni_sz;

        // item entropy
        double pv = v->ucv[i].avg;
        if (fabs(pv)>0) // TODO floating point mumbo jumbo
            v->mfv.item_entropy += pv * log2 (1 / pv) / nbit_unigram; // more divisions for accuracy

        for (int j = 0; j < n_unigram; j++) {

            v->bcv[i][j].avg = (double) v->bcv[i][j].tot / (v->mfv.uni_sz / 2); // rounds down

            // bigram entropy
            pv = v->bcv[i][j].avg;
            if (fabs(pv)>0) // TODO
                v->mfv.bigram_entropy  += pv * log2 (1 / pv) / nbit_bigram;
        }

        const double extmp = __builtin_powi ((double) i, 3) * v->ucv[i].avg;

        expectancy_x3 += extmp;        // for skewness
        expectancy_x4 += extmp * i;     // for kurtosis
    }

    const double variance  = (double) v->mfv.stddev_byte_val.tot / v->mfv.uni_sz
        - __builtin_powi (v->mfv.byte_value.avg, 2);

    v->mfv.stddev_byte_val.avg = sqrt (variance);

    const double sigma3    = variance * v->mfv.stddev_byte_val.avg;
    const double variance2 = __builtin_powi (variance, 2);

    // average absolute deviation
    v->mfv.abs_dev /= v->mfv.uni_sz;
    v->mfv.abs_dev /= n_unigram;

    // skewness
    v->mfv.skewness = (expectancy_x3
                     - v->mfv.byte_value.avg * (3 * variance
	                                      + __builtin_powi (v->mfv.byte_value.avg,
	                                                        2))) / sigma3;

    // kurtosis
    assert(isinf(expectancy_x4)==0);
    assert(isinf(variance2)==0);

    v->mfv.kurtosis = (expectancy_x4 / variance2);
    v->mfv.byte_value.avg      /= n_unigram;
    v->mfv.stddev_byte_val.avg /= n_unigram;
    v->mfv.kurtosis            /= n_unigram;
    v->mfv.contiguity.avg      /= n_unigram;
    //v->mfv.bzip2_len.avg        = 1;
    //v->mfv.lzw_len.avg          = 1;
    v->mfv.lo_ascii_freq.avg  = (double) v->mfv.lo_ascii_freq.tot  / v->mfv.uni_sz;
    v->mfv.med_ascii_freq.avg = (double) v->mfv.med_ascii_freq.tot / v->mfv.uni_sz;
    v->mfv.hi_ascii_freq.avg  = (double) v->mfv.hi_ascii_freq.tot  / v->mfv.uni_sz;
}


static int do_predict ( const struct model* model_ , const sceadan_vectors_t *v, struct feature_node *x )
{
    int n;
    int nr_feature=get_nr_feature(model_);
    if(model_->bias>=0){
        n=nr_feature+1;
    } else {
        n=nr_feature;
    }
    
    int i = 0;
    
    /* Add the unigrams to the vector */
    for (int k = 0 ; k < n_unigram; k++, i++) {
        x[i].index = i + 1;
        x[i].value = v->ucv[k].avg;
    }
    
    /* Add the bigrams to the vector */
    for (int k = 0; k < n_unigram; k++)
        for (int j = 0; j < n_unigram; j++) {
            x[i].index = i + 1;
            x[i].value = v->bcv[k][j].avg;
            i++;
        }
    
    /* Add the Bias */
    if(model_->bias>=0)    {
        x[i].index = n;
        x[i].value = model_->bias;
        i++;
    }
    x[i].index = -1;                    /* end of vectors? */
    return predict(model_,x);           /* run the predictor */
}


static void dump_vectors_as_json(const sceadan *s,const sceadan_vectors_t *v)
{
    printf("{ \"file_type\": %d,\n",s->file_type);
    if(v->file_name) printf("  \"file_name\": \"%s\",\n",v->file_name);
    printf("  \"unigrams\": { \n");
    int first = 1;
    for(int i=0;i<n_unigram;i++){
        if(v->ucv[i].avg>0){
            if(first) {
                first = 0;
            } else {
                printf(",\n");
            }
            printf("    \"%d\" : %.16lg",i,v->ucv[i].avg);
        }
    }
    printf("  },\n");
    printf("  \"bigrams:\": { \n");
    first = 1;
    for(int i=0;i<n_unigram;i++){
        for(int j=0;j<n_unigram;j++){
            if(v->bcv[i][j].avg>0){
                if(first){
                    first = 0;
                } else {
                    printf(",\n");
                    first = 0;
                }
                printf("    \"%d\" : %.16lg",i<<8|j,v->bcv[i][j].avg);
            }
        }
    }
    printf("  }\n");
#define OUTPUT(XXX) printf("  \"%s\": %.16lg,\n",#XXX,v->mfv.XXX)
    OUTPUT(bigram_entropy);
    OUTPUT(item_entropy);
    OUTPUT(hamming_weight.avg);
    OUTPUT(byte_value.avg);
    OUTPUT(stddev_byte_val.avg);
    OUTPUT(abs_dev);
    OUTPUT(skewness);
    OUTPUT(kurtosis);
    OUTPUT(contiguity.avg);
    OUTPUT( max_byte_streak.avg);
    OUTPUT(lo_ascii_freq.avg);
    OUTPUT(med_ascii_freq.avg);
    OUTPUT(hi_ascii_freq.avg);
    OUTPUT(byte_val_correlation);
    OUTPUT(byte_val_freq_correlation);
    OUTPUT(uni_chi_sq);
    printf("  \"version\":1.0\n");

    printf("}\n");
}

/* predict the vectors with a model and return the predicted type.
 * 
 * That is to handle vectors of too little or too much
 * variance/entropy, e.g. a file of all 0x00s will be considered
 * CONSTANT, or a file of all random unigrams will be considered
 * RANDOM. We consider those vectors abnormal and taken special care
 * of, instead of predicting. 
 */
static int predict_liblin(const sceadan *s,const sceadan_vectors_t *v)
{
    if(s->dump){                        /* dumping, not predicting */
        dump_vectors_as_json(s,v);
        return 0;
    }

    if (v->mfv.item_entropy > RANDOMNESS_THRESHOLD) {
        return RAND;
    }
    
    for (int i = 0; i < n_unigram; i++) {
        // TODO floating point comparison
        if (v->ucv[i].avg > UCV_CONST_THRESHOLD) {
            // previous programmer had an assignment here.
            // but there is no need, and that makes v non-const
            // slg
            //v->mfv.const_chr[0] = i;       
            return UCV_CONST;
        }
        for (int j = 0; j < n_unigram; j++)
            // previous programmer had an assignment here.
            // but there is no need, and that makes v non-const
            // slg
            //v->mfv.const_chr[0] = i;       
            if (v->bcv[i][j].avg > BCV_CONST_THRESHOLD) {
                //v->mfv.const_chr[0] = i;
                //v->mfv.const_chr[1] = j;
                return BCV_CONST;
            }
    }
    
    const int max_nr_attr = n_bigram + n_unigram + 3;//+ /*20*/ 17 /*6 + 2 + 9*/;
    struct feature_node *x = (struct feature_node *) malloc(max_nr_attr*sizeof(struct feature_node));
    int ret = do_predict(s->model,v, x);
    free(x);
    return ret;
}


struct model *model_ = 0;
const struct model *sceadan_model_default()
{
    if(model_==0){
        model_=load_model(MODEL);
        if(model_==0){
            fprintf(stderr,"can't open model file %s\n","");
            return 0;
        }
    }
    return model_;
}

void sceadan_model_dump(const struct model *model)
{
    puts("#include \"config.h\"");
    puts("#ifdef HAVE_LINEAR_H");
    puts("#include <linear.h>");
    puts("#endif");
    puts("#ifdef HAVE_LIBLINEAR_LINEAR_H");
    puts("#include <liblinear/linear.h>");
    puts("#endif");
    puts("#include \"sceadan.h\"");

    if(model->param.nr_weight){
        printf("static int weight_label[]={");
        for(int i=0;i<model->param.nr_weight;i++){
            if(i>0) putchar(',');
            printf("%d",model->param.weight_label[i]);
            if(i%10==9) printf("\n\t");
        }
        printf("};\n\n");
    }
    if(model->param.nr_weight){
        printf("static double weight[] = {");
        for(int i=0;i<model->param.nr_weight;i++){
            if(i>0) putchar(',');
            printf("%g",model->param.weight[i]);
            if(i%10==9) printf("\n\t");
        }
        printf("};\n\n");
    }

    printf("static int label[] = {");
    for(int i=0;i<model->nr_class;i++){
        printf("%d",model->label[i]);
        if(i<model->nr_class-1) putchar(',');
        if(i%20==19) printf("\n\t");
    }
    printf("};\n");

    printf("static double w[] = {");
    int n;
    if(model->bias>=0){
        n = model->nr_feature+1;
    } else {
        n = model->nr_feature;
    }
    int w_size = n;
    int nr_w;
    if(model->nr_class==2 && model->param.solver_type != 4){
        nr_w = 1;
    } else {
        nr_w = model->nr_class;
    }

    for(int i=0;i<w_size;i++){
        for(int j=0;j<nr_w;j++){
            printf("%.16lg",model->w[i*nr_w+j]);
            if(i!=w_size-1 || j!=nr_w-1) putchar(',');
        }
        printf("\n\t");
    }
    printf("};\n");
        
    printf("static struct model m = {\n");
    printf("\t.param = {\n");
    printf("\t\t.solver_type=%d,\n",model->param.solver_type);
    printf("\t\t.eps = %g,\n",model->param.eps);
    printf("\t\t.C = %g,\n",model->param.C);
    printf("\t\t.nr_weight = %d,\n",model->param.nr_weight);
    printf("\t\t.weight_label = %s,\n",model->param.nr_weight ? "weight_label" : "0");
    printf("\t\t.weight = %s,\n",model->param.nr_weight ? "weight" : "0");
    printf("\t\t.p = %g},\n",model->param.p);

    printf("\t.nr_class=%d,\n",model->nr_class);
    printf("\t.nr_feature=%d,\n",model->nr_feature);
    printf("\t.w=w,\n");
    printf("\t.label=label,\n");
    printf("\t.bias=%g};\n",model->bias);

    printf("const struct model *sceadan_model_precompiled(){return &m;}\n");
}


sceadan *sceadan_open(const char *model_name) // use 0 for default model
{
    sceadan *s = (sceadan *)calloc(sizeof(sceadan),1);
    if(model_name){
        s->model = load_model(model_name);
        if(s->model==0){
            free(s);
            return 0;
        }
        return s;
    }
    s->model = sceadan_model_precompiled();
    return s;
}

void sceadan_close(sceadan *s)
{
    memset(s,0,sizeof(*s));             /* clean object re-use */
    free(s);
}

int sceadan_classify_buf(const sceadan *s,const uint8_t *buf,size_t bufsize)
{
    sceadan_vectors_t v;
    memset(&v,0,sizeof(v));
    vectors_update (buf, bufsize, &v);
    vectors_finalize(&v);
    return predict_liblin(s,&v);
}

int sceadan_classify_file(const sceadan *s,const char *file_name)
{
    struct sceadan_vectors v;
    memset(&v,0,sizeof(v));
    v.file_name = file_name;
    const int fd = open(file_name, O_RDONLY|O_BINARY);
    if (fd<0) return -1;                /* error condition */
    while (true) {
        uint8_t    buf[BUFSIZ];
        const ssize_t rd = read (fd, buf, sizeof (buf));
        if(rd<=0) break;
        vectors_update (buf, rd, &v);
    }
    if(close(fd)<0) return -1;
    vectors_finalize(&v);
    return predict_liblin(s,&v);
}

void sceadan_dump_vectors_on_classify(sceadan *s,int file_type,FILE *out)
{
    s->dump = out;
    s->file_type = file_type;
}
