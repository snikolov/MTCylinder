/*
 *  node.h
 *  
 *
 *  Created by Maximilian Ebbinghaus on 08.07.09.
 *  Copyright 2009 Universität des Saarlandes. All rights reserved.
 *
 */
#ifndef point_h_included
#define point_h_included

struct point_string
{
  char str[20];
};


class Point
{
 public:
  double x, y, z;
  Point();
  Point(double x0, double y0, double z0);
  double norm();
  void normalize();
  void scale(double s);
  double distance_to(Point other);
  point_string to_string();
  static Point cross(Point a, Point b);
  static double dot(Point a, Point b);
  static Point sum(Point a, Point b);
  static Point difference(Point a, Point b);
};

#endif
