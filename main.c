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
#include <locale.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>

#include "edflib.h"



int main(int argc, char **argv)
{
  int i, j,
      hdl=-1,
      filetype=0,
      duration=1,
      sf=1;

  double *buf=NULL,
         w=1,
         sine_1=0,
         signalfreq=1,
         physmax=1200,
         physmin=-1200,
         peakamp=1000;

  setlocale(LC_ALL, "C");

  if((argc != 5) && (argc != 8))
  {
    printf("\nusage: edf_generator <filetype edf or bdf> <duration in seconds> <sample frequency> <signal frequency>\n"
           "[<physical max> <physical min> <peak amplitude>]\n"
           "\nexample: edf_generator bdf 30 1000 10\n\n");

    return EXIT_FAILURE;
  }

  if(!strcmp(argv[1], "edf"))
  {
    filetype = 0;
  }
  else if(!strcmp(argv[1], "bdf"))
    {
      filetype = 1;
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

  if(argc == 8)
  {
    physmax = atof(argv[5]);

    physmin = atof(argv[6]);

    peakamp = atof(argv[7]);

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

  if(filetype)
  {
    if(edf_set_digital_maximum(hdl, 0, 8388607))
    {
      printf("error: edf_set_digital_maximum()\n");

      return EXIT_FAILURE;
    }

    if(edf_set_digital_minimum(hdl, 0, -8388608))
    {
      printf("error: edf_set_digital_minimum()\n");

      return EXIT_FAILURE;
    }
  }
  else
  {
    if(edf_set_digital_maximum(hdl, 0, 32767))
    {
      printf("error: edf_set_digital_maximum()\n");

      return EXIT_FAILURE;
    }

    if(edf_set_digital_minimum(hdl, 0, -32768))
    {
      printf("error: edf_set_digital_minimum()\n");

      return EXIT_FAILURE;
    }
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

  if(edf_set_physical_dimension(hdl, 0, "uV"))
  {
    printf("error: edf_set_physical_dimension()\n");

    return EXIT_FAILURE;
  }

  if(edf_set_label(hdl, 0, "sine"))
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




















