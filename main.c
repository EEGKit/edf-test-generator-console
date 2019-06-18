/*
***************************************************************************
*
* Author: Teunis van Beelen
*
* Copyright (C) 2018 - 2019 Teunis van Beelen
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
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "edflib.h"

#define PROGRAM_VERSION    "1.01"

#define FILETYPE_EDF       0
#define FILETYPE_BDF       1

#define WAVE_SINE          0
#define WAVE_SQUARE        1
#define WAVE_RAMP          2
#define WAVE_TRIANGLE      3
#define WAVE_WHITE_NOISE   4
#define WAVE_PINK_NOISE    5


void remove_trailing_zeros(char *);



int main(int argc, char **argv)
{
  int i, j,
      err,
      fd=-1,
      hdl=-1,
      filetype=0,
      duration=1,
      sf=1,
      digmax=0,
      digmin=0,
      waveform,
      *randbuf=NULL;

  double *buf=NULL,
         w=1,
         q=1,
         sine_1=0,
         square_1=0,
         triangle_1=0,
         signalfreq=1,
         physmax=1200,
         physmin=-1200,
         peakamp=1000,
         dutycycle=50,
         ftmp,
         b0, b1, b2, b3, b4, b5, b6,
         white_noise;

  char physdim[32]="uV",
       str[1024]="";

  const char waveforms_str[6][16]={"sine", "square", "ramp", "triangle", "white-noise", "pink-noise"};

  if((argc != 7) && (argc != 11) && (argc != 13))
  {
    printf("\nEDF generator version: " PROGRAM_VERSION "   Author: Teunis van Beelen   License: GPLv3\n"
           "\nusage: edf_generator <filetype edf or bdf> <duration in seconds> <sample frequency> <signal frequency Hz> <waveform sine, square, ramp, triangle white-noise or pink-noise> <dutycycle %%>"
           " [<physical max> <physical min> <peak amplitude> <physical dimension> [<digital max> <digital min>]]\n"
           "\nexample 1: edf_generator edf 10 1000 1 sine 50\n"
           "EDF file, 10 seconds recording length, 1KHz samplerate, sine wave of 1Hz\n"
           "\nexample 2: edf_generator edf 30 113 3.2 square 50 3200 -3200 100 uV\n"
           "EDF file, 30 seconds recording length, 113Hz samplerate, square wave of 3.2Hz, duty cycle 50%%,\n"
           "3200 physical maximum, -3200 physical minimum, 100uV peak amplitude\n"
           "\nexample 3: edf_generator bdf 10 1000 1.5 ramp 10.5 1000 -1000 300 mV 1048575 -1048576\n"
           "BDF file, 10 seconds recording length, 1KHz samplerate, triangular wave of 1.5Hz, duty cycle 10.5%%,\n"
           "1000 physical maximum, -1000 physical minimum, 300mV peak amplitude, 1048575 digital maximum, -1048576 digital minimum\n"
           "\nexample 4: edf_generator bdf 20 512 3.7 triangle 100 1000 -1000 300 uV\n"
           "BDF file, 20 seconds recording length, 512Hz samplerate, triangular wave of 3.7Hz, duty cycle 100%%,\n"
           "1000 physical maximum, -1000 physical minimum, 300uV peak amplitude\n"
           "\nexample 5: edf_generator edf 60 10000 1 white-noise 50\n"
           "EDF file, 60 seconds recording length, 10KHz samplerate, white noise\n"
           "\n");

    return EXIT_FAILURE;
  }

  if(!strcmp(argv[1], "edf"))
  {
    filetype = FILETYPE_EDF;

    digmax = 32767;

    digmin = -32768;
  }
  else if(!strcmp(argv[1], "bdf"))
    {
      filetype = FILETYPE_BDF;

      digmax = 8388607;

      digmin = -8388608;
    }
    else
    {
      printf("error: invalid filetype %s\n", argv[1]);

      return EXIT_FAILURE;
    }

  duration = atoi(argv[2]);
  if((duration < 1) || (duration > 3600))
  {
    printf("error: invalid duration %i seconds\n", duration);

    return EXIT_FAILURE;
  }

  sf = atoi(argv[3]);
  if((sf < 4) || (sf > 200000))
  {
    printf("error: invalid sample frequency %i Hz\n", sf);

    return EXIT_FAILURE;
  }

  signalfreq = atof(argv[4]);
  if((signalfreq < 9.99999e-2) || (signalfreq > (sf / 4.0 + 1e-6)))
  {
    printf("error: invalid signal frequency %f Hz\n", signalfreq);

    return EXIT_FAILURE;
  }

  if(!strcmp(argv[5], "sine"))
  {
    waveform = WAVE_SINE;
  }
  else if(!strcmp(argv[5], "square"))
    {
      waveform = WAVE_SQUARE;
    }
    else if(!strcmp(argv[5], "ramp"))
      {
        waveform = WAVE_RAMP;
      }
      else if(!strcmp(argv[5], "triangle"))
        {
          waveform = WAVE_TRIANGLE;
        }
        else if(!strcmp(argv[5], "white-noise"))
          {
            waveform = WAVE_WHITE_NOISE;
          }
          else if(!strcmp(argv[5], "pink-noise"))
            {
              waveform = WAVE_PINK_NOISE;
            }
            else
            {
              printf("error: invalid waveform %s\n", argv[1]);

              return EXIT_FAILURE;
            }

  dutycycle = atof(argv[6]);
  if((dutycycle < 0.099999) || (dutycycle > 100.000001))
  {
    printf("error: invalid duty cycle %f %%\n", dutycycle);

    return EXIT_FAILURE;
  }

  if((argc == 11) || (argc == 13))
  {
    physmax = atof(argv[7]);

    physmin = atof(argv[8]);

    peakamp = atof(argv[9]);

    strncpy(physdim, argv[10], 16);

    physdim[16] = 0;

    if(physmax > 9999999.5)
    {
      printf("error: physical maximum must be <= 9999999\n");

      return EXIT_FAILURE;
    }

    if(physmin < -9999999.5)
    {
      printf("error: physical minimum must be >= -9999999\n");

      return EXIT_FAILURE;
    }

    if(peakamp < 0.9999)
    {
      printf("error: peak amplitude must be >= 1\n");

      return EXIT_FAILURE;
    }

    if((physmax < (peakamp * 1.05)) || (physmin > (peakamp * -1.05)))
    {
      printf("error: physical maximum must be higher than peak amplitude * 1.05 and physical minimum must be more lower than peak amplitude * -1.05\n");

      return EXIT_FAILURE;
    }
  }

  if(argc == 13)
  {
    digmax = atoi(argv[11]);

    digmin = atoi(argv[12]);

    if(filetype == FILETYPE_BDF)
    {
      if(digmax > 8388607)
      {
        printf("error: digital max must be <= 8388607\n");

        return EXIT_FAILURE;
      }

      if(digmin < -8388608)
      {
        printf("error: digital min must be >= -8388608\n");

        return EXIT_FAILURE;
      }
    }
    else if(filetype == FILETYPE_EDF)
      {
        if(digmax > 32767)
        {
          printf("error: digital max must be <= 32767\n");

          return EXIT_FAILURE;
        }

        if(digmin < -32768)
        {
          printf("error: digital min must be >= -32768\n");

          return EXIT_FAILURE;
        }
      }

    if(digmin >= digmax)
    {
      printf("error: digital min must be less than digital max\n");

      return EXIT_FAILURE;
    }
  }

  buf = (double *)calloc(1, sf * sizeof(double));
  if(!buf)
  {
    printf("Malloc error line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  randbuf = (int *)calloc(1, sf * sizeof(int));
  if(!randbuf)
  {
    printf("Malloc error line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  sprintf(str, "edf_generator_%iHz_%s_%f", sf, waveforms_str[waveform], signalfreq);

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
    printf("error: edfopen_file_writeonly() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_startdatetime(hdl, 2000, 1, 1, 0, 0, 0))
  {
    printf("error: edf_set_startdatetime() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_samplefrequency(hdl, 0, sf))
  {
    printf("error: edf_set_samplefrequency() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_digital_maximum(hdl, 0, digmax))
  {
    printf("error: edf_set_digital_maximum() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_digital_minimum(hdl, 0, digmin))
  {
    printf("error: edf_set_digital_minimum() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_physical_maximum(hdl, 0, physmax))
  {
    printf("error: edf_set_physical_maximum() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }


  if(edf_set_physical_minimum(hdl, 0, physmin))
  {
    printf("error: edf_set_physical_minimum() line %i\n", __LINE__);

    return EXIT_FAILURE;
  }

  if(edf_set_physical_dimension(hdl, 0, physdim))
  {
    printf("error: edf_set_physical_dimension() line %i\n", __LINE__);

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
    printf("error: edf_set_label() line %i\n", __LINE__);

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
      printf("error: open /dev/urandom line %i\n", __LINE__);

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

        buf[i] = sin(sine_1) * peakamp;
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
              }
              else if((ftmp * signalfreq) < (dutycycle / 100.0))
                {
                  buf[i] = peakamp * (400.0 / dutycycle) * ((dutycycle / 100.0) - (ftmp * signalfreq)) - peakamp;
                }
                else
                {
                  buf[i] = -peakamp;
                }
            }
          }
          else if((waveform == WAVE_WHITE_NOISE) || (waveform == WAVE_PINK_NOISE))
            {
              err = read(fd, randbuf, sf * sizeof(int));
              if(err != (sf * 4))
              {
                printf("error: read() line %i\n", __LINE__);

                return EXIT_FAILURE;
              }

              if(waveform == WAVE_WHITE_NOISE)
              {
                for(i=0; i<sf; i++)
                {
                  buf[i] = (randbuf[i] % ((int)(peakamp * 100.0))) / 100.0;
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
                    b6 = white_noise * 0.115926;
                  }
                }
            }

    if(edfwrite_physical_samples(hdl, buf))
    {
      printf("error: edfwrite_physical_samples() line %i\n", __LINE__);

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



void remove_trailing_zeros(char *str)
{
  int i, j,
      len,
      numberfound,
      dotfound,
      decimalzerofound,
      trailingzerofound=1;

  while(trailingzerofound)
  {
    numberfound = 0;
    dotfound = 0;
    decimalzerofound = 0;
    trailingzerofound = 0;

    len = strlen(str);

    for(i=0; i<len; i++)
    {
      if((str[i] < '0') || (str[i] > '9'))
      {
        if(decimalzerofound)
        {
          if(str[i-decimalzerofound-1] == '.')
          {
            decimalzerofound++;
          }

          for(j=i; j<(len+1); j++)
          {
            str[j-decimalzerofound] = str[j];
          }

          trailingzerofound = 1;

          break;
        }

        if(str[i] != '.')
        {
          numberfound = 0;
          dotfound = 0;
          decimalzerofound = 0;
        }
      }
      else
      {
        numberfound = 1;

        if(str[i] > '0')
        {
          decimalzerofound = 0;
        }
      }

      if((str[i] == '.') && numberfound)
      {
        dotfound = 1;
      }

      if((str[i] == '0') && dotfound)
      {
        decimalzerofound++;
      }
    }
  }

  if(decimalzerofound)
  {
    if(str[i-decimalzerofound-1] == '.')
    {
      decimalzerofound++;
    }

    for(j=i; j<(len+1); j++)
    {
      str[j-decimalzerofound] = str[j];
    }
  }

  if(len > 1)
  {
    if(!((str[len - 2] < '0') || (str[i] > '9')))
    {
       if(str[len - 1] == '.')
       {
         str[len - 1] = 0;
       }
    }
  }
}



















