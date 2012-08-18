
// C++ libs
#include <cstdio>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <string>

// STL libs
#include <vector>
#include <list>
#include <set>
#include <deque>

//C libs
#include <stdlib.h>

// Numerical recipes
#include "nr.h"

//Constants/Global variables
#include "constants.h"

//Class declarations
#include "node.h"
#include "Grid3D.h"
#include "Point.h"
#include "Filament.h"
#include "Axon.h"
#include "Analysis.h"

using namespace std;

#define PI 3.14159265

// Streams for writing and reading files
ofstream output;
ifstream input;

int DEBUG = 0;
 
bool COLLISIONS = false;   // Collision checking (true = on, false = off).
bool FLUCTUATIONS = false;
bool FORM_LINKS = false;   
bool BREAK_LINKS = false; // If true, links dissociate with LINK_RM_PROB
bool BUNDLE_FLUCT = false; // true: bundles of filaments fluctuate together. 
                          // false: only unliked filaments fluctuate

int RAND_SEED = 235769;
const double LINK_INTERACTION_LEN = .1; // Maximum distance at which two microtubules can link
const double LINK_LEN = 0.025; // The length of the linker protein. The spacing between the
                               // walls (not centers!) of the filaments once they link.
const double LINK_FORM_PROB = .9; // Probability of two nodes linking if they are close enough
const double LINK_RM_PROB = .5; // Probability of dissociation of an existing link.

const double AXON_RADIUS = 200; // Axon radius
const int MAX_LINKS = 13; // Maximum number of linker proteins attached to node.

const int NUM_STEPS = 2000;// Number of steps in a single simulation.
const int MAX_NODES = NUM_STEPS; // Maximum number of nodes in a microtubule filament.
const int NUM_REPS = 1; // Number of times to run simulation to run.
const int MAX_FILAMENTS = NUM_STEPS;
const double FILAMENT_RADIUS = .0125; // 12.5 nm
const double FILAMENT_BIRTH_PROB = 1;
const double PERSISTENCE_LEN = 30; // microns
const double MEAN_LEN_GROWTH = .1; // microns
const double STD_LEN_GROWTH = 0;//0.2*sqrt(MEAN_LEN_GROWTH); // Standard dev. of growth length in microns.
const double STD_THETA_GROWTH = (PI/13.5)*MEAN_LEN_GROWTH; // Std dev. of deviation angle

const double SIGMA_PROPOSED = .0006125; // St. dev. of distribution of proposed fluctuations. 

// Global method for generating unique IDs for filaments.
int filament_id = 0;
int next_id()
{
  int next = filament_id;
  filament_id++;
  return next;
}

//-------------------------EXTERNAL METHODS------------------

Point altitude_foot(Point p, Point q, Point r)
{
  // For a triangle defined by points p, q and r,
  // determine the point of intersection between the
  // segment from p to r and the segment perpendicular
  // to it passing through q.

  // Compute r - p
  double v1,v2,p1,p2,p3,q1,q2,q3,r1,r2,s;
  Point pr = Point::difference(r,p);
  Point rq = Point::difference(q,r);
  Point pr_x_rq = Point::cross(pr,rq);
  Point v = Point::cross(pr,pr_x_rq);
  s = (q.y - p.y + (v.y/v.x)*(p.x - q.x))/(r.y - p.y + (v.y/v.x)*(p.x - r.x));
  Point intersection = Point((1-s)*p.x + s*r.x,
			     (1-s)*p.y + s*r.y,
			     (1-s)*p.z + s*r.z);
  return intersection;
}

Point unrotate_sample(Point sample, Point q1, Point q2, Point q3)
{
  // Transforms a sample 'sample' in span{q1,q2} to a sample 'sample_xy' in the xy plane.  
  // The distribution the sample is taken from is assumed to be invariant 
  // to rotation about q3.

  // In other words, let Q be an orthogonal matrix with columns q1,q2,q3.  Then 
  // sample = Q*sample_xy, and sample_xy = Q'*sample
 
  Point sample_xy = Point(Point::dot(q1,sample),
			  Point::dot(q2, sample),
			  Point::dot(q3, sample));
  return sample_xy;
}

Point rotate_sample(Point sample, Point q1, Point q2, Point q3)
{
  // Transforms samples in the xy plane to
  // the plane defined by the orthonormal basis {q1,q2,q3}, such that
  // the unit z direction is mapped to q3. The
  // distribution is assumed to be invariant to rotation
  // about the z direction.

  // Normalize, just in case
  q3.normalize();
  q2.normalize();
  q1.normalize();
  
  // Transform sample
  q1.scale(sample.x);
  q2.scale(sample.y);
  q3.scale(sample.z);
  
  Point result = Point::sum(q1, q2);
  result = Point::sum(result, q3);
  return result;  
}


double density_gauss(double x, double y)
{
  double sigma = .7;
  return exp((-x*x - y*y)/(2*sigma*sigma));
}

double curve_hamiltonian_5(Point p1, Point p2, Point p3, Point p4, Point p5)
{
  // Computes the energy of a configuration of a chain of 5 consecutive points
  // p1--p2--p3--p4--p5
  
  // Compute discrete tangent vectors
  Point t1 = Point::difference(p2,p1);
  Point t2 = Point::difference(p3,p2);
  Point t3 = Point::difference(p4,p3);
  Point t4 = Point::difference(p5,p4);
  
  // Compute segment lenghts;
  double b1, b2, b3, b4;
  b1 = t1.norm();
  b2 = t2.norm();
  b3 = t3.norm();
  b4 = t4.norm();

  // Normalize tangent vectors
  t1.normalize();
  t2.normalize();
  t3.normalize();
  t4.normalize();

  // Compute Hamiltonian
  double h = (1.0/b1)*(1.0 - Point::dot(t1,t2)) + 
    (1.0/b2)*(1.0 - Point::dot(t2,t3)) +
    (1.0/b3)*(1.0 - Point::dot(t3,t4));

  h = h*PERSISTENCE_LEN;
  return h;
}

double curve_hamiltonian(vector<Point> points)
{
  if (points.size() <= 2)
    {
      // cout << "curve_hamiltonian received a vector of size " << points.size() << "!\n";
      return 0;
    }
  double h = 0;

  Point p = points[0];
  Point q = points[1];
  Point t1 = Point::difference(q,p);
  Point t2;
  double b1 = t1.norm();
  double b2;
  t1.normalize();
  for (int i = 2; i < points.size(); i++)
    {
      q = points[i];
      p = points[i-1];
      t2 = Point::difference(q,p);
      b2 = t2.norm();
      t2.normalize();
      h += (1 - Point::dot(t1,t2))/b1;
      b1 = b2;
      t1 = t2;
    }
  h = h*PERSISTENCE_LEN;
  return h;
}

//---------------------------------NODE------------------------------------

//Node structure for representing linked lists of pointers.
node::node()
{
  links = (node**)malloc(sizeof*links * MAX_LINKS);
  up = NULL;
  down = NULL;
  axon = NULL;
  link_grid = NULL;
  collide_grid = NULL;
  linked_to_wall = false;
  num_links = 0;
}

node::node(Point point0, int filament_id0)
{
  links = (node**)malloc(sizeof*links * MAX_LINKS);
  point = point0;
  up = NULL;
  down = NULL;
  axon = NULL;
  link_grid = NULL;
  collide_grid = NULL;
  filament_id = filament_id0;
  num_links = 0;
  linked_to_wall = false;
}

void node::print_links()
{
  cout << "Node " << this << " is linked to: \n";

  for (int i = 0; i < num_links; i++)
    {
      cout << links[i] << "\n";
    }

  cout << "\n";
}

void node::add_link(node* link)
{
  if (num_links <= MAX_LINKS)
    {
      links[num_links] = link;
    }
  num_links++;
}

void node::remove_link(node* link)
{
  for (int i = 0; i < num_links; i++)
    {
      if (links[i] == link)
	{
	  for (int j = i; j < num_links - 1; j++)
	    {
	      links[j] = links[j+1];
	    }
	  links[num_links] = NULL;
	  num_links--;
	  break;
	}
    }
}

bool node::contains_link(node* link)
{
  for (int i = 0; i < num_links; i++)
    {
      if (links[i] == link)
	return true;
    }
  return false;
}


set<node*> node::neighbors(set<node*> R)
{
  // Get the list of nodes linked by 0 or more edges (linker proteins) to the
  // current node. Returns a set of nodes including the current node. 
  // Takes an empty set as input, then populates it.

  deque<node*> Q;
   
  // Do a DFS
  Q.push_back(this);
  R.insert(this);
  
  node* n;
  while(Q.size() != 0)
    {
      n = Q.front();
      //      cout << "Popped node " << n << ". Found " << n->num_links << " links. \n"; 
      Q.pop_front();
      for (int i = 0; i < n->num_links; i++)
	{
	  if (R.find(n->links[i]) == R.end())
	    {
	      Q.push_back(n->links[i]);
	      R.insert(n->links[i]);
	    }
	}
    }
  return R;
}

int num_rejected = 0;
int num_total = 0;
int num_good = 0;
int num_collided = 0;
int num_out = 0;

int num_rejected1 = 0;
int num_total1 = 0;
int num_rejected2 = 0;
int num_total2 = 0;
int num_rejected3 = 0;
int num_total3 = 0;
int num_rejected4 = 0;
int num_total4 = 0;
int num_rejected5 = 0;
int num_total5 = 0;


void node::fluctuate(double (*hamiltonian)(vector<Point>) = curve_hamiltonian)
{
  int DEBUG0 = 0;
  int bundle_size = 0;

  // Generates a fluctuation.  
  // Takes: 
  // - a pointer to a density function f
  
  string fluct_energies = "anim/fluct_energies.dat";
  string fluct_data = "anim/fluct_data.dat";

  Point proposed;
  Point q1,q2,q3;
  if (up != NULL && down != NULL)
    {
      num_total++;
      // Determine 3 central points in the chain: p--q--r, where q is the fluctuating point
      Point r = up->point;
      Point q = this->point;
      Point p = down->point;
      Point intersection = altitude_foot(p,q,r);
      Point pr = Point::difference(r,p);
      Point rq = Point::difference(q,r);
      // Construct unit vectors q1, q2 such that {q1,q2,q3} is an orthonormal basis
      // and q3 || pr.
      q1 = Point::cross(pr,rq);
      q2 = Point::cross(pr,q1);
      q3 = pr;
    }
  else if (up == NULL && down != NULL && down->down != NULL)
    {
      num_total++;
      // Fluctuating tip.  In a chain p--q--r, the node at r fluctuates
      Point r = this->point;
      Point q = this->down->point;
      Point p = this->down->down->point;

      // Generate a fluctuation in the plane with normal vector q-r
      Point pq = Point::difference(q,p);
      Point qr = Point::difference(r,q);
      q1 = Point::cross(pq,qr);
      q2 = Point::cross(q1,qr);
      q3 = qr;
    }
  else
    {
      if (DEBUG0) cout << "Could not fluctuate " << this << " at point " << point.to_string().str 
		       << "." <<  "up = " << up << " down = " << down << "\n";
      return;
    }

  q1.normalize();
  q2.normalize();
  q3.normalize();

  // Propose a fluctuation
  if (DEBUG0) cout << "Proposing fluctuation\n" << flush;
  
  double* proposed_xy;
  proposed_xy = (double*)malloc(sizeof*proposed_xy * 2);
  // Generate gaussian in the xy plane w/ sigma = SIGMA_PROPOSED
  proposed_xy = NR::sample_gauss2(SIGMA_PROPOSED,proposed_xy); 
  proposed = rotate_sample(Point(proposed_xy[0], proposed_xy[1], 0), q1, q2, q3);
   
  // Find the size of the connected component of which the curent node
  // is a member.  That is, find the current node's neighbors, and their
  // neighbors, (and so on) and count how many there are (including the
  // current node.  These will all try to fluctuate together, as a bundle
  // of filaments.
  
  set<node*> bundle = set<node*>();
  bundle = this->neighbors(bundle);
  bundle_size = bundle.size();
  
  //	  if (bundle_size > 4) cout << "Bundle size is " << bundle_size << "\n";
  
   
  Point new_point;  
  
  bool collision = false;
  bool out_of_bounds = false;
  // Check if fluctuation is out of bounds or causes collision
  // Note that the move(new_loc) method also check for this, but we would like
  // to check beforehand to avoid calculating energies for moves that would
  // be invalid anyway.
  
  set<node*>::iterator it = bundle.begin();
  
  while (it != bundle.end() && !collision && !out_of_bounds)
    {
      new_point = Point::sum(proposed,(*it)->point);
      if ((*it)->up != NULL)
	{
	  // Check if the segment [(*it)->up->point; new_point] collides
	  if (COLLISIONS) collision = axon->check_collisions((*it)->up->point,new_point,(*it)->filament_id);
	}
      if (!collision && (*it)->down != NULL)
	{
	  // Check if the segment [(*it)->down->point; new_point] collides
	  if (COLLISIONS) collision = axon->check_collisions((*it)->down->point,new_point,(*it)->filament_id);
	}
      if (!collision && !axon->in_cylinder(new_point))
	out_of_bounds = true;
      
      it++;
    }
    
  /*
    if (out_of_bounds)
    {
    num_out++;
    
    if (NR::ran2(RAND_SEED) < .001)
    {
    cout << "Out of bounds.\n";
    cout << "num_out/num_total = " << num_out << "/" << num_total
    << " = " << (double)num_out/num_total << "\n";
    cout << "num_good/num_total = " << num_good << "/" << num_total << " = "
    << (double)num_good/num_total << "\n";
    
    int x;
    //	      cin >> x;
    }
    }
    if (collision)
    {
    num_collided++;
    
    if (NR::ran2(RAND_SEED) < .001)
    {
    cout << "Collision.\n";
    
    cout << "num_collided/num_total = " << num_collided << "/" << num_total
    << " = " << (double)num_collided/num_total << "\n";
    cout << "num_good/num_total = " << num_good << "/" << num_total << " = "
    << (double)num_good/num_total << "\n";
    
    int x;
    //		cin >> x;
    }
    }
  */
  
  
  if (!collision)
    {
      if (!out_of_bounds)
	{
	  
	  num_good++;
	  
	  if (bundle_size == 1)
	    {
	      num_total1++;
	    }
	  else if (bundle_size == 2)
	    {
	      num_total2++;
	    }
	  else if (bundle_size == 3)
	    {
	      num_total3++;
	    }
	  else if (bundle_size == 4)
	    {
	      num_total4++;
	    }
	  else if (bundle_size == 5)
	    {
	      num_total5++;
	    }
	  
	  
	  // Calculate energy of new and old configurations.
	  vector<Point> chain;
	  
	  double energy_old = 0;
	  double energy_new = 0;
	  double delta_old = 0;
	  double delta_new = 0;
	  

	  it = bundle.begin();
	  
	  while (it != bundle.end())
	    {
	      // TEST: if the chain is p1--p2--p3--p4--p5 want angle between p2p3 and p4p3	  
	      // for the new and old configurations
	      Point p4p3_old = Point(0,0,999);
	      Point p2p3_old = Point(0,0,999);
	      Point p4p3_new = Point(0,0,999);
	      Point p2p3_new = Point(0,0,999);
	  

	      new_point = Point::sum(proposed,(*it)->point);
	      chain = vector<Point>();
	      // Create old chain to input into hamiltonian;
	      if ((*it)->down != NULL)
		{
		  p2p3_old = Point::difference((*it)->down->point,(*it)->point);

		  if ((*it)->down->down != NULL)
		    {
		      chain.push_back((*it)->down->down->point);
		      chain.push_back((*it)->down->point);
		    }
		  else
		    {
		      chain.push_back((*it)->down->point);
		    }
		}
	      chain.push_back((*it)->point);
	      
	      if ((*it)->up != NULL)
		{
		  p4p3_old = Point::difference((*it)->up->point,(*it)->point);

		  chain.push_back((*it)->up->point);
		  if ((*it)->up->up != NULL)
		    {
		      chain.push_back((*it)->up->up->point);
		    }
		}

	      //cout << "Chain size is " << chain.size() << "\n";
	      
	      delta_old = (*hamiltonian)(chain);
	      energy_old += delta_old;
	      
	      /* output.open((char*)fluct_data.data(),ios::app);
	      // Print out angle of fluctuation at fluctuating node:
	      if (p4p3_old.z != 999 && p2p3_old.z != 999)
		{
		  p2p3_old.normalize();
		  p4p3_old.normalize();
		  output << 180*acos(Point::dot(p4p3_old,p2p3_old))/PI << "\t\t\t"
			 << delta_old << "\t\t\t";
		}
	      */

	      chain = vector<Point>();
	      
	      // Create new chain to input into hamiltonian;
	      if ((*it)->down != NULL)
		{
		  p2p3_new = Point::difference((*it)->down->point,new_point);

		  if ((*it)->down->down != NULL)
		    {
		      chain.push_back((*it)->down->down->point);
		      chain.push_back((*it)->down->point);
		    }
		  else
		    {
		      chain.push_back((*it)->down->point);
		    }
		}
	      chain.push_back(new_point);
	      
	      if ((*it)->up != NULL)
		{
		  p4p3_new = Point::difference((*it)->up->point,new_point);
		  
		  chain.push_back((*it)->up->point);
		  if ((*it)->up->up != NULL)
		    {
		      chain.push_back((*it)->up->up->point);
		    }
		}
	      
	      delta_new = (*hamiltonian)(chain);
	      energy_new += delta_new;
	      
	      /*
	      // Print out angle of fluctuation at fluctuating node:
	      if (p4p3_new.z != 999 && p2p3_new.z != 999)
		{
		  p2p3_new.normalize();
		  p4p3_new.normalize();
		  output << 180*acos(Point::dot(p4p3_new,p2p3_new))/PI << "\t\t\t"
			 << delta_new << "\n";
		}
	      output.close();
	      */

	      it++;
	      
	      /*
		if (bundle_size > 1)
		{
		cout << "bundle_size = " << bundle_size << "\n"
		<< "filament delta_energy = " << delta_new - delta_old << "\n";
		}
	      */
	      
	    }
	  
	  double rand = NR::ran2(RAND_SEED);
	  
	  if (rand < min(1.0, exp(-(energy_new - energy_old))))
	    {
	      // Write energies to a file, with some small probability,
	      // otherwise there will be unnecessarily many entries
	      if (NR::ran2(RAND_SEED) < 0.025)
		{
		  output.open((char*)fluct_energies.data(),ios::app);
		  output << energy_new << "\n";
		  output.close();
		}

	      // Try to move to new_q
	      it = bundle.begin();
	      while (it != bundle.end())
		{
		  new_point = Point::sum(proposed, (*it)->point);
		  (*it)->move(new_point);
		  
		  it++;
		}
	      
	      //if (bundle_size > 1) cout << "Move accepted\n";
	      
	      /*		  
				 cout << "Move accepted.\n";
				 
				 if (bundle_size == 2)
				 {
				 cout << "size = 2. Old energy = " << energy_old << " New energy = " << energy_new
				 << "\n Prob of acceptance = " << min(1.0,exp(-(energy_new - energy_old))) << "\n";
				 }
				 else if (bundle_size == 3)
				 {
				 cout << "size = 3. Old energy = " << energy_old << " New energy = " << energy_new
				 << "\n Prob of acceptance = " << min(1.0,exp(-(energy_new - energy_old))) << "\n";
				 }
				 else if (bundle_size == 4)
				 {
				 cout << "size = 4. Old energy = " << energy_old << " New energy = " << energy_new
				 << "\n Prob of acceptance = " << min(1.0,exp(-(energy_new - energy_old))) << "\n";
				 }
				 else if (bundle_size == 5)
				 {
				 cout << "size = 5. Old energy = " << energy_old << " New energy = " << energy_new
				 << "\n Prob of acceptance = " << min(1.0,exp(-(energy_new - energy_old))) << "\n";
				 }
				 
				 cout << "acceptance1 = " << num_total1 - num_rejected1 << "/" 
				 << num_total1 << " = " << 1.0 - (double)num_rejected1/num_total1 << "\n"
				 << "acceptance2 = " << num_total2 - num_rejected2 << "/" 
				 << num_total2 << " = "<< 1.0 - (double)num_rejected2/num_total2 << "\n"
				 << "acceptance3 = " << num_total3 - num_rejected3 << "/" 
				 << num_total3 << " = "<< 1.0 - (double)num_rejected3/num_total3 << "\n"
				 << "acceptance4 = " << num_total4 - num_rejected4 << "/" 
				 << num_total4 << " = "<< 1.0 - (double)num_rejected4/num_total4 << "\n"
				 << "acceptance5 = " << num_total5 - num_rejected5 << "/" 
				 << num_total5 << " = "<< 1.0 - (double)num_rejected5/num_total5 << "\n\n";
				 
				 if (bundle_size > 2)
				 {
				 int x;
				 cin >> x;
				 }
	      */
	    }    
	  else
	    {
	      num_rejected++;
	      //if (bundle_size > 1) cout << "Move rejected\n";
	      
	      // Write energies to a file, with some small probability,
	      // otherwise there will be many millions of entries
	      if (NR::ran2(RAND_SEED) < 0.025)
		{
		  output.open((char*)fluct_energies.data(),ios::app);
		  output << energy_old << "\n";
		  output.close();
		}

	      
	      if (DEBUG0) cout << "Move rejected.\n" << num_rejected << "/" 
			       << num_total << " = " << double(num_rejected)/num_total << "\n";	            
	      
	      //cout << "num_total = " << num_total << "\n";
	      //cout << "num_rejected = " << num_rejected << "\n";
	      //cout << "Move rejected. Acceptance rate = " << 1.0 - (double)num_rejected/num_total << "\n";
	      
	      /*
		cout << "Move rejected.\n";
		
		if (bundle_size == 1)
		num_rejected1++;
		else if (bundle_size == 2)
		{
		num_rejected2++;
		cout << "size = 2. Old energy = " << energy_old << " New energy = " << energy_new
		<< "\n Prob of acceptance = " << min(1.0,exp(-(energy_new - energy_old))) << "\n";
		}
		else if (bundle_size == 3)
		{
		num_rejected3++;
		cout << "size = 3. Old energy = " << energy_old << " New energy = " << energy_new
		<< "\n Prob of acceptance = " << min(1.0,exp(-(energy_new - energy_old))) << "\n";
		}
		else if (bundle_size == 4)
		{
		num_rejected4++;
		cout << "size = 4. Old energy = " << energy_old << " New energy = " << energy_new
		<< "\n Prob of acceptance = " << min(1.0,exp(-(energy_new - energy_old))) << "\n";
		}
		else if (bundle_size == 5)
		{
		num_rejected5++;
		cout << "size = 5. Old energy = " << energy_old << " New energy = " << energy_new
		<< "\n Prob of acceptance = " << min(1.0,exp(-(energy_new - energy_old))) << "\n";
		}
		
		cout << "acceptance1 = " << num_total1 - num_rejected1 << "/" 
		<< num_total1 << " = " << 1.0 - (double)num_rejected1/num_total1 << "\n"
		<< "acceptance2 = " << num_total2 - num_rejected2 << "/" 
		<< num_total2 << " = "<< 1.0 - (double)num_rejected2/num_total2 << "\n"
		<< "acceptance3 = " << num_total3 - num_rejected3 << "/" 
		<< num_total3 << " = "<< 1.0 - (double)num_rejected3/num_total3 << "\n"
		<< "acceptance4 = " << num_total4 - num_rejected4 << "/" 
		<< num_total4 << " = "<< 1.0 - (double)num_rejected4/num_total4 << "\n"
		<< "acceptance5 = " << num_total5 - num_rejected5 << "/" 
		<< num_total5 << " = "<< 1.0 - (double)num_rejected5/num_total5 << "\n\n";
		
		if (bundle_size > 2)
		{
		int x;
		cin >> x;
		}
	      */
	      
	    }
	  
	  // if (bundle_size > 1)
	  // cout << "\n\n";
	  
	}
      else
	{
	  if (DEBUG0) cout << "Out of bounds during fluctation. \n";
	}
    }
  else
    {
      if (DEBUG0) cout << "Collision during fluctuation. \n";
    }
}


bool node::move(Point new_loc)
{
  bool m1 = false;
  bool m2 = false;
  if (FORM_LINKS) m1 = move_within_grid(new_loc, link_grid);
  if (COLLISIONS) m2 = move_within_grid(new_loc, collide_grid);
  return (m1 || !FORM_LINKS) && (m2 || !COLLISIONS);
}

bool node::move_within_grid(Point new_loc, Grid3D *grid)
{
  int DEBUG0 = 0;
  triple ijk_new, ijk_old;

  /*
  cout << "this " << this << "\n";
  cout << "this->point " << this->point.to_string().str << "\n";
  cout << "getting old indices \n" << flush;
  cout << "grid after " << grid << "\n";
  */

  ijk_old = grid->point_to_indices(this->point);

  //  cout << "getting new indices \n" << flush;

  ijk_new = grid->point_to_indices(new_loc);
  
  if (grid->in_range(grid->point_to_indices(new_loc)) && 
      axon->in_cylinder(new_loc))
    {
      this->point = new_loc;
      
      if (DEBUG0) cout << "Move in range of cylinder \n" << flush;
      // Check if grid cell changes
      
      if (!(ijk_new.i == ijk_old.i &&
	    ijk_new.j == ijk_old.j &&
	    ijk_new.k == ijk_old.k))
	{
	  
	  if (DEBUG0) cout << "Proposed move accepted. Move node from cell (" 
			  << ijk_old.i << "," << ijk_old.j << "," << ijk_old.k << ") to new cell (" 
			  << ijk_new.i << "," << ijk_new.j << "," << ijk_new.k << ")\n" << flush;
  
	  grid->move_node(this, ijk_old);	    
	}
      else
	{
	  if (DEBUG0) cout << "Node moved but stayed in grid cell (" << ijk_new.i << "," << ijk_new.j 
			  << "," << ijk_new.k << ")\n"; 
	}
    }
  else
    {
      if (DEBUG0) cout << "Proposed move from cell (" << ijk_old.i << "," << ijk_old.j 
		      << "," << ijk_old.k << ") to new cell (" << ijk_new.i << "," << ijk_new.j 
		      << "," << ijk_new.k << ") rejected -- out of cylinder bounds.\n";
      return false;
    }
  return true;
}


//----------------------POINT-------------------------------
Point::Point()
{
  x = 0;
  y = 0;
  z = 0;
}

Point::Point(double x0, double y0, double z0)
{
  x = x0;
  y = y0;
  z = z0;
}

double Point::norm()
{
  return sqrt(x*x + y*y + z*z);
}

void Point::normalize()
{
  double norm = this->norm();
  this->scale(1.0/norm);
}

void Point::scale(double s)
{
  x = x*s;
  y = y*s;
  z = z*s;
}

Point Point::sum(Point a, Point b)
{
  double x0,y0,z0;
  x0 = a.x + b.x;
  y0 = a.y + b.y;
  z0 = a.z + b.z;
  
  return Point(x0,y0,z0);
}

Point Point::difference(Point a, Point b)
{
  double x0,y0,z0;
  x0 = a.x - b.x;
  y0 = a.y - b.y;
  z0 = a.z - b.z;
  
  return Point(x0,y0,z0);
}

double Point::distance_to(Point other)
{
  double x0,y0,z0;
  x0 = other.x;
  y0 = other.y;
  z0 = other.z;
  
  return sqrt((x-x0)*(x-x0) + (z-z0)*(z-z0) + (y-y0)*(y-y0));
}

point_string Point::to_string()
{
  point_string ptstr;
  sprintf(ptstr.str,"(%4.2f,%4.2f,%4.2f)",x,y,z);
  return ptstr;
}

Point Point::cross(Point a, Point b)
{
  return Point(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y -a.y*b.x);
}

double Point::dot(Point a, Point b)
{
  return a.x*b.x + a.y*b.y + a.z*b.z;
}



//----------------------GRID3D--------------------------------

Grid3D::Grid3D()
{
  //empty constructor
}

void Grid3D::init(double minx0, double maxx0, double miny0, double maxy0,
	    double minz0, double maxz0, double xstep0, double ystep0, double zstep0)
{
  minx = minx0;
  maxx = maxx0;
  miny = miny0;
  maxy = maxy0;
  minz = minz0;
  maxz = maxz0;
  xstep = xstep0;
  ystep = ystep0;
  zstep = zstep0;
  
  int numx = (int) ceil((maxx-minx)/xstep);
  int numy = (int) ceil((maxy-miny)/ystep);
  int numz = (int) ceil((maxz-minz)/zstep);
  
  isize = numx;
  jsize = numy;
  ksize = numz;
  
  //Allocate memory for cells.
  cells = new list<node*>**[numx];
  for (int i = 0; i < numx; i++)
    {
      cells[i] = new list<node*>*[numy];
      for (int j = 0; j < numy; j++)
	{
	  cells[i][j] = new list<node*>[numz];
	}
    }
}

Grid3D::Grid3D(double minx0, double maxx0, double miny0, double maxy0, double minz0, 
	       double maxz0, double xstep0, double ystep0, double zstep0)
{
  init(minx0, maxx0, miny0, maxy0, minz0, maxz0, xstep0, ystep0, zstep0);
}

Grid3D::Grid3D(double minx0, double maxx0, double miny0, double maxy0, double minz0, 
       double maxz0, double step0)
{
  init(minx0, maxx0, miny0, maxy0, minz0, maxz0, step0, step0, step0);
}

//Converts a point to its corresponding indices in a 3D grid.
triple Grid3D::point_to_indices(Point p)
{
  triple ijk;
  ijk.i = (int) floor((p.x - minx)/xstep);
  ijk.j = (int) floor((p.y - miny)/ystep);
  ijk.k = (int) floor((p.z - minz)/zstep);
  return ijk;
}


void Grid3D::add_node(node* new_node_ptr)
{
  if (new_node_ptr != NULL)
    {
      Point new_tip = new_node_ptr->point;
      if (DEBUG) cout << (new_tip.to_string()).str << "\n";
      triple ijk = this->point_to_indices(new_tip); //Get indices of new_tip in the grid
     

      //cout << "In add_node. new_node_ptr->grid is " << new_node_ptr->grid << "\n";


      int ii = ijk.i;
      int jj = ijk.j;	
      int kk = ijk.k;
      
      if (in_range(ijk))
	{
	  this->cells[ii][jj][kk].push_back(new_node_ptr);
	}
      else
	{
	  if (DEBUG) cout << "Node cannot be added to grid -- indices " << "(" << ii << "," << jj << "," << kk 
	       << ") are out of range!  The maximum indicies of the grid are \nisize: " << this->isize - 1 << "\n"
	       << "jsize: " << this->jsize - 1 << "\n"
	       << "ksize: " << this->ksize - 1 << "\n";
	}
    }
  else
    {
      //cout << "add_node(): Pointer to new node is NULL. \n";
    }
}

void Grid3D::move_node(node* n, triple ijk_old)
{
  int i,j,k;
  i = ijk_old.i;
  j = ijk_old.j;
  k = ijk_old.k;

  cells[i][j][k].remove(n);

  add_node(n); // It is assumed that n's new position has already
               // been set, so now we just add it using add_node(node* n)
}

bool Grid3D::in_range(triple ijk)
{
  int ii = ijk.i;
  int jj = ijk.j;	
  int kk = ijk.k;

  if (ii < this->isize && ii >= 0 &&
      jj < this->jsize && jj >= 0 &&
      kk < this->ksize && kk >= 0)
    return true;
  else
    return false;
}

//-----------------------------FILAMENT---------------------------
Filament::Filament()
{
  // Do nothing.
}

Filament::Filament(Point start, double axon_raduis = AXON_RADIUS)
{
  nodes = (node*)malloc(sizeof*nodes * MAX_NODES);
  id = next_id();
  nodes[0]= node(start,id);
  num_nodes = 1;
}

Filament::Filament(double axon_radius)
{
  nodes = (node*)malloc(sizeof*nodes * MAX_NODES);
  id = next_id();
  double eps = LINK_INTERACTION_LEN;
  double* disk_sample;
  disk_sample = (double*) malloc(sizeof*disk_sample * 2);
  disk_sample = NR::sample_circle(disk_sample);
  Point start = Point((axon_radius-eps)*disk_sample[0],
		      (axon_radius-eps)*disk_sample[1],
		      0);
  nodes[0] = node(start,id);
  num_nodes = 1;
}

  //Determine a new vector to add to the current tip of the filament
Point Filament::delta_tip() 
{
  double r,theta,phi;
  r = NR::sample_gauss(STD_LEN_GROWTH) + MEAN_LEN_GROWTH;
  phi = 2*PI*NR::ran2(RAND_SEED); // Pick phi uniformly
  theta = NR::sample_gauss(STD_THETA_GROWTH);
  //cout << ranx << "\n" << rany << "\n" << ranz << "\n";
  Point prev_dir; // Direction of previous growth;
  if (num_nodes-2 >= 0)
    {
      // Filament has at least two nodes already. Can determine direction
      // of previous growth.
      prev_dir = Point::difference(nodes[num_nodes - 1].point, nodes[num_nodes - 2].point);
    }
  else
    {
      // Filament has only 1 point (the one added at initialization). Assume prev_dir
      // is (0,0,1), i.e., point does not need to be transformed.
      Point init = Point(r*sin(theta)*sin(phi),r*sin(theta)*cos(phi),r*cos(theta));
      return init;
    }
  prev_dir.normalize();
  // Generate delta_tip in regular coordinate system.  Then transform such that
  // (0,0,1) gets mapped to prev_dir.
  Point init = Point(r*sin(theta)*sin(phi),r*sin(theta)*cos(phi),r*cos(theta));
  //  cout << "init " << init.to_string().str << "\n";

  // Generate two unit basis vectors q1, q2 such that {q1, q2, prev_dir} is an orthonormal
  // basis.

  // Pick a random vector;
  double ranx, rany, ranz;
  ranx = NR::ran2(RAND_SEED);
  rany = NR::ran2(RAND_SEED);
  ranz = NR::ran2(RAND_SEED);
  Point rand = Point(ranx,rany,ranz);
  Point q1 = Point::cross(rand, prev_dir);
  q1.normalize();
  Point q2 = Point::cross(q1,prev_dir);
  q2.normalize();
  Point delta_tip = rotate_sample(init, q1, q2, prev_dir);
  
  Point init_hat = init;
  init_hat.normalize();
  //cout << "init_hat*(0,0,1) = " << Point::dot(init_hat,Point(0,0,1)) << "\n";
  Point delta_tip_hat = delta_tip;
  delta_tip_hat.normalize();
  //cout << "delta_tip_hat*prev_dir = " << Point::dot(delta_tip_hat,prev_dir) << "\n";
  //cout << "theta = " << acos(Point::dot(delta_tip_hat,prev_dir))*180.0/PI << "\n";
  return delta_tip;
}	

void Filament::grow(Point delta_tip)  //grows the filament
{
  int DEBUG0 = 0;
  
  if (num_nodes >= MAX_NODES) 
    return;

  Point tip = nodes[num_nodes-1].point;
  Point new_tip = Point::sum(tip,delta_tip);
  //cout << "new tip " << new_tip.x << new_tip.y << new_tip.z << "\n";
  //cout << "There are " << points.size() << " points in the filament \n";

  node new_node = node(new_tip,id);
  nodes[num_nodes] = new_node;
  new_node.up = NULL;
  if (num_nodes == 0)
    {
      nodes[num_nodes].down = NULL;
      if (DEBUG0) cout << "num_nodes = 0\n"
		       << "tip = " << nodes[0].point.to_string().str << "\n"; 
    }
  else
    {
      nodes[num_nodes].down = &nodes[num_nodes - 1];
      if (DEBUG0) cout << "set" << &nodes[num_nodes] 
		       << "->down to " << nodes[num_nodes].down << "\n";
      nodes[num_nodes - 1].up = &nodes[num_nodes];
    }

  //cout << &nodes[num_nodes] << " --up-> " << nodes[num_nodes].up << "\n"
  //	 << &nodes[num_nodes] << "--down-> " << nodes[num_nodes].down << "\n\n";
  num_nodes++;
}

int Filament::slice(double z)
{
  // Determine the index of the point immediately below height z (and thus, the index
  // of the one immediately above z).

  // TODO: We assume for now that the filament has at least two points
  // and that there exist two points on either side of height z.
  // It follows from these assumptions that the first node is below z
  // and the last point is above z.
  int start, end, mid;

  start = 0;
  end = num_nodes - 1;
  
  if (start != end)
    {
       while (!(start == end - 1))
	{
	  mid = (int)((start + end)/2);
	  
	  if (nodes[mid].point.z > z)
	    {
	      end = mid;
	    }
	  else
	    {
	      start = mid;
	    } 
	}
    }

  return start;
}

void Filament::print()
{
  for (int i = 0; i < num_nodes; i++)
    {
      Point p = nodes[i].point;
      if (DEBUG) cout << "(" << p.x << "," << p.y << "," << p.z << ")\n"; 
    }
}



//-----------------------------------AXON----------------------------------------
void Axon::init(double radius, double length0, double xstep, double ystep, double zstep)
{
  r = radius;
  length = length0;
  /*  link_grid = Grid3D(-radius, radius, -radius, radius, 0, length, xstep, ystep, zstep);
  collide_grid = Grid3D(-radius, radius, -radius, radius, 0, length, 
  MEAN_LEN_GROWTH/2.0, MEAN_LEN_GROWTH/2.0, MEAN_LEN_GROWTH);*/

  link_grid = Grid3D(-radius, radius, -radius, radius, 0, length, 100, 100, 100);
  collide_grid = Grid3D(-radius, radius, -radius, radius, 0, length, 
			100,100,100);
  filaments = (Filament*)malloc(sizeof*filaments * MAX_FILAMENTS);
  num_filaments = 0;
  total_nodes = 0;
}

Axon::Axon(double radius, double length, double xstep, double ystep, double zstep)
{
  init(radius, length, xstep, ystep, zstep);
}

Axon::Axon(double radius, double length, double step)
{
  init(radius, length, step, step, step);
}

Axon::Axon(double radius, double length)
{
  init(radius, length, LINK_INTERACTION_LEN, LINK_INTERACTION_LEN, LINK_INTERACTION_LEN);
}

Axon::Axon(double length)
{
  init(AXON_RADIUS, length, LINK_INTERACTION_LEN, LINK_INTERACTION_LEN, LINK_INTERACTION_LEN);
}

void Axon::add_filament(Filament f)
{
  if (num_filaments < MAX_FILAMENTS)
    {
      filaments[num_filaments] = f;
      num_filaments++;
    }
}

bool Axon::in_cylinder(Point p)
{
  double eps = LINK_INTERACTION_LEN;
  double y = p.y;
  double x = p.x;
  double z = p.z;

  if (y*y + x*x > (this->r-eps)*(this->r-eps) &&
      z >= 0 && z <= length)
    {
      return false;
    }
  
  return true;
}

bool Axon::at_wall(Point p)
{
  // Returns true if p within eps from wall,
  // and false otherwise.
  double eps = LINK_INTERACTION_LEN;
  double y = p.y;
  double x = p.x;
  
  if (y*y + x*x > (r-eps)*(r-eps) &&
      y*y + x*x < r*r)
    {
      return true;
    }
  
  return false;
}

void Axon::grow_filaments()
{
  Filament* f;
  bool collision = false;
  int index, num_nodes;
  for (int i = 0; i < num_filaments; i++)
    {
      index = (int)floor(num_filaments*NR::ran2(RAND_SEED)); 
      if (DEBUG) cout << "filament " << index << "\n";
      f = &filaments[index];
      //f->print();
      
      Point tip = f->nodes[f->num_nodes - 1].point;
      Point delta_tip = f->delta_tip();
      Point new_tip = Point::sum(tip,delta_tip);
      
      if (COLLISIONS) collision = check_collisions(tip, new_tip, f->id);
    
      if (DEBUG)
	{
	  cout << "attempt";
	  if (collision)
	    {
	      cout << " -> collision\n";
	    }
	  else
	    cout << "\n";
	}

      if (in_cylinder(new_tip) &&
	  link_grid.in_range(link_grid.point_to_indices(new_tip)) &&
	  collide_grid.in_range(collide_grid.point_to_indices(new_tip)) &&
	  !collision)
	{
	  f->grow(delta_tip);
	  num_nodes = f->num_nodes;
	  if (DEBUG)  cout << "Growing by " << (delta_tip.to_string()).str << "\n"
			   << "New tip is " << (new_tip.to_string()).str << "\n\n";
	  
	  node* new_node_ptr = &(f->nodes[num_nodes - 1]); //Get address of the stored new_node.
	  //cout << "...done. new_node_ptr has address " << new_node_ptr <<"\n"
	  //     << "Adding new_node_ptr ...\n";
	  new_node_ptr->axon = this;
	  
	  new_node_ptr->link_grid = &link_grid; // Let the node know the grids it is in.
	  new_node_ptr->collide_grid = &collide_grid;

	  link_grid.add_node(new_node_ptr); // Add new node pointer to the grids.
	  collide_grid.add_node(new_node_ptr);

	  total_nodes++;
	 
	  if (FORM_LINKS) seek_links_for_node(new_node_ptr,LINK_LEN, LINK_FORM_PROB); 
	  //cout << "...done\n";
	}
      else
	{
	  if (DEBUG) cout << "At wall! Can't grow.";
	  //TODO
	}
    }
}


void Axon::fluctuate_filaments()
{
  int DEBUG0 = 0;

  int findex, nindex; // Filament index and node index
  Filament* f;
  for (int m = 0; m < total_nodes; m++)
    { 
      // Pick a random filament
      findex = (int)floor(NR::ran2(RAND_SEED)*num_filaments);
      f = &filaments[findex];
      nindex = (int)floor(NR::ran2(RAND_SEED)*(f->num_nodes));
      node* fluct_node;
      if (nindex >= 1)
	{
	  fluct_node = &f->nodes[nindex];
	  if (DEBUG0) cout << "Picking the " << nindex + 1 << "th node at " << (fluct_node->point).to_string().str 
			  << " from " << f->num_nodes << " nodes to fluctuate.\n" << flush;
	  if (BUNDLE_FLUCT || fluct_node->num_links == 0)
	    {
	      fluct_node->fluctuate();
	      if (FORM_LINKS)
		seek_links_for_node(fluct_node,LINK_LEN, LINK_FORM_PROB); 
	      
	    }
	  else
	    {
	      if (DEBUG0) cout << "Node is linked already. Cannot fluctuate.\n";
	    }
	}
    }
}


void Axon::break_links()
{
  // Go through each existing links and break it with probability LINK_RM_PROB
  
  Filament* f;
  for (int i = 0; i < num_filaments; i++)
    {
      f = &filaments[i];
      for (int j = 0; j < f->num_nodes; j++)
	{
	  for (int k = 0; k < f->nodes[j].num_links; k++)
	    {
	      if (NR::ran2(RAND_SEED) < LINK_RM_PROB)
		{
		  //cout << "Removed link between nodes " << &f->nodes[j]
		  //   << " and " << f->nodes[j].links[k] << "\n";
		  f->nodes[j].links[k]->remove_link(&f->nodes[j]);
		  f->nodes[j].remove_link(f->nodes[j].links[k]);
		}
	    }
	}
    }
}

void Axon::new_filament()
{
  if (NR::ran2(RAND_SEED) < FILAMENT_BIRTH_PROB)
    {
      Filament f = Filament(.2);//AXON_RADIUS);
      Point start = f.nodes[0].point;
      Point end = Point::sum(start, Point(0,0,.01));
      bool collision = check_collisions(start, end, f.id);
      if (!collision) 
	this->add_filament(f);
    }
}

void Axon::update_filaments()
{
  grow_filaments();
	      
  if (FLUCTUATIONS)
    {
      fluctuate_filaments();
    }

  if (BREAK_LINKS) break_links();
}

void Axon::seek_all_links(double link_len = LINK_LEN, double link_prob = LINK_FORM_PROB)
{
  cout << "Seeking all links\n";
  // Loop through nodes and seek links for each one.
  for (int i = 0; i < num_filaments; i++)
    {
      for (int j = 0; j < filaments[i].num_nodes; j++)
	{
	  seek_links_for_node(&filaments[i].nodes[j], link_len, link_prob); 
	}
    }
}

void Axon::seek_links_for_node(node* new_node_ptr, double link_len = LINK_LEN, double link_prob = LINK_FORM_PROB)
{
  /*
  int foo;
  if (new_node_ptr->grid == NULL){
    cout << "In seek_links.\n" 
	 << "new_node_ptr = " << new_node_ptr << "\n"
	 << "grid is " << new_node_ptr->grid << "\n"
	 << "grid = " << new_node_ptr->grid << "\n"
	 << "up = " << new_node_ptr->up << "\n"
	 << "down = " << new_node_ptr->down << "\n"
	 << "prev = " << new_node_ptr->prev << "\n"
	 << "next = " << new_node_ptr->next << "\n"
	 << "point = " << new_node_ptr->point.to_string().str << "\n";
    cin >> foo;
  }

  */


  Point new_tip = new_node_ptr->point;
  triple ijk = link_grid.point_to_indices(new_tip); //Get indices of new_tip in the grid
  int ii = ijk.i;
  int jj = ijk.j;	
  int kk = ijk.k;
  
  int i,j,k;
  double distance;
  bool linked;

  //cout << "new_node_ptr grid " << new_node_ptr->grid << "\n";

  //if (DEBUG) cout << "At grid cell (" << ii << "," << jj << "," << kk << ")\n";
  
  for (int off_i = -1; off_i <= 1; off_i++)
    {
      for (int off_j = -1; off_j <= 1; off_j++)
	{
	  for (int off_k = -1; off_k <= 1; off_k++)
	    {
	      i = ii + off_i;
	      j = jj + off_j;
	      k = kk + off_k;
	      
	      //cout << "Checking neighboring cell (" << i << "," << j << "," << k << ")\n";
	      
	      if (i < link_grid.isize && i >= 0 &&
		  j < link_grid.jsize && j >= 0 &&
		  k < link_grid.ksize && k >= 0)
		{
		  list<node*>::iterator it = link_grid.cells[i][j][k].begin();
		  linked = false;

		  while (it != link_grid.cells[i][j][k].end())
		    {
		      //cout << "\t\tnode_ptr " << node_ptr << "\n" << flush; 
		      linked = link(*it, new_node_ptr);
		      if (!linked)
			it++;
		      else
			break;
		    }  
		}
	      else
		{
		  //cout << "Grid cell (" << i << "," << j << "," << k << ") out of range\n";
		}
	    }
	}
    }
}

bool Axon::link(node* node_ptr, node* new_node_ptr)
{
  bool linked = false;
  double distance, shift;
  if (node_ptr != new_node_ptr &&
      node_ptr->filament_id != new_node_ptr->filament_id)
    {
      distance = (node_ptr->point).distance_to(new_node_ptr->point);
      //if (DEBUG) cout << "Trying to link nodes at points " << ((node_ptr->point).to_string()).str
      //	      << " and " << ((new_node_ptr->point).to_string()).str << " at distance " << distance << "\n" << flush;
      //Calc distance.
      
      if (distance <= LINK_INTERACTION_LEN)
	{
	  if (NR::ran2(RAND_SEED) < LINK_FORM_PROB)
	    {
	      //Check if both nodes have space for one more link
	      if (MAX_LINKS - node_ptr->num_links >= 1 &&
		  MAX_LINKS - new_node_ptr->num_links >= 1)
		{
		  if(!node_ptr->contains_link(new_node_ptr))
		    {
		      //Push nodes apart or pull them together so that they are at distance LINK_LEN
		      Point delta = Point::difference(node_ptr->point, new_node_ptr->point);
		      shift = 0.5*((LINK_LEN + 2*FILAMENT_RADIUS) - distance);

		      delta.normalize();
		      Point delta_tmp = delta;
		     
		      if (node_ptr->num_links == 0 && new_node_ptr->num_links == 0)
			{
			  // Move both nodes, since they are both free (have no
			  // existing links).
			
			  // Add shift units along delta to node_ptr->point
			  delta_tmp.scale(shift);
			  
			  node_ptr->move(Point::sum(delta_tmp,node_ptr->point));
			 
			  //Add shift units along -delta from new_node_ptr->point
			  
			  delta_tmp.scale(-1.0);
			  new_node_ptr->move(Point::sum(new_node_ptr->point, delta_tmp));
			  //new_node_ptr->print_links();
			  //node_ptr->print_links();
			  linked = true;
			}
		      else if (node_ptr->num_links > 0 && new_node_ptr->num_links == 0)
			{
			  // Move new_node_ptr
			
			  delta_tmp.scale(shift*2.0);
			  delta_tmp.scale(-1.0);
			  new_node_ptr->move(Point::sum(new_node_ptr->point, delta_tmp));
			  linked = true;
			}
		      else if (new_node_ptr->num_links > 0 && node_ptr->num_links == 0)
			{
			  // Move node_ptr
			  delta_tmp.scale(shift*2.0);
			  node_ptr->move(Point::sum(node_ptr->point, delta_tmp));
			  linked = true;
			}
		      else
			{
			  // Both nodes are linked somewhere; neither node is free to move.
			  linked = false;
			}
		      
		   
		      if (DEBUG) cout << "Linking nodes at points " << ((node_ptr->point).to_string()).str
				      << " and " << ((new_node_ptr->point).to_string()).str << " at distance " << distance << "\n" << flush;
		      node_ptr->add_link(new_node_ptr);
		      new_node_ptr->add_link(node_ptr);
		    }
		  else
		    {
		      if (DEBUG) cout << "Could not link. This link already exists.\n" << flush;
		      linked = false;
		    }
		}
	      else
		{
		  if (DEBUG) cout << "Could not link. At least one node has no more room for links. \n" << flush;
		  linked = false;
		}
	    }
	}
    }
  return linked;
}


double clip(double min, double max, double val)
{
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}


bool Axon::collision(Point s, Point e, Point p, Point q, double r)
{
  int DEBUG0 = 0;
  // Checks for collision between 2 cylinders of radius r, respectively
  // from s to e and p to q.
  
  // Doesn't work for collisions between parallel segments, but this should
  // never happen.

  double u,v,numu,denomu,numv,denomv;
  Point es = Point::difference(e,s);
  Point qp = Point::difference(q,p);
  Point sp = Point::difference(s,p);
 
  
  if (abs(Point::dot(es,qp) - es.norm()*qp.norm()) < 0.000000001)
    {
      // Segments are parallel.  Assume they
      // don't collide to avoid annoying special
      // cases.
      //     cout << "Segments are parallel\n";
      //cout << Point::dot(es,qp) - es.norm()*qp.norm() << "\n";
      return false;
    }
  
  numv = Point::dot(es,sp)*Point::dot(es,qp)
    - Point::dot(sp,qp)*Point::dot(es,es);
  if (DEBUG0) cout << "numv = " << numv << "\n";

  denomv = pow(Point::dot(es,qp),2.0) 
    - Point::dot(qp,qp)*Point::dot(es,es);
  if (DEBUG0) cout << "denomv = " << denomv << "\n";

  v = clip(0,1,numv/denomv);
  if (DEBUG0) cout << "v = " << v << "\n";

  numu = v*Point::dot(es,qp) - Point::dot(es,sp);
  if (DEBUG0) cout << "numu = " << numu << "\n";

  denomu = Point::dot(es,es);
  if (DEBUG0) cout << "denomu = " << denomu << "\n";

  u = clip(0,1,numu/denomu);
  if (DEBUG0) cout << "u = " << u << "\n";

  es.scale(u);
  qp.scale(v);

  Point min_dist = Point::sum(sp,Point::difference(es,qp));
  if (DEBUG0) cout << "min dist = " << min_dist.norm() << "\n";
  return (min_dist.norm() < 2*r);
}


bool Axon::check_collisions(Point p, Point q, int filament_id)
{
  int DEBUG0 = 0;

  // Return true if the segment formed by p and q
  // collides with any other segment, false otherwise.
  
  triple ijk_p = collide_grid.point_to_indices(p); 
  int pi = ijk_p.i;
  int pj = ijk_p.j;	
  int pk = ijk_p.k;

  triple ijk_q = collide_grid.point_to_indices(q); 
  int qi = ijk_q.i;
  int qj = ijk_q.j;	
  int qk = ijk_q.k;
  
  int mini = min(pi-1,qi-1);
  int maxi = max(pi+1,qi+1);
  int minj = min(pj-1,qj-1);
  int maxj = max(pj+1,qj+1);
  int mink = min(pk-1,qk-1);
  int maxk = max(pk+1,qk+1);
  

  int i,j,k;
  double distance;
  bool collided;

  //cout << "new_node_ptr grid " << new_node_ptr->grid << "\n";

  for (i = mini; i <= maxi; i++)
    {
      for (j = minj; j <= maxj; j++)
	{
	  for (k = mink; k <= maxk; k++)
	    {
		      
	    	      
	      if (i < collide_grid.isize && i >= 0 &&
		  j < collide_grid.jsize && j >= 0 &&
		  k < collide_grid.ksize && k >= 0)
		{
		  //cout << "Checking neighboring cell (" << i << "," << j << "," << k << ")\n";
		  list<node*>::iterator it = collide_grid.cells[i][j][k].begin();
		  collided = false;

		  while (it != collide_grid.cells[i][j][k].end())
		    {
		      if ((*it) == NULL)
			{
			  if (DEBUG0) cout << "Node pointer in check_collision is NULL\n";
			  it++;
			  continue;
			}
		      if ((*it)->up != NULL)
			{
			  if (DEBUG0) cout << "Trying to collide segments [" 
					   << (*it)->up->point.to_string().str << ";" 
					   << (*it)->point.to_string().str << "] and [" 
					   << p.to_string().str << ";" << q.to_string().str << "] \n";
			  if ((*it)->filament_id != filament_id)
			    collided = collision(p, q, (*it)->up->point, (*it)->point, FILAMENT_RADIUS);
			  if (collided)
			    {
			      return true;
			    }
			}
		      if ((*it)->down != NULL)
			{
			  if (DEBUG0) cout << "Trying to collide segments [" 
					   << (*it)->down->point.to_string().str << ";" 
					   << (*it)->point.to_string().str << "] and [" 
					   << p.to_string().str << ";" << q.to_string().str << "] \n";
			  if ((*it)->filament_id != filament_id)
			    collided = collision(p, q, (*it)->down->point, (*it)->point, FILAMENT_RADIUS);
			  if (collided)
			    {
			      return true;
			    }
			} 
		      it++;
		    }  
		}
	      else
		{
		  //cout << "Grid cell (" << i << "," << j << "," << k << ") out of range\n";
		}
	    }
	}
    }
  return false;
}


vector<Point> Axon::cross_section(double z, vector<Point> points)
{
  // Takes an empty vector of points and fills it with the points in the
  // cross-section at z.


  int i_below,i_above;
  double t,z0,x0,y0,dx,dy,dz,xi,yi,zi;
  double x1,y1,z1;
  Point delta, above, below;
  for (int i = 0; i < num_filaments; i++)
    {
      i_below = filaments[i].slice(z);
      i_above = i_below + 1;
      
      if (i_above <= filaments[i].num_nodes - 1)
	{
	  above = filaments[i].nodes[i_above].point;
	  below = filaments[i].nodes[i_below].point;
	  delta = Point::difference(above,below);
	  dx = delta.x;
	  dy = delta.y;
	  dz = delta.z;
	  
	  x0 = below.x;
	  y0 = below.y;
	  z0 = below.z;

	  x1 = above.x;
	  y1 = above.y;
	  z1 = above.z;

	  //cout << "below " << x0 << "," << y0 << "," << z0 << "\n"
	  //     << "above " << x1 << "," << y1 << "," << z1 << "\n";
	  
	  t = (z - z0)/dz;

	  xi = x0 + dx*t;
	  yi = y0 + dy*t;
	  zi = z0 + dz*t;
	  
	  //cout << xi << "," << yi << "," << zi << "\n";
	  if (z1 > z)
	    {
	      points.push_back(Point(xi,yi,zi));
	    }
	}
    }
 
  return points;
}


vector<double> Axon::cross_angles(double z, vector<double> angles)
{
  // Returns a list of the angles with which filaments penetrate
  // height z.
  //  cout << "In cross_angles \n";
  int i_below,i_above;
  double t,z0,x0,y0,dx,dy,dz,xi,yi,zi;
  double x1,y1,z1;
  Point delta, above, below;
  for (int i = 0; i < num_filaments; i++)
    {
      i_below = filaments[i].slice(z);
      i_above = i_below + 1;
      
      if (i_above <= filaments[i].num_nodes - 1)
	{
	  above = filaments[i].nodes[i_above].point;
	  below = filaments[i].nodes[i_below].point;
	  delta = Point::difference(above,below);
	  delta.normalize();
	  
	  //cout << xi << "," << yi << "," << zi << "\n";
	  if (above.z > z)
	    {
	      //cout << "Pushing back angle \n";
	      angles.push_back(180*acos(Point::dot(delta,Point(0,0,1)))/PI);
	    }
	}
    }
  return angles;
}


int Axon::count_links()
{
  Filament* f;
  node* n;
  int num_links = 0;
  for (int i = 0; i < num_filaments; i++)
    {
      f = &filaments[i];
      for (int j = 0; j < f->num_nodes; j++)
	{
	  n = &f->nodes[j];
	  num_links += n->num_links;
	}
    }
  return (int)(num_links/2);
}


void Axon::write_cross_section(vector<Point> points, char* outfile)
{
  output.open(outfile);
  double x,y,z;
  //  output << "# Cross-section penetration points\n";
  for (int i = 0; i < points.size(); i++)
    {
      x = points[i].x;
      y = points[i].y;
      z = points[i].z;
     
      output << x << " " << y << "\n"; 
   }

  output.close();
}

void Axon::write_cross_angles(vector<double> angles, char* outfile)
{
  output.open(outfile,ios::app);
  // output << "# Cross-section penetration angles\n";
  for (int i = 0; i < angles.size(); i++)
    {
      output << angles[i] << "\n"; 
    }

  output.close();
}

void Axon::write_scene(char* outfile)
{
  // Writes current scene to a .pov file for rendering
  output.open(outfile);
  output << "camera {\n"
	 << "\t location <35,0,25> \n"
	 << "\t look_at <0,0,25> \n"
	 << "} \n"
	 << "light_source{ \n"
	 << "\t <20,5,10> \n"
	 << "\t color rgb <1,1,1> \n"
	 << "} \n";

  // Cutting plane
  output << "plane{\n <0,0,1>, 30}\n";

  Filament* f;
  double x0,y0,z0,x1,y1,z1,xp,yp,zp;
  
  // Draw the axon as a transparent cylinder
  output << "cylinder{ \n"
	 << "\t <0,0,0>, "
	 << "<0,0," << this->length << ">," << this->r << "\n"
	 << "\t pigment { \n"
	 << "\t\t color rgbt <1,1,1,.95> \n"
	 << "\t } \n"
	 << "} \n";

  // Cylinder drawing loop -- draws each segment as a cylinder 

  for (int i = 0; i < num_filaments; i++)
    {
      f = &filaments[i];
     
      output << "union\n{\n";
      if (f->num_nodes > 0)
	{
	  x0 = (f->nodes[0].point).x;
	  y0 = (f->nodes[0].point).y;
	  z0 = (f->nodes[0].point).z;
	}
           
      for (int j = 1; j < f->num_nodes; j++)
	{
	  // Draw segments
	  x1 = (f->nodes[j].point).x;
	  y1 = (f->nodes[j].point).y;
	  z1 = (f->nodes[j].point).z;
	  
	  output << "cylinder{ \n"
		 << "\t <" << x0 << "," << y0 << "," << z0 << ">, "
		 << "<" << x1 << "," << y1 << "," << z1 << ">, " 
		 << FILAMENT_RADIUS << "\n"
		 << "\t pigment { \n"
		 << "\t\t color rgb <0,1,0> \n"
		 << "\t } \n"
		 << "} \n";

	  x0 = x1;
	  y0 = y1;
	  z0 = z1;

	  // Draw links between nodes
	  if (f->nodes[j].num_links != 0)
	    {
	      for (int k = 0; k < f->nodes[j].num_links; k++)
		{
		  xp = (f->nodes[j].links[k]->point).x;
		  yp = (f->nodes[j].links[k]->point).y;
		  zp = (f->nodes[j].links[k]->point).z;
		  
		  //Point diff = Point::difference(f->nodes[j].point,f->nodes[j].links[k]->point);
		  //cout << diff.norm() << "\n";
		  // cout << sqrt((x0-xp)*(x0-xp) + (y0-yp)*(y0-yp)+ (z0-zp)*(z0-zp)) << "\n";
		  
		  output << "sphere{ \n"
			 << "\t <" << 0.5*(x0+xp) << "," << 0.5*(y0+yp) << "," << 0.5*(z0+zp) << ">, "
		    //<< "\t <" << xp << "," << yp << "," << zp << ">, .025 \n"
			 << LINK_LEN << "\n"
			 << "\t pigment { \n"
			 << "\t\t color rgb <1,0,0> \n"
			 << "\t } \n"
			 << "} \n";
		}
	    }
	}
          output << "}\n";
    } 
  output.close();
}



vector<Point> Analysis::get_points_from_file(char* infile, vector<Point> points)
{
  int i,j,readsize;
  input.open(infile);
    
  string line;
  string buf1,buf2;
  double d1,d2;

  getline(input,line);
  readsize = line.length();

  while (!input.eof())
    {
      //cout << "readsize = " << readsize << "\n";
      if (line.at(0) == '#')
	{
	  getline(input,line);
	  readsize = line.length();
	  continue;
	}
      
      for (i = 0; i < readsize; i++)
	{
	  if (line.at(i) == ' ')
	    break;
	}
      buf1 = line.substr(0,i);
      buf2 = line.substr(i+1,readsize - i - 1);

      //cout << buf1 << "END\n"<<flush;
      //cout << buf2 << "END\n"<<flush;
      
      d1 = strtod(buf1.data(),NULL);
      d2 = strtod(buf2.data(),NULL);
     
      points.push_back(Point(d1,d2,0));
      //cout << d1 << "\n"<<flush;
      //cout << d2 << "\n\n"<<flush;
       
      //      cout << "Reading new line ... \n"<<flush;
      getline(input, line);
      readsize = line.length();
    }
  input.close();

  return points;
}

double Analysis::get_radius_from_file(char* infile)
{
  input.open(infile);
  
  char buf [10];
  input.getline(&buf[0],10);
  
  input.close();

  return strtod(&buf[0],NULL);
}

vector<double> Analysis::get_vals_from_file(char* data_file, vector<double> vals)
{
  int readsize;
  double val;
  string line;
  input.open(data_file);
  getline(input,line);
  readsize = line.length();

  while(! input.eof())
    {
      if (line.at(0) == '#')
	{
	  getline(input,line);
	  readsize = line.length();
	  continue;
	}

      val = strtod(line.data(),NULL);
      vals.push_back(val);
      getline(input,line);
      readsize = line.length();
    }

  input.close();
  return vals;
}

void Analysis::write_energy_hist(char* energy_file, char* outfile, int num_bins = 350)
{
  // Get energies from file
  vector<double> energies = vector<double>();
  energies = get_vals_from_file(energy_file, energies);
  cout << "energies size " << energies.size() << "\n"; 

   // Find maximum energy in the list
  double max = energies[0];
  double min = energies[0];
  
  /*
  for (int i = 0; i < energies.size(); i++)
    {
      if (energies[i] < min) min = energies[i];
      if (energies[i] > max) max = energies[i];
    }
  
  cout << "max energy = " << max << "\n"
       << "min energy = " << min << "\n";
  */
  
  min = 0;
  max = 1000;
  

  cout << "binning energies \n";
  vector<int> hist = bin(energies, num_bins, max, min);
  double bin_size = (double)(max - min)/num_bins;

  output.open(outfile);
  for (int i = 0; i < num_bins; i++)
    {
      output << (i + 0.5)*bin_size << " " << (double)hist[i]/energies.size() << "\n";
    }
  output.close();
}

vector<int> Analysis::bin(vector<double> data, int num_bins)
{
  // Determine min and max
  double min = data[0];
  double max = data[0];
  
  for (int i = 0; i < data.size(); i++)
    {
      if (data[i] < min) min = data[i];
      if (data[i] > max) max = data[i];
    }
  
  return bin(data, num_bins, max, min);
}

vector<int> Analysis::bin(vector<double> data, int num_bins, int max, int min)
{

  vector<int> hist = vector<int>();
  // Initialize hist to zeros
  for (int i = 0; i < num_bins; i++)
    {
      hist.push_back(0);
    }

  double bin_size = (double)(max - min)/num_bins;
  
  for (int i = 0; i < data.size(); i++)
    {
      //      cout << "hist[" << (int)floor((data[i] - min)/bin_size) << "] = "
      //         << hist[(int)floor((data[i] - min)/bin_size)] << "\n";
      if (data[i] >= min && data[i] <= max)
	hist[(int)floor((data[i] - min)/bin_size)] += 1;
    }
  return hist;
}

void Analysis::write_angles_hist(char* angle_file, char* outfile, int num_bins = 150)
{
  vector<double> angles = vector<double>();
  cout << "getting angles from file \n";
  angles = get_vals_from_file(angle_file,angles);
  
  double min = 0;
  double max = 180;
  
  for (int i = 0; i < angles.size(); i++)
    {
      if (angles[i] < min) min = angles[i];
      if (angles[i] > max) max = angles[i];
    }

  vector<int> hist = Analysis::bin(angles, num_bins, max, min); 
  double bin_size = (double)(max - min)/num_bins;
  // Go through hist and write data to file
  cout << "Writing histogram to file \n";
  output.open(outfile);
  for (int i = 0; i < num_bins; i++)
    {
      //cout << "hist[" << i << "] = " << hist[i] << "\n"
      //   << hist[i] << "/" << angles.size() << " = " << (double)hist[i]/angles.size() << "\n";
      output << (i + 0.5)*bin_size << " " << (double)hist[i]/angles.size() << "\n";
    }
  output.close();
  cout << "\n\n\n";
}

void Analysis::write_distance_hist_all_pairs(char* point_file, char* radius_file,
					     char* outfile, int num_bins = 100)
{
  vector<Point> points = vector<Point>();
  points = get_points_from_file(point_file, points);

  double r = get_radius_from_file(radius_file);

  double dist,max_dist, bin_size;
  int* hist;
  hist = (int*)malloc(sizeof*hist * num_bins);
  max_dist = 2*r;
  bin_size = (double)max_dist/num_bins;

  // Initialize hist to 0;
  for (int i = 0; i < num_bins; i++)
    {
      hist[i] = 0;
    }
  
  for (int i = 0; i < points.size(); i++)
    {
      for (int j = i + 1; j < points.size(); j++)
	{
	  dist = points[i].distance_to(points[j]);
	  hist[(int)floor(dist/bin_size)] += 1;
	}
    }
  output.open(outfile);
  for (int i = 0; i < num_bins; i++)
    {
      output << i*bin_size + 0.5*bin_size << "\t" << (double)hist[i]/points.size() << "\n";
    }
  output.close();
}


void Analysis::write_distance_hist_nn(char* point_file, char* radius_file, 
				      char* outfile, double dist_range, int num_bins = 20)
{
  // Computes a distance histogram looking only at "nearest"-neighbors,
  // that is, points that are in a circle of radius dist_range
  // centered at the current point.
  //  cout << "In write_distance_hist_nn()\n";

  vector<Point> points = vector<Point>();
  points = get_points_from_file(point_file, points);
  // cout << "got " << points.size() << "points\n";

  double r = get_radius_from_file(radius_file);
  //cout << "got radius: " << r << "\n";

  double dist,max_dist, min_x, min_y, bin_size, bucket_size;
  
  int* hist;
  hist = (int*)malloc(sizeof*hist * num_bins);

  bucket_size = 2*dist_range;
  bin_size = (double)dist_range/num_bins;
 
  max_dist = r*2;
  min_x = min_y = -r;
  
  // Map all points into a 2D grid with cells of side-length bucket_size
  
  // cout << "Allocating memory for grid\n";
  // Allocate memory for grid
  vector<Point>** grid;
  int num_buckets = (int)ceil((double)max_dist/bucket_size);
  grid = new vector<Point>*[num_buckets];
  for (int i = 0; i < num_buckets; i++)
    {
      grid[i] = new vector<Point>[num_buckets];
    }
  
  //cout << "Done. num_buckets = " << num_buckets << "\n";

  //cout << "Mapping all points to grid\n";
  
  double x,y;
  int i,j;
  for (int m = 0; m < points.size(); m++)
    {
      x = points[m].x;
      y = points[m].y;
      i = (int)floor((x-min_x)/bucket_size);
      j = (int)floor((y-min_y)/bucket_size);
      //  cout << "Pushing back " << m << "th point address at " << i << "," << j << "\n";
      grid[i][j].push_back(points[m]);
    }
  //cout << "Done.\n";
  
  //cout << "Initializing frequencies to 0\n";
  // Initialize hist to 0;
  for (int i = 0; i < num_bins; i++)
    {
      hist[i] = 0;
    }
  //cout << "Done.\n";

  //cout << "Computing distances\n";
  int num_distances = 0;
  for (int m = 0; m < points.size(); m++)
    {
      x = points[m].x;
      y = points[m].y;
      i = (int)floor((x-min_x)/bucket_size);
      j = (int)floor((y-min_y)/bucket_size);
      
      for (int k = 0; k < grid[i][j].size(); k++)
	{
	  //  cout << "Computing distance " << k << " from point " << m 
	  //     << " in cell " << i << "," << j << "\n"; 

	  dist = points[m].distance_to(grid[i][j][k]);
	  if (dist < bucket_size/2.0 && dist > 0.00001) //if in circle
	    {
	      hist[(int)floor(dist/bin_size)] += 1;
	      num_distances++;
	    }
	}
    }

  //cout << "Writing histogram data\n";
  output.open(outfile);
  for (int i = 0; i < num_bins; i++)
    {
      if (num_distances != 0)
	output << i*bin_size + 0.5*bin_size << "\t" << (double)hist[i]/num_distances << "\n";
    }
  output.close();

  //cout << "Done writing distance histogram\n";
}



void run()
{
 
  Axon a = Axon(MAX_NODES*MEAN_LEN_GROWTH);
  
  // File for writing output
  char* outfile;
  char* outfile2;
  char* outfile3;

  // Empty vectors for computing cross section data
  vector<Point> cross_section_pts;
  vector<double> cross_section_angles;

  outfile = (char*)malloc(sizeof*outfile * 50);
  outfile2 = (char*)malloc(sizeof*outfile2 * 50);
  outfile3 = (char*)malloc(sizeof*outfile3 * 50);

  for (int j = 0; j < NUM_STEPS; j++)
    {
      a.new_filament();
      a.update_filaments();
      
      if ((j+1)%100 == 0)
	cout << "step " << j+1 << "/" << NUM_STEPS << "\n";
      if ((j+1) > 1 && (j+1)%100 == 0)
	{
	  // Write scene
	  sprintf(outfile,"%s%d%s","./anim/scene",j+1,".pov");
	  a.write_scene(outfile);
	  cout << "Wrote scene at step " << j+1 << " to " << outfile << "\n";
	  
	  // Count the number of links
	  cout << "num_links = " << a.count_links() << "\n";
	  
	  // Count the number of filaments
	  cout << "num_filaments = " << a.num_filaments << "\n";
	    
	  // Write cross section points
	  double z = 2.0;
	  sprintf(outfile2,"%s%d%s","./anim/cross",(j+1),".dat");
	  cross_section_pts = vector<Point>();
	  cross_section_pts = a.cross_section(z,cross_section_pts);
	  cout << "num_pts = " << cross_section_pts.size() << "\n";
	  a.write_cross_section(cross_section_pts, outfile2);
	  cout << "Wrote cross section points at step " << j+1 << " to " << outfile2 << "\n";

	  
	  // Write cross section angles
	  // Write cross section angles
	  sprintf(outfile3,"%s%d%s%d%s","./anim/cross_angles_",(j+1),"_",(int)z,".dat");
	  cross_section_angles = vector<double>();
	  cross_section_angles = a.cross_angles(z,cross_section_angles);
	  cout << "num_pts (angles) = " << cross_section_angles.size() << "\n";
	  a.write_cross_angles(cross_section_angles, outfile3);
	  cout << "Wrote cross section angles at step " << j+1 << " to " << outfile3 << "\n";

	  z = 1;
	  sprintf(outfile3,"%s%d%s%d%s","./anim/cross_angles_",(j+1),"_",(int)z,".dat");
	  cross_section_angles = vector<double>();
	  cross_section_angles = a.cross_angles(z,cross_section_angles);
	  cout << "num_pts (angles) = " << cross_section_angles.size() << "\n";
	  a.write_cross_angles(cross_section_angles, outfile3);
	  cout << "Wrote cross section angles at step " << j+1 << " to " << outfile3 << "\n";

	  z = .5;
	  sprintf(outfile3,"%s%d%s%d%s","./anim/cross_angles_",(j+1),"_",(int)z,".dat");
	  cross_section_angles = vector<double>();
	  cross_section_angles = a.cross_angles(z,cross_section_angles);
	  cout << "num_pts (angles) = " << cross_section_angles.size() << "\n";
	  a.write_cross_angles(cross_section_angles, outfile3);
	  cout << "Wrote cross section angles at step " << j+1 << " to " << outfile3 << "\n";
	  
	  // Print out fluctuation statistics
	  cout << "Fluctuation acceptance rate = " << 1 - (double)num_rejected/num_good << "\n";
	}
    }
  cout << "Completed " << NUM_STEPS << " steps\n";
 
}


void analyze()
{
// Analysis

//  string pts_hist = "anim/cross_hist400.plot";
//  string pts_data = "anim/cross400.dat";
//  string radius_file = "anim/radius.dat";
  
  string angl_hist_300_0 = "anim/cross_angles_300_0.plot";
  string angl_data_300_0 = "anim/cross_angles_300_0.dat";

  string angl_hist_300_1 = "anim/cross_angles_300_1.plot";
  string angl_data_300_1 = "anim/cross_angles_300_1.dat";
  
  Analysis::write_angles_hist((char*)angl_data_300_0.data(),(char*)angl_hist_300_0.data());
  
  Analysis::write_angles_hist((char*)angl_data_300_1.data(),(char*)angl_hist_300_1.data());

  //  Analysis::write_distance_hist_nn((char*)infile.data(), (char*)radius_file.data(), (char*)outfile.data(), .1); 
  
  string fluct_energies_data = "anim/fluct_energies.dat";
  string fluct_energies_hist = "anim/fluct_energies.plot";

  Analysis::write_energy_hist((char*)fluct_energies_data.data(), (char*)fluct_energies_hist.data()); 
}

//Main
int main()
{
    
  for (int i = 0; i < NUM_REPS; i++)
    { 
      run();
      //      analyze();
    }
  
  //analyze();
  
  return 0;
}

