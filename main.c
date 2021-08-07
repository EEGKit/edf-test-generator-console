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
#define PROGRAM_VERSION    "1.05"

#define FILETYPE_EDF       0
#define FILETYPE_BDF       1

#define WAVE_SINE          0
#define WAVE_SQUARE        1
#define WAVE_RAMP          2
#define WAVE_TRIANGLE      3
#define WAVE_WHITE_NOISE   4
#define WAVE_PINK_NOISE    5




int main(int argc, char **argv)
{
  int i, j,
      err,
      option_index=0,
      c=0,
      fd=-1,
      hdl=-1,
      filetype=0,
      duration=30,
      sf=500,
      digmax=0,
      digmin=0,
      waveform=0,
      *randbuf=NULL,
      digmax_set=0,
      digmin_set=0;

  double *buf=NULL,
         w=1,
         q=1,
         sine_1=0,
         square_1=0,
         triangle_1=0,
         signalfreq=10,
         physmax=1200,
         physmin=-1200,
         peakamp=1000,
         dutycycle=50,
         ftmp,
         b0, b1, b2, b3, b4, b5, b6,
         white_noise,
         dc_offset=0;

  char physdim[32]="uV",
       str[1024]="";

  const char waveforms_str[6][16]={"sine", "square", "ramp", "triangle", "white-noise", "pink-noise"};

  setlocale(LC_ALL, "C");

  setlinebuf(stdout);
  setlinebuf(stderr);

  struct option long_options[] = {
    {"type", required_argument, 0, 0},
    {"len", required_argument, 0, 0},
    {"rate", required_argument, 0, 0},
    {"freq", required_argument, 0, 0},
    {"wave", required_argument, 0, 0},
    {"dcycle", required_argument, 0, 0},
    {"physmax", required_argument, 0, 0},
    {"physmin", required_argument, 0, 0},
    {"amp", required_argument, 0, 0},
    {"unit", required_argument, 0, 0},
    {"digmax", required_argument, 0, 0},
    {"digmin", required_argument, 0, 0},
    {"offset", required_argument, 0, 0},
    {"help", no_argument, 0, 0},
    {0, 0, 0, 0}
  };

  while(1)
  {
    c = getopt_long_only(argc, argv, "", long_options, &option_index);

    if(c == -1)  break;

    if(c == 0)
    {
      if(option_index < 13)
      {
        if(optarg == NULL)
        {
          fprintf(stderr, "missing value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
      }

      if(option_index == 0)
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

      if(option_index == 1)
      {
        duration = atoi(optarg);
        if(duration < 1)
        {
          fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
      }

      if(option_index == 2)
      {
        sf = atoi(optarg);
        if(sf < 1)
        {
          fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
      }

      if(option_index == 3)
      {
        signalfreq = atof(optarg);
        if((signalfreq < 9.99999e-2) || (signalfreq > (sf / 4.0 + 1e-6)))
        {
          fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
      }

      if(option_index == 4)
      {
        for(i=0; i<6; i++)
        {
          if(!strcmp(optarg, waveforms_str[i]))
          {
            waveform = i;
            break;
          }
        }
        if(i==6)
        {
          fprintf(stderr, "unrecognized value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
      }

      if(option_index == 5)
      {
        dutycycle = atof(optarg);
        if((dutycycle < 0.099999) || (dutycycle > 100.000001))
        {
          fprintf(stderr, "illegal value for option %s\n", long_options[option_index].name);
          return EXIT_FAILURE;
        }
      }

      if(option_index == 6)
      {
        physmax = atof(optarg);
      }

      if(option_index == 7)
      {
        physmin = atof(optarg);
      }

      if(option_index == 8)
      {
        peakamp = atof(optarg);
      }

      if(option_index == 9)
      {
        strlcpy(physdim, optarg, 16);
      }

      if(option_index == 10)
      {
        digmax = atoi(optarg);
        digmax_set = 1;
      }

      if(option_index == 11)
      {
        digmin = atoi(optarg);
        digmin_set = 1;
      }

      if(option_index == 12)
      {
        dc_offset = atof(optarg);
      }

      if(option_index == 13)
      {
        fprintf(stdout, "\n EDF generator version " PROGRAM_VERSION
          " Copyright (c) 2020 - 2021 Teunis van Beelen   email: teuniz@protonmail.com\n"
          "\n Usage: " PROGRAM_NAME " [OPTION]...\n"
          "\n options:\n"
          "\n --type=edf|bdf default: edf\n"
          "\n --len=file duration in seconds default: 30\n"
          "\n --rate=samplerate in herz default: 500\n"
          "\n --freq=signal frequency in herz default: 10\n"
          "\n --wave=sine | square | ramp | triangle | white-noise | pink-noise default: sine\n"
          "\n --dcycle=dutycycle: 0.1-100%% default: 50\n"
          "\n --physmax=physical maximum default: 1200\n"
          "\n --physmin=physical minimum default: -1200\n"
          "\n --amp=peak amplitude default: 1000\n"
          "\n --unit=physical dimension default: uV\n"
          "\n --digmax=digital maximum default: 32767 (EDF) or 8388607 (BDF)\n"
          "\n --digmin=digital minimum default: -32768 (EDF) or -8388608 (BDF)\n"
          "\n --offset=physical dc-offset default: 0\n"
          "\n --help\n\n"
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

  if(!digmax_set)
  {
    if(filetype == FILETYPE_EDF)
    {
      digmax = 32767;
    }
    else
    {
      digmax = 8388607;
    }
  }

  if(!digmin_set)
  {
    if(filetype == FILETYPE_EDF)
    {
      digmin = -32768;
    }
    else
    {
      digmin = -8388608;
    }
  }

  if(physmax > 9999999.5)
  {
    fprintf(stderr, "error: physical maximum must be <= 9999999\n");
    return EXIT_FAILURE;
  }

  if(physmin < -9999999.5)
  {
    fprintf(stderr, "error: physical minimum must be >= -9999999\n");
    return EXIT_FAILURE;
  }

  if(peakamp < 0.9999)
  {
    fprintf(stderr, "error: peak amplitude must be >= 1\n");
    return EXIT_FAILURE;
  }

  if((physmax < ((peakamp * 1.05) + dc_offset)) || (physmin > ((peakamp * -1.05) + dc_offset)))
  {
    fprintf(stderr, "error: physical maximum must be higher than peak amplitude * 1.05 + DC-offset and physical minimum must be more lower than peak amplitude * -1.05 + DC-offset\n");
    return EXIT_FAILURE;
  }

  if(filetype == FILETYPE_BDF)
  {
    if(digmax > 8388607)
    {
      fprintf(stderr, "error: digital maximum must be <= 8388607\n");
      return EXIT_FAILURE;
    }

    if(digmin < -8388608)
    {
      fprintf(stderr, "error: digital minimum must be >= -8388608\n");
      return EXIT_FAILURE;
    }
  }
  else if(filetype == FILETYPE_EDF)
    {
      if(digmax > 32767)
      {
        fprintf(stderr, "error: digital maximum must be <= 32767\n");
        return EXIT_FAILURE;
      }

      if(digmin < -32768)
      {
        fprintf(stderr, "error: digital minimum must be >= -32768\n");
        return EXIT_FAILURE;
      }
    }

    if(digmin >= digmax)
    {
      fprintf(stderr, "error: digital minimum must be less than digital maximum\n");
      return EXIT_FAILURE;
    }

  buf = (double *)calloc(1, sf * sizeof(double));
  if(buf==NULL)
  {
    fprintf(stderr, "Malloc error line %i\n", __LINE__);
    return EXIT_FAILURE;
  }

  randbuf = (int *)calloc(1, sf * sizeof(int));
  if(randbuf==NULL)
  {
    fprintf(stderr, "Malloc error line %i\n", __LINE__);
    return EXIT_FAILURE;
  }

  sprintf(str, "edfgenerator_%iHz_%s_%f", sf, waveforms_str[waveform], signalfreq);

  remove_trailing_zeros(str);

  strcat(str, "Hz");

  if(waveform)
  {
    sprintf(str + strlen(str), "_%f", dutycycle);

    remove_trailing_zeros(str);

    strcat(str, "pct");
  }

  if(filetype == FILETYPE_BDF)
  {
    strcat(str, ".bdf");

    hdl = edfopen_file_writeonly(str, EDFLIB_FILETYPE_BDFPLUS, 1);
  }
  else if(filetype == FILETYPE_EDF)
    {
      strcat(str, ".edf");

      hdl = edfopen_file_writeonly(str, EDFLIB_FILETYPE_EDFPLUS, 1);
    }

  if(hdl<0)
  {
    fprintf(stderr, "error: edfopen_file_writeonly() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_startdatetime(hdl, 2000, 1, 1, 0, 0, 0))
  {
    fprintf(stderr, "error: edf_set_startdatetime() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_samplefrequency(hdl, 0, sf))
  {
    fprintf(stderr, "error: edf_set_samplefrequency() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_digital_maximum(hdl, 0, digmax))
  {
    fprintf(stderr, "error: edf_set_digital_maximum() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_digital_minimum(hdl, 0, digmin))
  {
    fprintf(stderr, "error: edf_set_digital_minimum() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_physical_maximum(hdl, 0, physmax))
  {
    fprintf(stderr, "error: edf_set_physical_maximum() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }


  if(edf_set_physical_minimum(hdl, 0, physmin))
  {
    fprintf(stderr, "error: edf_set_physical_minimum() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_physical_dimension(hdl, 0, physdim))
  {
    fprintf(stderr, "error: edf_set_physical_dimension() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(waveform == WAVE_SINE)
  {
    snprintf(str, 18, "sine %.2fHz", signalfreq);
  }
  else if(waveform == WAVE_SQUARE)
    {
      snprintf(str, 18, "square %.2fHz", signalfreq);
    }
    else if(waveform == WAVE_RAMP)
      {
        snprintf(str, 18, "ramp %.2fHz", signalfreq);
      }
      else if(waveform == WAVE_TRIANGLE)
        {
          snprintf(str, 18, "triangle %.2fHz", signalfreq);
        }
        else if(waveform == WAVE_WHITE_NOISE)
          {
            snprintf(str, 18, "white noise");
          }
          else if(waveform == WAVE_PINK_NOISE)
            {
              snprintf(str, 18, "pink noise");
            }

  remove_trailing_zeros(str);

  if(edf_set_label(hdl, 0, str))
  {
    fprintf(stderr, "error: edf_set_label() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  sine_1 = 0.0;

  square_1 = 0.0;

  w = M_PI * 2.0;

  q = 1.0 / sf;

  b0 = 0;
  b1 = 0;
  b2 = 0;
  b3 = 0;
  b4 = 0;
  b5 = 0;
  b6 = 0;

  w /= (sf / signalfreq);

  if((waveform == WAVE_WHITE_NOISE) || (waveform == WAVE_PINK_NOISE))
  {
    fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK);
    if(fd < 0)
    {
      fprintf(stderr, "error: open /dev/urandom line %i\n", __LINE__);

      return EXIT_FAILURE;
    }
  }

  for(j=0; j<duration; j++)
  {
    if(waveform == WAVE_SINE)
    {
      for(i=0; i<sf; i++)
      {
        sine_1 += w;

        buf[i] = (sin(sine_1) * peakamp) + dc_offset;
      }
    }
    else if(waveform == WAVE_SQUARE)
      {
        for(i=0; i<sf; i++)
        {
          square_1 += q;

          ftmp = fmod(square_1, 1.0 / signalfreq);

          if((ftmp * signalfreq) < (dutycycle / 100.0))
          {
            buf[i] = peakamp;
          }
          else
          {
            buf[i] = -peakamp;
          }
          buf[i] += dc_offset;
        }
      }
      else if(waveform == WAVE_RAMP)
        {
          for(i=0; i<sf; i++)
          {
            triangle_1 += q;

            ftmp = fmod(triangle_1, 1.0 / signalfreq);

            if((ftmp * signalfreq) < (dutycycle / 100.0))
            {
              buf[i] = peakamp * (200.0 / dutycycle) * ftmp * signalfreq - peakamp;
            }
            else
            {
              buf[i] = -peakamp;
            }
            buf[i] += dc_offset;
          }
        }
        else if(waveform == WAVE_TRIANGLE)
          {
            for(i=0; i<sf; i++)
            {
              triangle_1 += q;

              ftmp = fmod(triangle_1, 1.0 / signalfreq);

              if((ftmp * signalfreq) < (dutycycle / 200.0))
              {
                buf[i] = peakamp * (400.0 / dutycycle) * ftmp * signalfreq - peakamp;
                buf[i] += dc_offset;
              }
              else if((ftmp * signalfreq) < (dutycycle / 100.0))
                {
                  buf[i] = peakamp * (400.0 / dutycycle) * ((dutycycle / 100.0) - (ftmp * signalfreq)) - peakamp;
                }
                else
                {
                  buf[i] = -peakamp;
                }
                buf[i] += dc_offset;
            }
          }
          else if((waveform == WAVE_WHITE_NOISE) || (waveform == WAVE_PINK_NOISE))
            {
              err = read(fd, randbuf, sf * sizeof(int));
              if(err != (sf * 4))
              {
                fprintf(stderr, "error: read() line %i\n", __LINE__);

                return EXIT_FAILURE;
              }

              if(waveform == WAVE_WHITE_NOISE)
              {
                for(i=0; i<sf; i++)
                {
                  buf[i] = (randbuf[i] % ((int)(peakamp * 100.0))) / 100.0;
                  buf[i] += dc_offset;
                }
              }
              else if(waveform == WAVE_PINK_NOISE)
                {
/* This is an approximation to a -10dB/decade (-3dB/octave) filter using a weighted sum
 * of first order filters. It is accurate to within +/-0.05dB above 9.2Hz
 * (44100Hz sampling rate). Unity gain is at Nyquist, but can be adjusted
 * by scaling the numbers at the end of each line.
 * http://www.firstpr.com.au/dsp/pink-noise/
 */
                  for(i=0; i<sf; i++)
                  {
                    white_noise = (randbuf[i] % ((int)(peakamp * 100.0))) / 600.0;
                    b0 = 0.99886 * b0 + white_noise * 0.0555179;
                    b1 = 0.99332 * b1 + white_noise * 0.0750759;
                    b2 = 0.96900 * b2 + white_noise * 0.1538520;
                    b3 = 0.86650 * b3 + white_noise * 0.3104856;
                    b4 = 0.55000 * b4 + white_noise * 0.5329522;
                    b5 = -0.7616 * b5 - white_noise * 0.0168980;
                    buf[i] = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white_noise * 0.5362;
                    buf[i] += dc_offset;
                    b6 = white_noise * 0.115926;
                  }
                }
            }

    if(edfwrite_physical_samples(hdl, buf))
    {
      fprintf(stderr, "error: edfwrite_physical_samples() line %i\n", __LINE__);

      return EXIT_FAILURE;
    }
  }

  if((waveform == WAVE_WHITE_NOISE) || (waveform == WAVE_PINK_NOISE))
  {
    close(fd);
  }

  edfclose_file(hdl);

  free(buf);
  free(randbuf);

  return EXIT_SUCCESS;
}






















