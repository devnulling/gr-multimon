/* -*- c++ -*- */
/*
 * 
 * (C) 2012 Frederik M.J.V.
 * Based on multimon :
 * (C) 1996 Thomas Sailer (sailer@ife.ee.ethz.ch, hb9jnx@hb9w.che.eu)
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "filter.h"
#include <stdio.h>
#include <stdarg.h>
#include <afsk1200.h>
#include <gnuradio/io_signature.h>

/*
 * Create a new instance of multimon_afsk1200 and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
multimon_afsk1200_sptr
make_multimon_afsk1200()//(const std::string &args)
{
  //return multimon_afsk1200_sptr(new multimon_afsk1200());
  return gnuradio::get_initial_sptr(new multimon_afsk1200());
}

/*
 * Specify constraints on number of input and output streams.
 * This info is used to construct the input and output signatures
 * (2nd & 3rd args to gr::block's constructor).  The input and
 * output signatures are used by the runtime system to
 * check that a valid number and type of inputs and outputs
 * are connected to this block.  In this case, we accept
 * only 1 input and 1 output.
 */
static const int MIN_IN = 1;    // mininum number of input streams
static const int MAX_IN = 1;    // maximum number of input streams
static const int MIN_OUT = 1;   // minimum number of output streams
static const int MAX_OUT = 1;   // maximum number of output streams

//Multimon API
static const struct demod_param *dem[] = { ALL_DEMOD };
//For AFSK1200
#define FREQ_MARK  1200
#define FREQ_SPACE 2200
#define FREQ_SAMP  12000
#define BAUD       1200
#define SUBSAMP    2

/* ---------------------------------------------------------------------- */

#define CORRLEN ((int)(FREQ_SAMP/BAUD))
#define SPHASEINC (0x10000u*BAUD*SUBSAMP/FREQ_SAMP)


static float corr_mark_i[CORRLEN];
static float corr_mark_q[CORRLEN];
static float corr_space_i[CORRLEN];
static float corr_space_q[CORRLEN];

/*
 * The private constructor
 */
multimon_afsk1200::multimon_afsk1200 ()
  : gr::block ("multimon_afsk1200_demod",
              gr::io_signature::make (MIN_IN, MAX_IN, sizeof (float)),
              gr::io_signature::make (MIN_OUT, MAX_OUT, sizeof (char)))
{
   state = (demod_state*) malloc(sizeof(demod_state));
   memset(state, 0, sizeof(*state));
//  if (demod_multimon_afsk1200.init)
//    demod_multimon_afsk1200.init(state);

	float f;
	int i;

	//hdlc_init(s);
	memset(&state->l1.afsk12, 0, sizeof(state->l1.afsk12));
	for (f = 0, i = 0; i < CORRLEN; i++) {
		corr_mark_i[i] = cos(f);
		corr_mark_q[i] = sin(f);
		f += 2.0*M_PI*FREQ_MARK/FREQ_SAMP;
	}
	for (f = 0, i = 0; i < CORRLEN; i++) {
		corr_space_i[i] = cos(f);
		corr_space_q[i] = sin(f);
		f += 2.0*M_PI*FREQ_SPACE/FREQ_SAMP;
	}

}

/*
 * Our virtual destructor.
 */
multimon_afsk1200::~multimon_afsk1200 ()
{
  // nothing else required in this example
  free(state);
}
static void afsk12_demod(struct demod_state *s, const float *inbuffer, int length, unsigned char* outbuffer, unsigned int* outlen)
{
	float f;
	unsigned char curbit;
    unsigned int numdemod = 0;

	if (s->l1.afsk12.subsamp) {
		int numfill = SUBSAMP - s->l1.afsk12.subsamp;
		if (length < numfill) {
			s->l1.afsk12.subsamp += length;
			return;
		}
		inbuffer += numfill;
		length -= numfill;
		s->l1.afsk12.subsamp = 0;
	}
	for (; length >= SUBSAMP; length -= SUBSAMP, inbuffer += SUBSAMP) {
		f = fsqr(mac(inbuffer, corr_mark_i, CORRLEN)) +
			fsqr(mac(inbuffer, corr_mark_q, CORRLEN)) -
			fsqr(mac(inbuffer, corr_space_i, CORRLEN)) -
			fsqr(mac(inbuffer, corr_space_q, CORRLEN));
		s->l1.afsk12.dcd_shreg <<= 1;
		s->l1.afsk12.dcd_shreg |= (f > 0);
//		verbprintf(10, "%c", '0'+(s->l1.afsk12.dcd_shreg & 1));
		/*
		 * check if transition
		 */
		if ((s->l1.afsk12.dcd_shreg ^ (s->l1.afsk12.dcd_shreg >> 1)) & 1) {
			if (s->l1.afsk12.sphase < (0x8000u-(SPHASEINC/2)))
				s->l1.afsk12.sphase += SPHASEINC/8;
			else
				s->l1.afsk12.sphase -= SPHASEINC/8;
		}
		s->l1.afsk12.sphase += SPHASEINC;
		if (s->l1.afsk12.sphase >= 0x10000u) {
			s->l1.afsk12.sphase &= 0xffffu;
			s->l1.afsk12.lasts <<= 1;
			s->l1.afsk12.lasts |= s->l1.afsk12.dcd_shreg & 1;
			curbit = (s->l1.afsk12.lasts ^ 
				  (s->l1.afsk12.lasts >> 1) ^ 1) & 1;
 			//verbprintf(9, " %c ", '0'+curbit);
            if(!(numdemod < *outlen)){
                printf("NUMOVERFLOW\n");
                return;
            }
            outbuffer[numdemod++] = curbit;
//			hdlc_rxbit(s, curbit);
		}
	}
    *outlen = numdemod;
	s->l1.afsk12.subsamp = length;
}

int 
multimon_afsk1200::general_work (int noutput_items,
                               gr_vector_int &ninput_items,
                               gr_vector_const_void_star &input_items,
                               gr_vector_void_star &output_items)
{
  const float *in = (const float *) input_items[0];
  char out[255];
  unsigned int outlen = noutput_items;
  //float *out = (float *) output_items[0];
  afsk12_demod(state, (const float*) input_items[0], ninput_items[0], (unsigned char*) output_items[0], &outlen);

 /* printf("PE:%d", outlen);
  for (int i = 0; i < outlen; i++){
        printf("%c", '0'+((unsigned char*)output_items[0])[i]);
  }
  printf("\n");*/

  // Tell runtime system how many input items we consumed on
  // each input stream.

  consume_each (ninput_items[0]);

  // Tell runtime system how many output items we produced.
  
  return outlen;
}

/*static void process_buffer(float *buf, unsigned int len)
{
	int i;

	for (i = 0; i <  NUMDEMOD; i++) 
		if (MASK_ISSET(i) && dem[i]->demod)
			dem[i]->demod(dem_st+i, buf, len);
}*/
static int verbose_level = 10;

/* ---------------------------------------------------------------------- */
extern "C"{
void verbprintf(int verb_level, const char *fmt, ...)
{
        va_list args;
        
        va_start(args, fmt);
        if (verb_level <= verbose_level)
                vfprintf(stdout, fmt, args);
        va_end(args);
}
}
