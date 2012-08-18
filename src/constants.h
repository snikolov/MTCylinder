#ifndef constants_h_included
#define constants_h_included


extern int DEBUG;
extern int RAND_SEED;
extern const double LINK_LEN; //Maximum distance at which two microtubules can link
extern const double LINK_PROB; //Probability of two nodes linking if they are close enough
extern const double AXON_RADIUS; //Radius of axon
extern const int MAX_NODES; //Maximum number of nodes in a microtubule filament.
extern const int MAX_LINKS; //Maximum number of linker proteins attached to a node

#endif
