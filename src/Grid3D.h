/*
 *  node.h
 *  
 *
 *  Created by Maximilian Ebbinghaus on 08.07.09.
 *  Copyright 2009 Universität des Saarlandes. All rights reserved.
 *
 */

#ifndef grid_h_included
#define grid_h_included



#include "Point.h"
#include "node.h"

class node;

struct triple
{
  int i,j,k;
};


class Grid3D
{
  
 public:
  double minx, maxx, miny, maxy, minz, maxz, xstep, ystep, zstep;
  int isize, jsize, ksize;
  //  node**** cells;
  list<node*>*** cells;
  Grid3D();
  void init(double minx0, double maxx0, double miny0, double maxy0,
	    double minz0, double maxz0, double xstep0, double ystep0, double zstep0);
  Grid3D(double minx0, double maxx0, double miny0, double maxy0, double minz0, 
	 double maxz0, double xstep0, double ystep0, double zstep0);
  Grid3D(double minx0, double maxx0, double miny0, double maxy0, double minz0, 
	 double maxz0, double step0);
  //Converts a point to its corresponding indices in a 3D grid.
  triple point_to_indices(Point p);
  void add_node(node* new_node_ptr);
  void move_node(node* n, triple ijk);
  bool in_range(triple ijk);
};

#endif
