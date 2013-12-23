/***
  This file is part of PulseAudio.
  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.
  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#define BUFSIZE 2048

#include <iostream>
#include <cmath>
#include <speex/speex_preprocess.h>

int main(int argc, char*argv[])
{
	/* The Sample format to use */
	static const pa_sample_spec ss =
	{
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 2
	};

	pa_simple* dev_out = 0;
	pa_simple* dev_in = 0;

	int ret = 1;
	int error;


	/* Create a new playback stream */
	if (!(dev_out = pa_simple_new(NULL, "Noise Remover", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error)))
	{
		fprintf(stderr, __FILE__": pa_simple_new() failed: %dev_out\n", pa_strerror(error));
		goto finish;
	}
	if (!(dev_in = pa_simple_new(NULL, "Noise Remover", PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error)))
	{
		fprintf(stderr, __FILE__": pa_simple_new() failed: %dev_out\n", pa_strerror(error));
		goto finish;
	}

	{
		int i;
		float f;

		SpeexPreprocessState* pp = speex_preprocess_state_init(BUFSIZE, ss.rate);

		i = 1;       speex_preprocess_ctl(pp, SPEEX_PREPROCESS_SET_DENOISE, &i);
		i = 1;       speex_preprocess_ctl(pp, SPEEX_PREPROCESS_SET_AGC, &i);
		f = 8000;    speex_preprocess_ctl(pp, SPEEX_PREPROCESS_SET_AGC_LEVEL, &f);
		i = 1;       speex_preprocess_ctl(pp, SPEEX_PREPROCESS_SET_DEREVERB, &i);
		f = 0.04;     speex_preprocess_ctl(pp, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
		f = 0.03;     speex_preprocess_ctl(pp, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);

		double lowest_rms = 99999999999999;
		int silence_count = 0;

		for (;;)
		{
			int16_t buf[BUFSIZE];

			/* Read some data ... */
			if (pa_simple_read(dev_in, buf, sizeof(buf), &error) < 0)
			{
		        fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n",  pa_strerror(error));
		        goto finish;
		    }

		    /* ... Use speex to de-noise ... */
		    double total = 0;
		    for(int n = 0; n < sizeof(buf); n++)
		    	total += buf[n] * buf[n];
		    double rms = std::sqrt(total / sizeof(buf));

		    if(rms < lowest_rms)
		    	lowest_rms = rms;
		    
			
		    if((rms - lowest_rms) < 50) // this value will probably need adjusting for you
		    	silence_count = 0;
			else if(silence_count < 10)
				silence_count++;

			if(silence_count == 10)
				speex_preprocess_run(pp, buf);
			else
				continue; // don't write it out...

			/* ... and play it */
			if (pa_simple_write(dev_out, buf, sizeof(buf), &error) < 0)
			{
				fprintf(stderr, __FILE__": pa_simple_write() failed: %dev_out\n", pa_strerror(error));
				goto finish;
			}
		}
		/* Make sure that every single sample was played */
		if (pa_simple_drain(dev_out, &error) < 0)
		{
			fprintf(stderr, __FILE__": pa_simple_drain() failed: %dev_out\n", pa_strerror(error));
			goto finish;
		}
	}
	ret = 0;
finish:
	if (dev_out)
		pa_simple_free(dev_out);
	if (dev_in)
		pa_simple_free(dev_in);
	return ret;
}