/*
 *  node.h
 *  
 *
 *  Created by Maximilian Ebbinghaus on 08.07.09.
 *  Copyright 2009 Universität des Saarlandes. All rights reserved.
 *
 */

#ifndef axon_h_included
#define axon_h_included


#include "Filament.h"
#include "Grid3D.h"
#include "Point.h"
#include "node.h"

class Axon
{
 public:
  double r; //radius of cylinder
  double length; // length of cylinder
  Filament *filaments; //An array of filaments
  int num_filaments;
  int total_nodes;
  Grid3D link_grid; //3D grid containing a 3D array of pointers to nodes. 
                    // For deterimining links between nodes.
  Grid3D collide_grid; //3D grid containing a 3D array of pointers to nodes. 
                       // For determining collision between segments.
  void init(double radius, double length, double xstep, double ystep, double zstep);
  Axon(double radius, double length, double xstep, double ystep, double zstep);
  Axon(double radius, double length, double step);
  Axon(double radius, double length);
  Axon(double length);
  void add_filament(Filament f);
  bool in_cylinder(Point p);
  bool at_wall(Point p);
  void new_filament(); // Gives birth to new filament with some probability
  void update_filaments();
  void fluctuate_filaments();
  void grow_filaments();
  void break_links();
  bool collision(Point a, Point b, Point c, Point d, double r);
  bool check_collisions(Point a, Point b, int fil_id);
  void seek_links_for_node(node* new_node_ptr, double link_len, double link_prob);
  void seek_all_links(double link_len, double link_prob);
  bool link(node* node_ptr, node* new_node_ptr);
  vector<Point> cross_section(double z, vector<Point> points);
  vector<double> cross_angles(double z, vector<double> angles);
  void write_cross_section(vector<Point> points, char* outfile);
  void write_cross_angles(vector<double> angles, char* outfile);
  void write_scene(char* outfile);
  void write_distance_hist_all_pairs(vector<Point> points, char* outfile);
  void write_distance_hist_nn(vector<Point> points, char* outfile);
  int count_links();
};

#endif
