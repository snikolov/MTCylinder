/*
 *  node.h
 *  
 *
 *  Created by Maximilian Ebbinghaus on 08.07.09.
 *  Copyright 2009 Universität des Saarlandes. All rights reserved.
 *
 */

#ifndef node_h_included
#define node_h_included


#include "constants.h"
#include "Point.h"
#include "Grid3D.h"
#include "Axon.h"

class node
{
 public:
  Point point;
  node* up; // Next node in the sequence of nodes along the current filament
  node* down; // Previous node in the sequence of nodes along the current filament
  node** links;
  bool linked_to_wall;
  Grid3D* link_grid; // 3D link-grid that the node is in.
  Grid3D* collide_grid; // 3D collision grid that the node is in.
  Axon* axon; // The axon that the node is in.
  int num_links;
  int filament_id;
  node();
  node(Point point0, int filament_id0);
  set<node*> neighbors(set<node*> R);
  void fluctuate(double (*h)(vector<Point>));
  bool move(Point p);
  bool move_within_grid(Point p, Grid3D *g);
  void print_links();
  void add_link(node* n);
  void remove_link(node* n);
  bool contains_link(node* link);
};

#endif
