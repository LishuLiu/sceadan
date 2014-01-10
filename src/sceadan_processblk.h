#ifndef SCEADAN_PROCESSBLK_H
#define SCEADAN_PROCESSBLK_H


/* STANDARD INCLUDES */                                  /* STANDARD INCLUDES */
#include <stdio.h>
#include <stdlib.h>
/* END STANDARD INCLUDES */                          /* END STANDARD INCLUDES */

/* DTFE INCLUDES */                                          /* DTFE INCLUDES */
#include "file_type.h"
#include "ngram.h"
/* END DTFE INCLUDES */                                  /* END DTFE INCLUDES */

/* low  ascii range is
   0x00 <= char < 0x20 */
#define ASCII_LO_VAL (0x20)

/* mid  ascii range is
   0x20 <= char < 0x80 */

/* high ascii range is
   0x80 <= char */
#define ASCII_HI_VAL (0x80)

/* type for accumulators */
typedef unsigned long long sum_t;

/* type for summation/counting
   followed by floating point ops */
typedef union {
	sum_t  tot;
	double avg;
} cv_e;

/* unigram count vector
   map unigram to count,
          then to frequency
   implements the probability distribution function for unigrams
   TODO beware of overflow
         more implementations */
typedef cv_e ucv_t[n_unigram];

/* bigram count vector
   map bigram to count,
         then to frequency
   implements the probability distribution function for bigrams
   TODO beware of overflow
         more implementations */
typedef cv_e bcv_t[n_unigram][n_unigram];

typedef enum {
	ID_UNDEF = 0,
	ID_CONTAINER,
	ID_BLOCK
} id_e;

/* main feature vector */
typedef struct {

	id_e id_type;

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
	cv_e bzip2_len;

	/* Feature Name: Compressed item length - LZW
	   Description : Lempel-Ziv-Welch algorithm; fast */
	cv_e lzw_len;

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

/* INIT'N LITERALS */

// you'll see what this is for
#define ZEROES_____1 .tot=0
#define ZEROES_____2 ZEROES_____1, ZEROES_____1
#define ZEROES_____4 ZEROES_____2, ZEROES_____2
#define ZEROES_____8 ZEROES_____4, ZEROES_____4
#define ZEROES____16 ZEROES_____8, ZEROES_____8
#define ZEROES____32 ZEROES____16, ZEROES____16
#define ZEROES____64 ZEROES____32, ZEROES____32
#define ZEROES___128 ZEROES____64, ZEROES____64
#define ZEROES___256 ZEROES___128, ZEROES___128

#define UCV_LIT {{ ZEROES___256 }}

#define ROWS___2 UCV_LIT,  UCV_LIT
#define ROWS___4 ROWS___2, ROWS___2
#define ROWS___8 ROWS___4, ROWS___4
#define ROWS__16 ROWS___8, ROWS___8
#define ROWS__32 ROWS__16, ROWS__16
#define ROWS__64 ROWS__32, ROWS__32
#define ROWS_128 ROWS__64, ROWS__64
#define ROWS_256 ROWS_128, ROWS_128


#define BCV_LIT { ROWS_256 }

#define MFV_LIT { \
	.id_type                   = ID_UNDEF, \
	/*.id_container              = NULL, */\
	/*.id_block                   = 0, */\
	/*.const_chr                   = NULL, */\
	.uni_sz                    = 0, \
	/*.bi_sz                     = 0,*/ \
	.bigram_entropy            = 0, \
	.item_entropy              = 0, \
	.hamming_weight            = { .tot = 0 }, \
	.byte_value                = { .tot = 0 }, \
	.stddev_byte_val           = { .tot = 0 }, \
	.kurtosis                  = 0, \
	.bzip2_len                 = { .tot = 0 }, \
	/*.lzw_len                   = { .tot = 0 }, */\
	.contiguity                = { .tot = 0 }, \
	.max_byte_streak           = { .tot = 0 }, \
	.lo_ascii_freq             = { .tot = 0 }, \
	.med_ascii_freq            = { .tot = 0 }, \
	.hi_ascii_freq             = { .tot = 0 }, \
	/*.byte_val_correlation      = 0, */\
	/*.byte_val_freq_correlation = 0, */\
	/*.uni_chi_sq                = 0  */\
}

#define MFV_CONTAINER_LIT { \
	.id_type                   = ID_CONTAINER, \
	/*.id_container              = NULL, */\
	.id_block                   = 0, \
	/*.const_chr                   = NULL, */\
	.uni_sz                    = 0, \
	/*.bi_sz                     = 0,*/ \
	.bigram_entropy            = 0, \
	.item_entropy              = 0, \
	.hamming_weight            = { .tot = 0 }, \
	.byte_value                = { .tot = 0 }, \
	.stddev_byte_val           = { .tot = 0 }, \
	.kurtosis                  = 0, \
	.bzip2_len                 = { .tot = 0 }, \
	/*.lzw_len                   = { .tot = 0 }, */\
	.contiguity                = { .tot = 0 }, \
	.max_byte_streak           = { .tot = 0 }, \
	.lo_ascii_freq             = { .tot = 0 }, \
	.med_ascii_freq            = { .tot = 0 }, \
	.hi_ascii_freq             = { .tot = 0 }, \
	/*.byte_val_correlation      = 0, */\
	/*.byte_val_freq_correlation = 0, */\
	/*.uni_chi_sq                = 0  */\
}

#define MFV_BLOCK_LIT { \
	.id_type                   = ID_BLOCK, \
	/*.id_container              = NULL, */\
	/*.id_block                   = 0, */\
	/*.const_chr                   = NULL, */\
	.uni_sz                    = 0, \
	/*.bi_sz                     = 0,*/ \
	.bigram_entropy            = 0, \
	.item_entropy              = 0, \
	.hamming_weight            = { .tot = 0 }, \
	.byte_value                = { .tot = 0 }, \
	.stddev_byte_val           = { .tot = 0 }, \
	.kurtosis                  = 0, \
	.bzip2_len                 = { .tot = 0 }, \
	/*.lzw_len                   = { .tot = 0 }, */\
	.contiguity                = { .tot = 0 }, \
	.max_byte_streak           = { .tot = 0 }, \
	.lo_ascii_freq             = { .tot = 0 }, \
	.med_ascii_freq            = { .tot = 0 }, \
	.hi_ascii_freq             = { .tot = 0 }, \
	/*.byte_val_correlation      = 0, */\
	/*.byte_val_freq_correlation = 0, */\
	/*.uni_chi_sq                = 0  */\
}
/* END INIT'N LITERALS */

/* TYPEDEFS FOR I/O */
typedef int (*output_f) (
	const ucv_t        ucv,
	const bcv_t        bcv,
	const mfv_t *const mfv,
	      FILE  *const outs[3],
	      file_type_e  file_type
);
/* END TYPEDEFS FOR I/O */


/* FUNCTIONS */
// TODO full path vs relevant path may matter
int
process_dir (
	const          char path[],
	const unsigned int  block_factor,
	const output_f      do_output,
	      FILE *const outs[3],
	      file_type_e file_type
) ;

int
process_file (
	const          char path[],
	const unsigned int  block_factor,
	const output_f      do_output,
	      FILE *const outs[3],
	      file_type_e file_type
) ;

int
process_blocks (
	const char         path[],
	const unsigned int block_factor,
	const output_f     do_output,
	      FILE *const outs[3],
	      file_type_e file_type
) ;

int
process_container (
	const char     path[],
	const output_f do_output,
	      FILE *const outs[3],
	      file_type_e file_type
) ;

void
vectors_update (
	const unigram_t        buf[],
	const size_t           sz,
	      ucv_t            ucv,
	      bcv_t            bcv,
	      mfv_t     *const mfv,

	      sum_t     *const last_cnt,
	      unigram_t *const last_val
) ;

void
vectors_finalize (
	      ucv_t        ucv,
	      bcv_t        bcv,
	      mfv_t *const mfv
) ;
/* END OF FUNCTIONS */

int
output_competition  (
	const ucv_t        ucv,
	const bcv_t        bcv,
	const mfv_t *const mfv,
	      FILE  *const unused[3],
	      file_type_e file_type
) ;
#endif


