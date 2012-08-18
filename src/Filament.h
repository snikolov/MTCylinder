/*
 *  node.h
 *  
 *
 *  Created by Maximilian Ebbinghaus on 08.07.09.
 *  Copyright 2009 Universität des Saarlandes. All rights reserved.
 *
 */

#ifndef filaments_h_included
#define filaments_h_included



#include "node.h"
#include "Point.h"
#include "constants.h"


class Filament
{
 public:
  node* nodes;
  int num_nodes;
  int id;
  Filament();
  Filament(Point start, double axon_raduis);
  Filament(double axon_radius);
  //Determine a new vector to add to the current tip of the filament
  Point delta_tip();
  int slice(double z); // Determine the two points immediately above
                                       // and immediately below height z
  void grow(Point delta_tip);  //grows the filament
  void print();
};


#endif
