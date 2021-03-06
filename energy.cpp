#include "field.h"
#include "energy.h"
#include "mass.h"
#include "globals.h"
#include "outFiles.h"
#include "mass.h"

#include <math.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>

Energy::Energy(Mesh * mesh, int lat, int lon, Globals * Consts, Field * UVel, Field * VVel, Mass * MassField) : Field (mesh, lat, lon)
{
	consts = Consts;
	// u = UVel;
	// v = VVel;
	mass = MassField;

	// uArray = u->solution;
	// vArray = v->solution;
	massArray = mass->solution;

	timePos = 0;
	totalSize = (int) (consts->period.Value()/consts->timeStep.Value());

	dtKinEAvg = new double[totalSize+1];
	dtDissEAvg = new double[totalSize+1];

  currentDissEAvg = 0;

	orbitDissEAvg[1] = 0;
	orbitDissEAvg[0] = 0;

	dissipation.push_back(0);
};

void Energy::UpdateKinE(double ** u, double ** v) {
	switch (consts->fric_type) {
		//Linear dissipation
	case LINEAR:
		for (int i = 0; i < fieldLatLen - 1; i++) {
			for (int j = 0; j < fieldLonLen; j++) {
				solution[i][j] = 0.5*massArray[i][j] * (u[i][j]*u[i][j] + v[i][j]*v[i][j]);
			}
		}
		break;

	case QUADRATIC:
		for (int i = 0; i < fieldLatLen - 1; i++) {
			for (int j = 0; j < fieldLonLen; j++) {
				solution[i][j] = 0.5*massArray[i][j] * pow((u[i][j]*u[i][j] + v[i][j]*v[i][j]), 1.5);
			}
		}
		break;
	}
};

void Energy::UpdateDtKinEAvg(void) {
	double kineticSum = 0; //Joules

	for (int i = 0; i < fieldLatLen-1; i++) {
		for (int j = 0; j < fieldLonLen; j++) {
			kineticSum += solution[i][j];
		}
	}
	dtKinEAvg[timePos] = kineticSum / (4 * pi*pow(consts->radius.Value(),2)); //Joules per meter^2

	//Automatically update dissipation - ensures vectors of samelength
	UpdateDtDissEAvg();
};

void Energy::UpdateDtDissEAvg(void) {
	switch (consts->fric_type) {
		//Linear dissipation
	case LINEAR:
		dtDissEAvg[timePos] = 2 * dtKinEAvg[timePos] * consts->alpha.Value(); //Joules per meter^2
		break;
	case QUADRATIC:
		dtDissEAvg[timePos] = 2 * dtKinEAvg[timePos] * consts->alpha.Value()/consts->h.Value();
		break;
	default:
		consts->Output.TerminateODIS();
	}

  currentDissEAvg = dtDissEAvg[timePos];

	timePos ++;
};


void Energy::UpdateOrbitalKinEAvg(int inc) {
	orbitKinEAvg = 0;

	//int pos = orbitKinEAvg.size()-1;

	for (unsigned int i = 0; i < timePos; i++) {
		orbitKinEAvg += dtKinEAvg[i];
	}

	orbitKinEAvg /= timePos;//inc;

	//Automatically update dissipation
	UpdateOrbitalDissEAvg();

};

void Energy::UpdateOrbitalDissEAvg(void) {
	orbitDissEAvg[1] = orbitDissEAvg[0];
	orbitDissEAvg[0] = 0;

	switch (consts->fric_type) {
		//Linear dissipation
	case LINEAR:
		orbitDissEAvg[0] = 2 * orbitKinEAvg * consts->alpha.Value(); //Joules per meter
		break;
	case QUADRATIC:
		orbitDissEAvg[0] = 2 * orbitKinEAvg * consts->alpha.Value() / consts->h.Value();
		break;
	}

	//orbitDissEAvg[0] = orbitDissEAvg[0]/(4*pi*pow(consts->radius.Value(),2));

	//Reset time position after updating orbital values
	// timePos = 0;

	dissipation.push_back(orbitDissEAvg[0]);

};

void Energy::IsConverged(void) {
	// if (dissipation.size() > 20 && !converged && (consts->h.Value() > 10.0)) {
	// 	for (int i=derivative.size(); i < dissipation.size() - 1; i++) {
	// 		derivative.push_back(dissipation[i+1]-dissipation[i]);
	// 	}
	//
	// 	for (int i=derivative.size()-2; i < derivative.size()-1; i++) {
	// 		// Check for minima
	// 		if (derivative[i] < 0 && derivative[i+1] > 0) {
	// 			minima.push_back(i);
	// 			std::cout<<"Minima found at orbit "<<i<<std::endl;
	// 		}
	// 		else if (derivative[i] > 0 && derivative[i+1] < 0) {
	// 			maxima.push_back(i);
	// 			std::cout<<"Maxima found at orbit "<<i<<std::endl;
	// 		}
	// 	}
	//
	// 	if (minima.size() == 2) converged = true;
	// }

	//residual.push_back(fabs(orbitDissEAvg[1] - orbitDissEAvg[0]));
	//if (residual.size() > 10 && !converged) {
		//check latest value for convergence
	//	for (unsigned int i = dissipation.size() - 2; i > dissipation.size() - 80; i--) {
	//		converged = true;
	//		if (fabs(dissipation[dissipation.size()-1]-dissipation[i])/dissipation[dissipation.size()-1]*100 >= 0.5) {
	//			converged = false; //reset if previous two values not converged
	//			break;
	//		}
	//	}
	//}

	residual.push_back(fabs(orbitDissEAvg[1] - orbitDissEAvg[0]));
	if (residual.size() > 6 && !converged) {
		//check latest value for convergence
		if (residual[residual.size() - 1] < consts->converge.Value()) {
			converged = true;

			//check previous two values for convergence also:
			//Convergence will be reset if convergence is not consistent over three orbits.
			for (unsigned int i = residual.size() - 2; i > residual.size() - 5; i--) {
				if (residual[i] > consts->converge.Value()) {
					converged = false; //reset if previous two values not converged
					break;
				}
			}
		}
	}

	if (residual[residual.size() - 1] > 1e5) {
		consts->outstring << std::endl << "Residual is now greater than 100,000. Your model appears to have blown up. Sorry Chum." << std::endl;
		consts->Output.Write(ERR_MESSAGE, &consts->outstring);
		consts->Output.TerminateODIS();
	}

	converged = false;
	printf("\t Resdiual: %1.4e \n", residual[residual.size() - 1]);
	if (converged) std::cout << "Convergence criteria met." << std::endl;

	count += 1;
};
