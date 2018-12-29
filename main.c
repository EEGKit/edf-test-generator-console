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

  double *buf,
         w=1,
         sine_1=0,
         signalfreq=1;

  setlocale(LC_ALL, "C");

  if(argc != 5)
  {
    printf("\nusage: edf_generator <filetype EDF or BDF> <duration in seconds> <sample frequency> <signal frequency>\n"
           "\nexample: edf_generator BDF 30 1000 10\n");

    return EXIT_FAILURE;
  }

  if(!strcmp(argv[1], "EDF"))
  {
    filetype = 0;
  }
  else if(!strcmp(argv[1], "BDF"))
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

  if(edf_set_physical_maximum(hdl, 0, 1200.0))
  {
    printf("error: edf_set_physical_maximum()\n");

    return EXIT_FAILURE;
  }


  if(edf_set_physical_minimum(hdl, 0, -1200.0))
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
      buf[i] = sin(sine_1) * 1000.0;
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




















