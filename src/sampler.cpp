#include <cstdio>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <string>
#include <vector>

#include <stdlib.h>

// Numerical recipes
#include "nr.h"

//Global variables/Constants
#include "constants.h"

using namespace std;

double* NR::sample_metropolis(double (*f)(double,double), double* old_sample)
{
  //Generates a 2D sample of the density function f
  double x0,x1,y0,y1,rand,p_acc;
  x0 = old_sample[0];
  x1 = old_sample[1];
 
  double* neighbor;
  neighbor = (double*)malloc(sizeof*neighbor * 2);
  
  while(true)
    {
      if (DEBUG) cout << "\tsampling ...\n";
      // Pick a point in the neighborhood according to a Gaussian distribution
      neighbor = NR::sample_gauss2(0.1,neighbor); //TODO: automatically determine sigma
      y0 = neighbor[0] + x0;
      y1 = neighbor[1] + x1;
      // Accept the neighboring point with probability p_acc;
      p_acc = min(1.0,((*f)(y0,y1)/(*f)(x0,x1)));
      rand = NR::ran2(RAND_SEED);
      double* new_sample = old_sample;
      if (rand < p_acc)
	{
	  new_sample[0] = y0;
	  new_sample[1] = y1;
	  return new_sample;
	}
    }
  return NULL;
}

double* NR::sample_circle(double* empty_sample)
{
  double x1,x2,s,ups;
  x1 = sample_gauss(1);
  x2 = sample_gauss(1);
  s = x1*x1 + x2*x2;
  ups = sqrt(ran2(RAND_SEED));
  x1 = x1*ups/sqrt(s);
  x2 = x2*ups/sqrt(s);
  double sample[2];
  empty_sample[0] = x1;
  empty_sample[1] = x2;
  return empty_sample;
}

double NR::sample_gauss(double sigma)
{
  double x,y;
  while(true)
    {
      x = 2*NR::ran2(RAND_SEED) - 1;
      y = 2*NR::ran2(RAND_SEED) - 1;
      
      //      cout << "uniform random samples\nx: " << x << "\ny: " << y << "\n\n";

      double ups1 = x*x + y*y;
      
      //      cout << "ups1: " << ups1 << "\n";
      
      if (!(ups1 > 1 || ups1 == 0))
	{
	  double ups = -log(ups1);

	  //	  cout << "ups: " << ups << "\n";

	  double ups2 = sigma*sqrt(2*ups/ups1);

	  //	  cout << "ups2: " << ups2 << "\n";
	  
	  x = ups2*x;
	  y = ups2*y;
	  break;
	}
      else
	{
	  continue;
	}
    }
  return x; //TODO what to do with y?

}


double* NR::sample_gauss2(double sigma, double* empty_sample)
{
  double x,y;
  while(true)
    {
      x = 2*NR::ran2(RAND_SEED) - 1;
      y = 2*NR::ran2(RAND_SEED) - 1;
      
      //      cout << "uniform random samples\nx: " << x << "\ny: " << y << "\n\n";

      double ups1 = x*x + y*y;
      
      //      cout << "ups1: " << ups1 << "\n";
      
      if (!(ups1 > 1 || ups1 == 0))
	{
	  double ups = -log(ups1);

	  //	  cout << "ups: " << ups << "\n";

	  double ups2 = sigma*sqrt(2*ups/ups1);

	  //	  cout << "ups2: " << ups2 << "\n";
	  
	  x = ups2*x;
	  y = ups2*y;
	  break;
	}
      else
	{
	  continue;
	}
    }
  empty_sample[0] = x;
  empty_sample[1] = y;
  return empty_sample;

}
