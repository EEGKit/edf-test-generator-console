/*
***************************************************************************
*
* Author: Teunis van Beelen
*
* Copyright (C) 2018 - 2021 Teunis van Beelen
*
* Email: teuniz@protonmail.com
*
***************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
***************************************************************************
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <float.h>
#include <getopt.h>
#include <errno.h>

#include "edflib.h"
#include "utils.h"

#define PROGRAM_NAME       "edfgenerator"
#define PROGRAM_VERSION    "1.09"

#define FILETYPE_EDF       (0)
#define FILETYPE_BDF       (1)

#define WAVE_SINE          (0)
#define WAVE_SQUARE        (1)
#define WAVE_RAMP          (2)
#define WAVE_TRIANGLE      (3)
#define WAVE_WHITE_NOISE   (4)
#define WAVE_PINK_NOISE    (5)

#define EDF_MAX_CHNS      (16)


struct sig_par_struct
{
  int sf[EDF_MAX_CHNS];
  int digmax[EDF_MAX_CHNS];
  int digmin[EDF_MAX_CHNS];
  int waveform[EDF_MAX_CHNS];

  double w[EDF_MAX_CHNS];
  double q[EDF_MAX_CHNS];
  double sine_1[EDF_MAX_CHNS];
  double square_1[EDF_MAX_CHNS];
  double triangle_1[EDF_MAX_CHNS];
  double signalfreq[EDF_MAX_CHNS];
  double physmax[EDF_MAX_CHNS];
  double physmin[EDF_MAX_CHNS];
  double peakamp[EDF_MAX_CHNS];
  double dutycycle[EDF_MAX_CHNS];
  double dc_offset[EDF_MAX_CHNS];
  double phase[EDF_MAX_CHNS];

  double b0[EDF_MAX_CHNS];
  double b1[EDF_MAX_CHNS];
  double b2[EDF_MAX_CHNS];
  double b3[EDF_MAX_CHNS];
  double b4[EDF_MAX_CHNS];
  double b5[EDF_MAX_CHNS];
  double b6[EDF_MAX_CHNS];

  char physdim[EDF_MAX_CHNS][32];

  double *buf[EDF_MAX_CHNS];

  int *randbuf[EDF_MAX_CHNS];
} sig_par;


int main(int argc, char **argv)
{
  int i, j, n, chan,
      err,
      option_index=0,
      c=0,
      fd=-1,
      hdl=-1,
      filetype=0,
      duration=30,
      digmax_set=0,
      digmin_set=0,
      datrecs=0,
      datrecs_set=0,
      datrecduration_set=0,
      merge_set=0,
      chns=1,
      edf_chns=1;

  double datrecduration=1,
         ftmp,
         white_noise,
         *merge_buf=NULL;

  char str[1024]="",
       *s_ptr=NULL;

  const char waveforms_str[6][16]={"sine", "square", "ramp", "triangle", "white-noise", "pink-noise"};

  setlocale(LC_ALL, "C");

  setlinebuf(stdout);
  setlinebuf(stderr);

  memset(&sig_par, 0, sizeof(struct sig_par_struct));

  for(i=0; i<EDF_MAX_CHNS; i++)
  {
    sig_par.sf[i] = 500;
    sig_par.w[i] = 1;
    sig_par.q[i] = 1;
    sig_par.signalfreq[i] = 10;
    sig_par.physmax[i] = 1200;
    sig_par.physmin[i] = -1200;
    sig_par.peakamp[i] = 1000;
    sig_par.dutycycle[i] = 50;
    sig_par.phase[i] = 0;
    strlcpy(sig_par.physdim[i], "uV", 32);
  }

  struct option long_options[] = {
    {"type",            required_argument, 0, 0},  /*  0 */
    {"len",             required_argument, 0, 0},  /*  1 */
    {"rate",            required_argument, 0, 0},  /*  2 */
    {"freq",            required_argument, 0, 0},  /*  3 */
    {"wave",            required_argument, 0, 0},  /*  4 */
    {"dcycle",          required_argument, 0, 0},  /*  5 */
    {"phase",           required_argument, 0, 0},  /*  6 */
    {"physmax",         required_argument, 0, 0},  /*  7 */
    {"physmin",         required_argument, 0, 0},  /*  8 */
    {"amp",             required_argument, 0, 0},  /*  9 */
    {"unit",            required_argument, 0, 0},  /* 10 */
    {"digmax",          required_argument, 0, 0},  /* 11 */
    {"digmin",          required_argument, 0, 0},  /* 12 */
    {"offset",          required_argument, 0, 0},  /* 13 */
    {"datrecs",         required_argument, 0, 0},  /* 14 */
    {"datrec-duration", required_argument, 0, 0},  /* 15 */
    {"signals",         required_argument, 0, 0},  /* 16 */
    {"merge",           no_argument,       0, 0},  /* 17 */
    {"help",            no_argument,       0, 0},  /* 18 */
    {0, 0, 0, 0}
  };

  while(1)
  {
    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    if(c == -1)  break;

      if(option_index == 16)  /* signals */
      {
        chns = atoi(optarg);
        if((chns < 1) || (chns > EDF_MAX_CHNS))
        {
          fprintf(stderr, "illegal value for option %s, must be in the range 1 to %i\n", long_options[option_index].name, EDF_MAX_CHNS);
          return EXIT_FAILURE;
        }
        if(chns == 1)
        {
          merge_set = 0;
        }
      }
  }

  optind = 1;

  while(1)
  {
    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    if(c == -1)  break;

    if(c == 0)
    {
      if(option_index < 17)
      {
        if(optarg == NULL)
        {
          fprintf(stderr, "missing value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
      }

      if(option_index == 0)  /* type */
      {
        if(!strcmp(optarg, "edf"))
        {
          filetype = FILETYPE_EDF;
        }
        else if(!strcmp(optarg, "bdf"))
          {
            filetype = FILETYPE_BDF;
          }
          else
          {
            fprintf(stderr, "unrecognized value for option %s\n", long_options[option_index].name);
            return EXIT_FAILURE;
          }
      }

      if(option_index == 1)  /* len */
      {
        duration = atoi(optarg);
        if(duration < 1)
        {
          fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
      }

      if((option_index >= 2) && (option_index <= 13))  /* signal parameters */
      {
        for(n=0; n<chns; n++)
        {
          if(!n)
          {
            s_ptr = strtok(optarg, ",");
          }
          else
          {
            s_ptr = strtok(NULL, ",");
          }
          if(s_ptr == NULL)
          {
            break;
          }

          if(option_index == 2)
          {
            sig_par.sf[n] = atoi(s_ptr);
            if(sig_par.sf[n] < 1)
            {
              fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
              return EXIT_FAILURE;
            }
          }

          if(option_index == 3)  /* rate */
          {
            sig_par.signalfreq[n] = atof(s_ptr);
            if((sig_par.signalfreq[n] < 9.99999e-2) || (sig_par.signalfreq[n] > (sig_par.sf[n] / 4.0 + 1e-6)))
            {
              fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
              return EXIT_FAILURE;
            }
          }

          if(option_index == 4)  /* wave */
          {
            for(i=0; i<6; i++)
            {
              if(!strcmp(s_ptr, waveforms_str[i]))
              {
                sig_par.waveform[n] = i;
                break;
              }
            }
            if(i==6)
            {
              fprintf(stderr, "unrecognized value for option %s\n", long_options[option_index].name);
              return EXIT_FAILURE;
            }
          }

          if(option_index == 5)  /* dcycle */
          {
            sig_par.dutycycle[n] = atof(s_ptr);
            if((sig_par.dutycycle[n] < 0.099999) || (sig_par.dutycycle[n] > 100.000001))
            {
              fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
              return EXIT_FAILURE;
            }
          }

          if(option_index == 6)  /* phase */
          {
            sig_par.phase[n] = atof(s_ptr);
            if((sig_par.phase[n] <= -360) || (sig_par.phase[n] >= 360))
            {
              fprintf(stderr, "illegal value for option %s, must be in the range -359.999999 to 359.999999\n", long_options[option_index].name);
              return EXIT_FAILURE;
            }
            sig_par.phase[n] = fmod(sig_par.phase[n] + 360, 360);
          }

          if(option_index == 7)  /* physmax */
          {
            sig_par.physmax[n] = atof(s_ptr);
          }

          if(option_index == 8)  /* physmin */
          {
            sig_par.physmin[n] = atof(s_ptr);
          }

          if(option_index == 9)  /* amp */
          {
            sig_par.peakamp[n] = atof(s_ptr);
          }

          if(option_index == 10)  /* unit */
          {
            strlcpy(sig_par.physdim[n], s_ptr, 16);
          }

          if(option_index == 11)  /* digmax */
          {
            sig_par.digmax[n] = atoi(s_ptr);
            digmax_set = 1;
          }

          if(option_index == 12)  /* digmin */
          {
            sig_par.digmin[n] = atoi(s_ptr);
            digmin_set = 1;
          }

          if(option_index == 13)  /* offset */
          {
            sig_par.dc_offset[n] = atof(s_ptr);
          }
        }

        if(n != chns)
        {
          fprintf(stderr, "found %i parameters for option %s but expected %i\n", n, long_options[option_index].name, chns);
          return EXIT_FAILURE;
        }
      }

      if(option_index == 14)  /* datrecs */
      {
        datrecs = atoi(optarg);
        if(datrecs < 1)
        {
          fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
        datrecs_set = 1;
      }

      if(option_index == 15)  /* datrec-duration */
      {
        datrecduration = atof(optarg);
        if((datrecduration < 0.001) || (datrecduration > 60))
        {
          fprintf(stderr, "illegal value for option %s, must be in the range 0.001 to 60\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
        datrecduration_set = 1;
      }

      if(option_index == 17)  /* merge */
      {
        if(chns > 1)
        {
          merge_set = 1;
        }
      }

      if(option_index == 18)
      {
        fprintf(stdout, "\n EDF generator version " PROGRAM_VERSION
          " Copyright (c) 2020 - 2021 Teunis van Beelen   email: teuniz@protonmail.com\n"
          "\n Usage: " PROGRAM_NAME " [OPTION]...\n"
          "\n options:\n"
          "\n --type=edf|bdf default: edf\n"
          "\n --len=file duration in seconds default: 30\n"
          "\n --rate=samplerate in Hertz default: 500 (integer only)\n"
          "\n --freq=signal frequency in Hertz default: 10 (may be a real number e.g. 333.17)\n"
          "\n --wave=sine | square | ramp | triangle | white-noise | pink-noise default: sine\n"
          "\n --dcycle=dutycycle: 0.1-100%% default: 50\n"
          "\n --phase=phase: 0-360degr. default: 0\n"
          "\n --physmax=physical maximum default: 1200 (may be a real number e.g. 1199.99)\n"
          "\n --physmin=physical minimum default: -1200 (may be a real number e.g. -1199.99)\n"
          "\n --amp=peak amplitude default: 1000 (may be a real number e.g. 999.99)\n"
          "\n --unit=physical dimension default: uV\n"
          "\n --digmax=digital maximum default: 32767 (EDF) or 8388607 (BDF) (integer only)\n"
          "\n --digmin=digital minimum default: -32768 (EDF) or -8388608 (BDF) (integer only)\n"
          "\n --offset=physical dc-offset default: 0\n"
          "\n --datrecs=number of datarecords that will be written into the file, has precedence over --len.\n"
          "\n --datrec-duration=duration of a datarecord in seconds default: 1 (may be a real number e.g. 0.25)\n"
          "                   effective samplerate and signal frequency will be inversely proportional to the datarecord duration\n"
          "\n --signals=number of signals default: 1 in case of multiple signals, signal parameters must be separated by a comma e.g.: --rate=1000,800,133\n"
          "\n --merge  merge all signals into one trace, requires equal samplerate and equal physical max/min and equal digital max/min and equal physical dimension (units) for all signals\n"
          "\n --help\n\n"
          " Note: decimal separator (if any) must be a dot, do not use a comma as a decimal separator\n\n"
        );
        return EXIT_SUCCESS;
      }
    }
  }

  if(optind < argc)
  {
    fprintf(stderr, "unrecognized argument(s):");
    while(optind < argc)
    {
      fprintf(stderr, " %s", argv[optind++]);
    }
    fprintf(stderr, "\n--help for help\n");
    return EXIT_FAILURE;
  }

  for(chan=0; chan<chns; chan++)
  {
    if(!digmax_set)
    {
      if(filetype == FILETYPE_EDF)
      {
        sig_par.digmax[chan] = 32767;
      }
      else
      {
        sig_par.digmax[chan] = 8388607;
      }
    }

    if(!digmin_set)
    {
      if(filetype == FILETYPE_EDF)
      {
        sig_par.digmin[chan] = -32768;
      }
      else
      {
        sig_par.digmin[chan] = -8388608;
      }
    }

    if(sig_par.physmax[chan] > 9999999.5)
    {
      fprintf(stderr, "error: physical maximum must be <= 9999999\n");
      return EXIT_FAILURE;
    }

    if(sig_par.physmin[chan] < -9999999.5)
    {
      fprintf(stderr, "error: physical minimum must be >= -9999999\n");
      return EXIT_FAILURE;
    }

    if(sig_par.peakamp[chan] <= 0.009999)
    {
      fprintf(stderr, "error: peak amplitude must be >= 0.01\n(amp: %f)\n", sig_par.peakamp[chan]);
      return EXIT_FAILURE;
    }

    if((sig_par.physmax[chan] < ((sig_par.peakamp[chan] * 1.05) + sig_par.dc_offset[chan])) || (sig_par.physmin[chan] > ((sig_par.peakamp[chan] * -1.05) + sig_par.dc_offset[chan])))
    {
      fprintf(stderr, "error: physical maximum must be higher than peak amplitude * 1.05 + DC-offset and physical minimum must be more lower than peak amplitude * -1.05 + DC-offset\n"
                      "peak amplitude: %f   physical maximum: %f\n", sig_par.peakamp[chan], sig_par.physmax[chan]);
      return EXIT_FAILURE;
    }

    if(filetype == FILETYPE_BDF)
    {
      if(sig_par.digmax[chan] > 8388607)
      {
        fprintf(stderr, "error: digital maximum must be <= 8388607\n");
        return EXIT_FAILURE;
      }

      if(sig_par.digmin[chan] < -8388608)
      {
        fprintf(stderr, "error: digital minimum must be >= -8388608\n");
        return EXIT_FAILURE;
      }
    }
    else if(filetype == FILETYPE_EDF)
      {
        if(sig_par.digmax[chan] > 32767)
        {
          fprintf(stderr, "error: digital maximum must be <= 32767\n");
          return EXIT_FAILURE;
        }

        if(sig_par.digmin[chan] < -32768)
        {
          fprintf(stderr, "error: digital minimum must be >= -32768\n");
          return EXIT_FAILURE;
        }
      }

      if(sig_par.digmin[chan] >= sig_par.digmax[chan])
      {
        fprintf(stderr, "error: digital minimum must be less than digital maximum\n");
        return EXIT_FAILURE;
      }

    if(merge_set)
    {
      if(chan)
      {
        if(sig_par.sf[chan] != sig_par.sf[0])
        {
          fprintf(stderr, "error: option --merge requires that all signals have equal samplerate\n");
          return EXIT_FAILURE;
        }

        if(sig_par.physmax[chan] != sig_par.physmax[0])
        {
          fprintf(stderr, "error: option --merge requires that all signals have equal value for physical maximum\n");
          return EXIT_FAILURE;
        }

        if(sig_par.physmin[chan] != sig_par.physmin[0])
        {
          fprintf(stderr, "error: option --merge requires that all signals have equal value for physical minimum\n");
          return EXIT_FAILURE;
        }

        if(sig_par.digmax[chan] != sig_par.digmax[0])
        {
          fprintf(stderr, "error: option --merge requires that all signals have equal value for digital maximum\n");
          return EXIT_FAILURE;
        }

        if(sig_par.digmin[chan] != sig_par.digmin[0])
        {
          fprintf(stderr, "error: option --merge requires that all signals have equal for digital minimum\n");
          return EXIT_FAILURE;
        }

        if(strcmp(sig_par.physdim[chan], sig_par.physdim[0]))
        {
          fprintf(stderr, "error: option --merge requires that all signals have equal physical dimension (units)\n");
          return EXIT_FAILURE;
        }
      }
    }
  }

  if(datrecduration_set)
  {
    duration = (duration / datrecduration) + 0.5;
    if(duration < 1)
    {
      duration = 1;
    }

    if(!datrecs_set)
    {
      datrecs = duration;
    }
  }
  else
  {
    datrecs = duration;
  }

  for(i=0; i<EDF_MAX_CHNS; i++)
  {
    sig_par.buf[i] = NULL;
  }

  for(i=0; i<chns; i++)
  {
    sig_par.buf[i] = (double *)calloc(1, sizeof(double[sig_par.sf[i]]));
    if(sig_par.buf[i]==NULL)
    {
      fprintf(stderr, "Malloc error line %i file %s\n", __LINE__, __FILE__);
      return EXIT_FAILURE;
    }

    if((sig_par.waveform[i] == WAVE_WHITE_NOISE) || (sig_par.waveform[i] == WAVE_PINK_NOISE))  /* white or pink noise */
    {
      sig_par.randbuf[i] = (int *)calloc(1, sizeof(int[sig_par.sf[i]]));
      if(sig_par.randbuf[i]==NULL)
      {
        fprintf(stderr, "Malloc error line %i file %s\n", __LINE__, __FILE__);
        return EXIT_FAILURE;
      }
    }
  }

  if(chns == 1)
  {
    snprintf(str, 1024, "edfgenerator_%fHz_%s_%fHz",
             sig_par.sf[0] / datrecduration, waveforms_str[sig_par.waveform[0]], sig_par.signalfreq[0] / datrecduration);

    if((sig_par.waveform[0] >= WAVE_SQUARE) && (sig_par.waveform[0]<= WAVE_TRIANGLE))
    {
      snprintf(str + strlen(str), 1024, "_%fpct", sig_par.dutycycle[0]);
    }

    if(sig_par.waveform[0] <= WAVE_TRIANGLE)
    {
      snprintf(str + strlen(str), 1024, "_%fdegr", sig_par.phase[0]);
    }

    remove_trailing_zeros(str);
  }
  else
  {
    snprintf(str, 1024, "edfgenerator");
  }

  if(merge_set)
  {
    edf_chns = 1;
  }
  else
  {
    edf_chns = chns;
  }

  if(filetype == FILETYPE_BDF)
  {
    strlcat(str, ".bdf", 1024);

    hdl = edfopen_file_writeonly(str, EDFLIB_FILETYPE_BDFPLUS, edf_chns);
  }
  else if(filetype == FILETYPE_EDF)
    {
      strlcat(str, ".edf", 1024);

      hdl = edfopen_file_writeonly(str, EDFLIB_FILETYPE_EDFPLUS, edf_chns);
    }

  if(hdl<0)
  {
    fprintf(stderr, "error: edfopen_file_writeonly() line %i file %s\n", __LINE__, __FILE__);

    return EXIT_FAILURE;
  }

  if(datrecduration_set)
  {
    if(edf_set_datarecord_duration(hdl, nearbyint(datrecduration * 100000)))
    {
      fprintf(stderr, "error: edf_set_datarecord_duration() line %i file %s\n", __LINE__, __FILE__);

      return EXIT_FAILURE;
    }
  }

  if(edf_set_startdatetime(hdl, 2000, 1, 1, 0, 0, 0))
  {
    fprintf(stderr, "error: edf_set_startdatetime() line %i file %s\n", __LINE__, __FILE__);

    return EXIT_FAILURE;
  }

  for(i=0; i<edf_chns; i++)
  {
    if(edf_set_samplefrequency(hdl, i, sig_par.sf[i]))
    {
      fprintf(stderr, "error: edf_set_samplefrequency() line %i file %s\n", __LINE__, __FILE__);

      return EXIT_FAILURE;
    }

    if(edf_set_digital_maximum(hdl, i, sig_par.digmax[i]))
    {
      fprintf(stderr, "error: edf_set_digital_maximum() line %i file %s\n", __LINE__, __FILE__);

      return EXIT_FAILURE;
    }

    if(edf_set_digital_minimum(hdl, i, sig_par.digmin[i]))
    {
      fprintf(stderr, "error: edf_set_digital_minimum() line %i file %s\n", __LINE__, __FILE__);

      return EXIT_FAILURE;
    }

    if(edf_set_physical_maximum(hdl, i, sig_par.physmax[i]))
    {
      fprintf(stderr, "error: edf_set_physical_maximum() line %i file %s\n", __LINE__, __FILE__);

      return EXIT_FAILURE;
    }


    if(edf_set_physical_minimum(hdl, i, sig_par.physmin[i]))
    {
      fprintf(stderr, "error: edf_set_physical_minimum() line %i file %s\n", __LINE__, __FILE__);

      return EXIT_FAILURE;
    }

    if(edf_set_physical_dimension(hdl, i, sig_par.physdim[i]))
    {
      fprintf(stderr, "error: edf_set_physical_dimension() line %i file %s\n", __LINE__, __FILE__);

      return EXIT_FAILURE;
    }

    if(sig_par.waveform[i] == WAVE_SINE)
    {
      snprintf(str, 18, "sine %.2fHz", sig_par.signalfreq[i] / datrecduration);
    }
    else if(sig_par.waveform[i] == WAVE_SQUARE)
      {
        snprintf(str, 18, "square %.2fHz", sig_par.signalfreq[i] / datrecduration);
      }
      else if(sig_par.waveform[i] == WAVE_RAMP)
        {
          snprintf(str, 18, "ramp %.2fHz", sig_par.signalfreq[i] / datrecduration);
        }
        else if(sig_par.waveform[i] == WAVE_TRIANGLE)
          {
            snprintf(str, 18, "triangle %.2fHz", sig_par.signalfreq[i] / datrecduration);
          }
          else if(sig_par.waveform[i] == WAVE_WHITE_NOISE)
            {
              snprintf(str, 18, "white noise");
            }
            else if(sig_par.waveform[i] == WAVE_PINK_NOISE)
              {
                snprintf(str, 18, "pink noise");
              }

    remove_trailing_zeros(str);

    if(edf_set_label(hdl, i, str))
    {
      fprintf(stderr, "error: edf_set_label() line %i file %s\n", __LINE__, __FILE__);

      return EXIT_FAILURE;
    }
  }

  for(i=0; i<chns; i++)
  {
    sig_par.w[i] = M_PI * 2.0;

    sig_par.q[i] = 1.0 / sig_par.sf[i];

    sig_par.w[i] /= (sig_par.sf[i] / sig_par.signalfreq[i]);

    sig_par.sine_1[i] = ((M_PI * 2.0) * sig_par.phase[i]) / 360;

    sig_par.square_1[i] = ((1.0 / sig_par.signalfreq[i]) * sig_par.phase[i]) / 360;

    sig_par.triangle_1[i] = ((1.0 / sig_par.signalfreq[i]) * sig_par.phase[i]) / 360;
  }

  for(i=0; i<chns; i++)
  {
    if((sig_par.waveform[i] == WAVE_WHITE_NOISE) || (sig_par.waveform[i] == WAVE_PINK_NOISE))
    {
      fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK);
      if(fd < 0)
      {
        fprintf(stderr, "error: cannot open /dev/urandom    line %i file %s\n", __LINE__, __FILE__);

        return EXIT_FAILURE;
      }

      break;
    }
  }

  if(merge_set)
  {
    merge_buf = malloc(sizeof(double[sig_par.sf[0]]));
    if(merge_buf == NULL)
    {
      fprintf(stderr, "Malloc error line %i file %s\n", __LINE__, __FILE__);
      return EXIT_FAILURE;
    }
  }

  for(j=0; j<datrecs; j++)
  {
    if(merge_set)
    {
      memset(merge_buf, 0, sizeof(double[sig_par.sf[0]]));
    }

    for(chan=0; chan<chns; chan++)
    {
      if(sig_par.waveform[chan] == WAVE_SINE)
      {
        for(i=0; i<sig_par.sf[chan]; i++)
        {
          sig_par.buf[chan][i] = (sin(sig_par.sine_1[chan]) * sig_par.peakamp[chan]) + sig_par.dc_offset[chan];

          sig_par.sine_1[chan] += sig_par.w[chan];
        }
      }
      else if(sig_par.waveform[chan] == WAVE_SQUARE)
        {
          for(i=0; i<sig_par.sf[chan]; i++)
          {
            ftmp = fmod(sig_par.square_1[chan], 1.0 / sig_par.signalfreq[chan]);

            if((ftmp * sig_par.signalfreq[chan]) < (sig_par.dutycycle[chan] / 100.0))
            {
              sig_par.buf[chan][i] = sig_par.peakamp[chan];
            }
            else
            {
              sig_par.buf[chan][i] = -sig_par.peakamp[chan];
            }
            sig_par.buf[chan][i] += sig_par.dc_offset[chan];

            sig_par.square_1[chan] += sig_par.q[chan];
          }
        }
        else if(sig_par.waveform[chan] == WAVE_RAMP)
          {
            for(i=0; i<sig_par.sf[chan]; i++)
            {
              ftmp = fmod(sig_par.triangle_1[chan], 1.0 / sig_par.signalfreq[chan]);

              if((ftmp * sig_par.signalfreq[chan]) < (sig_par.dutycycle[chan] / 100.0))
              {
                sig_par.buf[chan][i] = sig_par.peakamp[chan] * (200.0 / sig_par.dutycycle[chan]) * ftmp * sig_par.signalfreq[chan] - sig_par.peakamp[chan];
              }
              else
              {
                sig_par.buf[chan][i] = -sig_par.peakamp[chan];
              }
              sig_par.buf[chan][i] += sig_par.dc_offset[chan];

              sig_par.triangle_1[chan] += sig_par.q[chan];
            }
          }
          else if(sig_par.waveform[chan] == WAVE_TRIANGLE)
            {
              for(i=0; i<sig_par.sf[chan]; i++)
              {
                ftmp = fmod(sig_par.triangle_1[chan], 1.0 / sig_par.signalfreq[chan]);

                if((ftmp * sig_par.signalfreq[chan]) < (sig_par.dutycycle[chan] / 200.0))
                {
                  sig_par.buf[chan][i] = sig_par.peakamp[chan] * (400.0 / sig_par.dutycycle[chan]) * ftmp * sig_par.signalfreq[chan] - sig_par.peakamp[chan];
                  sig_par.buf[chan][i] += sig_par.dc_offset[chan];
                }
                else if((ftmp * sig_par.signalfreq[chan]) < (sig_par.dutycycle[chan] / 100.0))
                  {
                    sig_par.buf[chan][i] = sig_par.peakamp[chan] * (400.0 / sig_par.dutycycle[chan]) * ((sig_par.dutycycle[chan] / 100.0) - (ftmp * sig_par.signalfreq[chan])) - sig_par.peakamp[chan];
                  }
                  else
                  {
                    sig_par.buf[chan][i] = -sig_par.peakamp[chan];
                  }
                  sig_par.buf[chan][i] += sig_par.dc_offset[chan];

                  sig_par.triangle_1[chan] += sig_par.q[chan];
              }
            }
            else if((sig_par.waveform[chan] == WAVE_WHITE_NOISE) || (sig_par.waveform[chan] == WAVE_PINK_NOISE))
              {
                err = read(fd, sig_par.randbuf[chan], sizeof(int[sig_par.sf[chan]]));
                if(err != (sig_par.sf[chan] * 4))
                {
                  perror(NULL);
                  fprintf(stderr, "error: read() returned %i   line %i\n", err, __LINE__);

                  return EXIT_FAILURE;
                }

                if(sig_par.waveform[chan] == WAVE_WHITE_NOISE)
                {
                  for(i=0; i<sig_par.sf[chan]; i++)
                  {
                    sig_par.buf[chan][i] = (sig_par.randbuf[chan][i] % ((int)(sig_par.peakamp[chan] * 100.0))) / 100.0;
                    sig_par.buf[chan][i] += sig_par.dc_offset[chan];
                  }
                }
                else if(sig_par.waveform[chan] == WAVE_PINK_NOISE)
                  {
  /* This is an approximation to a -10dB/decade (-3dB/octave) filter using a weighted sum
   * of first order filters. It is accurate to within +/-0.05dB above 9.2Hz
   * (44100Hz sampling rate). Unity gain is at Nyquist, but can be adjusted
   * by scaling the numbers at the end of each line.
   * http://www.firstpr.com.au/dsp/pink-noise/
   */
                    for(i=0; i<sig_par.sf[chan]; i++)
                    {
                      white_noise = (sig_par.randbuf[chan][i] % ((int)(sig_par.peakamp[chan] * 100.0))) / 600.0;
                      sig_par.b0[chan] = 0.99886 * sig_par.b0[chan] + white_noise * 0.0555179;
                      sig_par.b1[chan] = 0.99332 * sig_par.b1[chan] + white_noise * 0.0750759;
                      sig_par.b2[chan] = 0.96900 * sig_par.b2[chan] + white_noise * 0.1538520;
                      sig_par.b3[chan] = 0.86650 * sig_par.b3[chan] + white_noise * 0.3104856;
                      sig_par.b4[chan] = 0.55000 * sig_par.b4[chan] + white_noise * 0.5329522;
                      sig_par.b5[chan] = -0.7616 * sig_par.b5[chan] - white_noise * 0.0168980;
                      sig_par.buf[chan][i] = sig_par.b0[chan] + sig_par.b1[chan] + sig_par.b2[chan] + sig_par.b3[chan] + sig_par.b4[chan] + sig_par.b5[chan] + sig_par.b6[chan] + white_noise * 0.5362;
                      sig_par.buf[chan][i] += sig_par.dc_offset[chan];
                      sig_par.b6[chan] = white_noise * 0.115926;
                    }
                  }
              }

      if(merge_set)
      {
        for(i=0; i<sig_par.sf[chan]; i++)
        {
          merge_buf[i] += sig_par.buf[chan][i];
        }
      }
      else
      {
        if(edfwrite_physical_samples(hdl, sig_par.buf[chan]))
        {
          fprintf(stderr, "error: edfwrite_physical_samples() line %i file %s\n", __LINE__, __FILE__);
          return EXIT_FAILURE;
        }
      }
    }

    if(merge_set)
    {
      if(edfwrite_physical_samples(hdl, merge_buf))
      {
        fprintf(stderr, "error: edfwrite_physical_samples() line %i file %s\n", __LINE__, __FILE__);
        return EXIT_FAILURE;
      }
    }
  }

  if(fd >= 0)
  {
    close(fd);
  }

  edfclose_file(hdl);

  for(i=0; i<chns; i++)
  {
    free(sig_par.buf[i]);
    free(sig_par.randbuf[i]);
  }
  free(merge_buf);

  return EXIT_SUCCESS;
}






















