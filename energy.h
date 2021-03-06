#ifndef ENERGY_H
#define ENERGY_H

#include "vector"
#include "field.h"
#include "mesh.h"
#include "mass.h"
#include "globals.h"

class Energy : public Field {
private:
	Globals * consts;
	Field * u;
	Field * v;

	double ** massArray;
	double ** vArray;
	double ** uArray;


	int totalSize;

	double median;
	int count = 0;
	int mark = 0;

	void UpdateDtDissEAvg(void);
	void UpdateOrbitalDissEAvg(void);

public:
	Mass * mass;
	Energy(Mesh*, int, int, Globals *, Field *, Field *, Mass *);
	int timePos;
	std::vector<double> dissipation;

	bool converged = false;

	std::vector<double> residual;

	//vector of length n/(period/dt*1000), for average kinetic energy for n timesteps in one orbit.
	//std::vector<double> dtKinEAvg;

	double * dtKinEAvg;
	//std::vector<double> dtDissEAvg;

	double * dtDissEAvg;

	//vector of length m, for average kinetic energy at periapse for m orbits in simulation.
	double orbitKinEAvg;
	double orbitDissEAvg[2];

  double currentDissEAvg;


	std::vector<double> derivative;
	std::vector<double> minima;
	std::vector<double> maxima;

	void UpdateKinE(double **, double **);

	//Function finds globally averaged kinetic energy at the current timestep and appends it
	//to timeStepGlobalAvg
	void UpdateDtKinEAvg(void);


	//Function finds orbtially and globally averaged kinetic energy at end of the simulation for
	//the last whole orbit complete
	void UpdateOrbitalKinEAvg(int inc);

	void IsConverged(void);

};

#endif
