/* Contains functions to compute the components of the tidal potential gradient.
*
* Each function requires two Field objects to write the gradient in latitude
* and longitude, a Globals object containing constants relevant to the
* calculation, and the current simulation time.
*/

#ifndef TIDALPOTENTIALS_H
#define TIDALPOTENTIALS_H

#include "field.h"
#include <math.h>

// Degree 2 component of the eccentricity tide (see Tyler 2011, Matsuyama 2014)
void deg2Ecc(Field * dUlat, Field * dUlon, double simulationTime, double radius, double omega, double ecc);

// Degree 2 component of the eccentricity-radial tide (see Tyler 2011, Matsuyama 2014)
void deg2EccRad(Field * DUlat, Field * DUlon, double simulationTime, double radius, double omega, double ecc);

// Degree 2 component of the eccentricity-libration tide (see Tyler 2011, Matsuyama 2014)
void deg2EccLib(Field * DUlat, Field * DUlon, double simulationTime, double radius, double omega, double ecc);

// Degree 2 component of the obliquity tide (see Tyler 2011, Matsuyama 2014)
void deg2Obliq(Field * dUlat, Field * dUlon, double simulationTime, double radius, double omega, double theta);

// Degree 3 component of the eccentricity tide (see docs)
void deg3Ecc(Field * DUlat, Field * DUlon, double simulationTime, double radius, double smAxis, double omega, double ecc);

// Degree 3 component of the obliquity tide (see docs)
void deg3Obliq(Field * DUlat, Field * DUlon, double simulationTime, double radius, double smAxis, double omega, double theta);


#endif
