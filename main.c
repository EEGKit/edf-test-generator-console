/*
*****************************************************************************
*
* Copyright (c) 2018 Teunis van Beelen
* All rights reserved.
*
*
*****************************************************************************
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "edflib.h"



int main(int argc, char **argv)
{
  int i, j,
      hdl=-1,
      filetype=0,
      duration=1,
      sf=1,
      digmax=0,
      digmin=0;

  double *buf=NULL,
         w=1,
         sine_1=0,
         signalfreq=1,
         physmax=1200,
         physmin=-1200,
         peakamp=1000;

  char physdim[32]="uV",
       str[128]="";

  if((argc != 5) && (argc != 9) && (argc != 11))
  {
    printf("\nusage: edf_generator <filetype edf or bdf> <duration in seconds> <sample frequency> <signal frequency>\n"
           " [<physical max> <physical min> <peak amplitude> <physical dimension> [<digital max> <digital min>]]\n"
           "\nexample: edf_generator edf 10 1000 1\n"
           "\nexample: edf_generator edf 10 113.67 1 3200 -3200 100 uV\n"
           "\nexample: edf_generator bdf 10 1000 1.5 1000 -1000 300 mV 1048575 -1048576\n\n");

    return EXIT_FAILURE;
  }

  if(!strcmp(argv[1], "edf"))
  {
    filetype = 0;

    digmax = 32767;

    digmin = -32768;
  }
  else if(!strcmp(argv[1], "bdf"))
    {
      filetype = 1;

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
  if((sf < 4) || (sf > 96000))
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

  if((argc == 9) || (argc == 11))
  {
    physmax = atof(argv[5]);

    physmin = atof(argv[6]);

    peakamp = atof(argv[7]);

    strncpy(physdim, argv[8], 16);

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

  if(argc == 11)
  {
    digmax = atoi(argv[9]);

    digmin = atoi(argv[10]);

    if(filetype)
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
    else
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

  buf = (double *)malloc(sf * sizeof(double));
  if(!buf)
  {
    printf("Malloc error\n");

    return EXIT_FAILURE;
  }

  if(filetype)
  {
    hdl = edfopen_file_writeonly("edf_generator.bdf", EDFLIB_FILETYPE_BDFPLUS, 1);
  }
  else
  {
    hdl = edfopen_file_writeonly("edf_generator.edf", EDFLIB_FILETYPE_EDFPLUS, 1);
  }

  if(hdl<0)
  {
    printf("error: edfopen_file_writeonly()\n");

    return EXIT_FAILURE;
  }

  if(edf_set_samplefrequency(hdl, 0, sf))
  {
    printf("error: edf_set_samplefrequency()\n");

    return EXIT_FAILURE;
  }

  if(edf_set_digital_maximum(hdl, 0, digmax))
  {
    printf("error: edf_set_digital_maximum()\n");

    return EXIT_FAILURE;
  }

  if(edf_set_digital_minimum(hdl, 0, digmin))
  {
    printf("error: edf_set_digital_minimum()\n");

    return EXIT_FAILURE;
  }

  if(edf_set_physical_maximum(hdl, 0, physmax))
  {
    printf("error: edf_set_physical_maximum()\n");

    return EXIT_FAILURE;
  }


  if(edf_set_physical_minimum(hdl, 0, physmin))
  {
    printf("error: edf_set_physical_minimum()\n");

    return EXIT_FAILURE;
  }

  if(edf_set_physical_dimension(hdl, 0, physdim))
  {
    printf("error: edf_set_physical_dimension()\n");

    return EXIT_FAILURE;
  }

  snprintf(str, 18, "sine %.2fHz", signalfreq);

  if(edf_set_label(hdl, 0, str))
  {
    printf("error: edf_set_label()\n");

    return EXIT_FAILURE;
  }

  sine_1 = 0.0;

  w = M_PI * 2.0;

  w /= (sf / signalfreq);

  for(j=0; j<duration; j++)
  {
    for(i=0; i<sf; i++)
    {
      sine_1 += w;
      buf[i] = sin(sine_1) * peakamp;
    }

    if(edfwrite_physical_samples(hdl, buf))
    {
      printf("error: edfwrite_physical_samples()\n");

      return EXIT_FAILURE;
    }
  }

  edfclose_file(hdl);

  free(buf);

  return EXIT_SUCCESS;
}




















