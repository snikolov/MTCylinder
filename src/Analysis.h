#ifndef analysis_h_included
#define analysis_h_included

#include "Point.h"

class Analysis
{
 public:
  static vector<Point> get_points_from_file(char* point_file, vector<Point> points);
  static double get_radius_from_file(char* radius_file);
  static vector<double> get_vals_from_file(char* data_file, vector<double> vals);
  static void write_angles_hist(char* angle_file, char* outfile, int num_bins);
  static void write_distance_hist_all_pairs(char* point_file, char* radius_file, char* outfile, int num_bins);
  static void write_distance_hist_nn(char* point_file, char* radius_file, char* outfile, double range, int num_bins);
  static void write_energy_hist(char* infile, char* outfile, int num_bins);
  static vector<int> bin(vector<double> data, int num_bins);
  static vector<int> bin(vector<double> data, int num_bins, int max, int min);
};

#endif
