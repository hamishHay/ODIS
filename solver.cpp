#ifdef _WIN32
#include <Windows.h>
#define mkdir CreateDirectory

#elif _WIN64
#include <Windows.h>

#elif __linux__
#include <unistd.h>

#else
#error "OS not supported!"
#endif

#include "solver.h"
#include "globals.h"
#include "field.h"
#include "depth.h"
#include "mesh.h"
#include "mathRoutines.h"
#include "energy.h"
#include "outFiles.h"
#include "vector"

#include <math.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <sys/stat.h>
#include <errno.h>
#include "H5Cpp.h"
#include <signal.h>

#ifndef H5_NO_NAMESPACE
    using namespace H5;
#endif

extern"C"
{
    void extractshcoeff_(double *, int *, int *, double *);
}

int flag = 0;

void CatchExit(int sig) {
  printf("%s\n", "Caught Terminate Signal...");
  flag = 1;
  // DumpFields(output);
}

Solver::Solver(int type, int dump, Globals * Consts, Mesh * Grid, Field * UGradLon, Field * UGradLat, Field * VelU, Field * VelV, Field * Eta, Energy * EnergyField, Depth * Depth_h) {
  solverType = type;
  dumpTime = dump;
  Out = &Consts->Output;

  //Assign member pointers to passed in pointers from main.cpp
  consts = Consts;
  grid = Grid;
  // dUlon = UGradLon;
  // dUlat = UGradLat;
  u = VelU;
  v = VelV;
  eta = Eta;
  energy = EnergyField;

  loading = false;

  // etaOld = new Field(grid,0,0);
  etaOld = eta;
  etaOldArray = etaOld->solution;

  etaNew = new Field(grid,0,0);
  etaNewArray = etaNew->solution;

  uOld = u; //new Field(grid,0,1);
  uOldArray = uOld->solution;

  uNew = new Field(grid,0,1);
  uNewArray = uNew->solution;

  vOld = v; //new Field(grid,1,0);
  vOldArray = vOld->solution;

  vNew = new Field(grid,1,0);
  vNewArray = vNew->solution;

  vDissTerm = new Field(grid, 1, 0);
  vDissArray = vDissTerm->solution;

  uDissTerm = new Field(grid, 0, 1);
  uDissArray = uDissTerm->solution;

  etaVAvg = new Field(grid, 1, 0);
  etaVAvgArray = etaVAvg->solution;

  etaUAvg = new Field(grid, 0, 1);
  etaUAvgArray = etaUAvg->solution;

  etaInterp = new Field(grid, 1, 1);
  etaInterpArray = etaInterp->solution;

  vNorthEastAvg = new Field(grid, 1, 0);
  vNEAvgArray = vNorthEastAvg->solution;

  uSouthWestAvg = new Field(grid, 0, 1);
  uSWAvgArray = uSouthWestAvg->solution;

  dUlat = UGradLat;// new Field(grid,1,0);
  dUlatArray = dUlat->solution;

  dUlon = UGradLon; // new Field(grid,0,1);
  dUlonArray = dUlon->solution;

  depth = Depth_h;
  depthArray = Depth_h->solution;

  oceanLoading = new Field(grid, 0, 0);
  oceanLoadingArray = oceanLoading->solution;


  //newRadius = new Depth(grid);
  //newRadiusArray = newRadius->solution;

  l_max = consts->l_max.Value();

  SH_cos_coeff = new double*[l_max+1];
  SH_sin_coeff = new double*[l_max+1];
  for (int i=0; i<l_max+1; i++) {
    SH_cos_coeff[i] = new double[l_max+1];
    SH_sin_coeff[i] = new double[l_max+1];
  }

  uLatLen = u->fieldLatLen;
  uLonLen = u->fieldLonLen;
  udLon = u->dLon;
  udLat = u->dLat;

  vLatLen = v->fieldLatLen;
  vLonLen = v->fieldLonLen;
  vdLon = v->dLon;
  vdLat = v->dLat;

  etaLatLen = eta->fieldLatLen;
  etaLonLen = eta->fieldLonLen;
  etadLon = eta->dLon;
  etadLat = eta->dLat;

  dt = consts->timeStep.Value();

  for (int i=0; i<etaLatLen; i++) {
    for (int j=0; j<etaLonLen; j++) {
      oceanLoadingArray[i][j] = 0.0;
    }
  }
  etaLegendreArray = new double**[etaLatLen];
  for (int i=0; i<etaLatLen; i++) {
    etaLegendreArray[i] = new double*[l_max+1];
    for (int l=0; l<l_max+1; l++) {
      etaLegendreArray[i][l] = new double[l_max+1];
      for (int m=0; m <= l; m++) {
        etaLegendreArray[i][l][m] = assLegendre(l, m, etaNew->cosCoLat[i]);
        // std::cout <<"i="<<i<<", l="<<l<<", m="<<m<<'\t'<< etaLegendreArray[i][l][m]<<'\t'<<etaNew->cosCoLat[i] << std::endl;
      }
    }
  }

  etaCosMLon = new double*[etaLonLen];
  etaSinMLon = new double*[etaLonLen];
  for (int j=0; j<etaLonLen; j++) {
    etaCosMLon[j] = new double[l_max+1];
    etaSinMLon[j] = new double[l_max+1];
    for (int m=0; m<l_max+1; m++){
      etaCosMLon[j][m] = eta->cosMLon[m][j];
      etaSinMLon[j][m] = eta->sinMLon[m][j];
    }
  }

  loadK = new double[l_max+1];
  loadH = new double[l_max+1];
  gammaFactor = new double[l_max+1];
  shPower = new double*[l_max+1];
  lm_solve = new int*[l_max+1];
  l_solve_len = l_max+1;

  for (int l=0; l<l_max+1; l++) {
    shPower[l] = new double[l_max+1];
    lm_solve[l] = new int[2];
  }




  double mu_bar = 0.0;

  for (int l=0; l<l_max+1; l++) {
    mu_bar = 6.78e9/(1610.0*consts->g.Value()*consts->radius.Value());
    mu_bar *= (double)(2*l*l + 4*l +3)/((double)l);

    loadK[l] = -1.0 / (1.0 + mu_bar);
    loadH[l] = -1.0 / (1.0 + mu_bar) * (2.0*(double)l + 1.0)/3.0;


    gammaFactor[l] = 1.0 - (1 + loadK[l] - loadH[l]) * 3000.0 / ((2*l + 1) * 1610.0);
    gammaFactor[l] = 1.0;

  }

  cosMinusB = new double[dUlat->fieldLonLen];
  cosPlusB = new double[dUlat->fieldLonLen];
  sinMinusB = new double[dUlon->fieldLonLen];
  sinPlusB = new double[dUlon->fieldLonLen];

  if (consts->potential.Value() == "ECC_RAD") tide = ECC_RAD;
  else if (consts->potential.Value() == "ECC_LIB") tide = ECC_LIB;
  else if (consts->potential.Value() == "ECC") tide = ECC;
  else if (consts->potential.Value() == "OBLIQ") tide = OBLIQ;
  else if (consts->potential.Value() == "FULL") tide = FULL;
  else if (consts->potential.Value() == "TOTAL") tide = TOTAL;
  else {
    outstring << "No potential forcing found." << std::endl;
    Out->Write(ERR_MESSAGE, &outstring);
    Out->TerminateODIS();
  }

  CreateHDF5FrameWork();

  signal(SIGINT, CatchExit);

};

int Solver::InitialConditions(void) {
  bool action = consts->init.Value();

  outstring << "Use initial conditions: ";

  if (action) {
    outstring << "Yes." << std::endl;
    Out->Write(OUT_MESSAGE, &outstring);
    ReadInitialConditions();
    return 1;
  }

  outstring << "No." << std::endl;
  Out->Write(OUT_MESSAGE, &outstring);
  return 0;
};

void Solver::Solve() {
  outstring << "Entering solver: ";
  switch (solverType) {
  case 0:
    outstring << "Explicit\n";
    Explicit();
    break;
  default:
    outstring << "No solver type selected.\n Terminating execution.";
  }
};

void Solver::UpdatePotential() {
  switch (tide) {
  case ECC_RAD:
    UpdateEccRadPotential();
    break;

  case ECC_LIB:
    UpdateEccLibPotential();
    break;

  case ECC:
    UpdateEccPotential();
    break;

  case OBLIQ:
    UpdateObliqPotential();
    break;

  case FULL:
    UpdateFullPotential();
    break;

  case TOTAL:
    UpdateTotalPotential();
    break;
  }
}

inline void Solver::UpdateEccLibPotential(void) {
  double sin2Lat = 0;
  double cosLat = 0;
  double B = consts->angVel.Value()*simulationTime;
  double A = 0.25*pow(consts->angVel.Value(), 2)*pow(consts->radius.Value(), 2)*consts->e.Value();
  double Alat, Alon = 0;

  for (int j = 0; j < dUlat->fieldLonLen; j++) {
    cosMinusB[j] = cos(2 * dUlat->lon[j] - B);
    cosPlusB[j] = cos(2 * dUlat->lon[j] + B);
  }

  for (int j = 0; j < dUlon->fieldLonLen; j++) {
    sinMinusB[j] = sin(2 * dUlon->lon[j] - B);
    sinPlusB[j] = sin(2 * dUlon->lon[j] + B);
  }

  Alat = A * -1.5;
  for (int i = 0; i < dUlat->fieldLatLen; i++) {
    //lat = 2*dUlat->lat[i];
    sin2Lat = dUlat->sin2Lat[i];
    for (int j = 0; j < dUlat->fieldLonLen; j++) {
      dUlatArray[i][j] = Alat*(sin2Lat * (7 * cosMinusB[j] - cosPlusB[j])); //P22
    }
  }

  Alon = A * 3;
  for (int i = 0; i < dUlon->fieldLatLen; i++) {
    cosLat = dUlon->cosLat[i];
    for (int j = 0; j < dUlon->fieldLonLen; j++) {
      dUlonArray[i][j] = Alon * cosLat*cosLat*(-7 * sinMinusB[j] + sinPlusB[j]); //P22
    }
  }
}

inline void Solver::UpdateEccPotential(void) {
  double cosLat = 0;
  double sin2Lat = 0;
  double cosB = 0;
  double B = consts->angVel.Value()*simulationTime;
  double A = 0.25*pow(consts->angVel.Value(), 2)*pow(consts->radius.Value(), 2)*consts->e.Value();

  for (int j = 0; j < dUlat->fieldLonLen; j++) {
    cosMinusB[j] = cos(2 * dUlat->lon[j] - B);
    cosPlusB[j] = cos(2 * dUlat->lon[j] + B);
  }

  cosB = cos(B);
  for (int i = 0; i < dUlat->fieldLatLen; i++) {
    sin2Lat = dUlat->sin2Lat[i];
    for (int j = 0; j < dUlat->fieldLonLen; j++) {
      //lon = dUlat->lon[j];
      dUlatArray[i][j] = A*((-1.5*sin2Lat * (7 * cosMinusB[j] - cosPlusB[j])) + (-9.*sin2Lat*cosB)); //P22 + P20
    }
  }

  for (int j = 0; j < dUlon->fieldLonLen; j++) {
    sinMinusB[j] = sin(2 * dUlon->lon[j] - B);
    sinPlusB[j] = sin(2 * dUlon->lon[j] + B);
  }

  for (int i = 0; i < dUlon->fieldLatLen; i++) {
    cosLat = dUlon->cosLat[i];
    for (int j = 0; j < dUlon->fieldLonLen; j++) {
      //lon = dUlon->lon[j];
      dUlonArray[i][j] = A * 3 * cosLat*cosLat*(-7 * sinMinusB[j] + sinPlusB[j]); //P22
    }
  }
}

inline void Solver::UpdateEccRadPotential(void) {
  double cosB = 0;
  double sin2Lat = 0;
  double B = consts->angVel.Value()*simulationTime;
  double A = 0.25*pow(consts->angVel.Value(), 2)*pow(consts->radius.Value(), 2)*consts->e.Value();
  double value = 0;

  cosB = cos(B);
  for (int i = 0; i < dUlat->fieldLatLen; i++) {
    sin2Lat = dUlat->sin2Lat[i]; //sin(2*lat)
    value = A*(-9.*sin2Lat*cosB);
    for (int j = 0; j < dUlat->fieldLonLen; j++) {
      dUlatArray[i][j] = value; // P20
    }
  }
}

inline void Solver::UpdateObliqPotential(void) {
  double cos2Lat = 0;
  double sin2Lat = 0;
  double value = 0;
  double B = consts->angVel.Value()*simulationTime;
  double A = 0.25*pow(consts->angVel.Value(), 2)*pow(consts->radius.Value(), 2)*consts->theta.Value();

  for (int j = 0; j < dUlat->fieldLonLen; j++) {
    cosMinusB[j] = cos(dUlat->lon[j] - B);
    cosPlusB[j] = cos(dUlat->lon[j] + B);
  }

  for (int i = 0; i < dUlat->fieldLatLen; i++) {
    //lat = dUlat->lat[i];
    cos2Lat = dUlat->cos2Lat[i];
    value = -6 * A*cos2Lat;
    for (int j = 0; j < dUlat->fieldLonLen; j++) {
      //lon = dUlat->lon[j];
      dUlatArray[i][j] = value*(cosMinusB[j] + cosPlusB[j]); //P21
    }
  }

  for (int j = 0; j < dUlon->fieldLonLen; j++) {
    sinMinusB[j] = sin(dUlon->lon[j] - B);
    sinPlusB[j] = sin(dUlon->lon[j] + B);
  }

  for (int i = 0; i < dUlon->fieldLatLen; i++) {
    //lat = dUlon->lat[i]  ;
    sin2Lat = dUlon->sin2Lat[i];
    value = 3 * A * sin2Lat;
    for (int j = 0; j < dUlon->fieldLonLen; j++) {
      //lon = dUlon->lon[j];
      dUlonArray[i][j] = value*(sinMinusB[j] + sinPlusB[j]); //P21
    }
  }
}

inline void Solver::UpdateFullPotential(void) {
  double lon = 0;
  double B = consts->angVel.Value()*simulationTime;
  double A = 0.25 * pow(consts->angVel.Value(),2)*pow(consts->radius.Value(),2);
  double sin2Lat = 0;
  double cos2Lat = 0;
  double cosLat = 0;

  for (int i = 0; i < dUlat->fieldLatLen; i++) {
    sin2Lat = dUlat->sin2Lat[i];
    cos2Lat = dUlat->cos2Lat[i];
    for (int j = 0; j < dUlat->fieldLonLen; j++) {
      lon = dUlat->lon[j];
      dUlatArray[i][j] = consts->theta.Value()*-6 * cos2Lat*(cos(lon - B) + cos(lon + B));
      dUlatArray[i][j] += consts->e.Value()*(-9.*sin2Lat*cos(B));
      dUlatArray[i][j] += consts->e.Value()*(-1.5*sin2Lat * (7 * cos(2 * lon - B) - cos(2 * lon + B)));
      dUlatArray[i][j] *= A;
    }
  }

  for (int i = 0; i < dUlon->fieldLatLen; i++) {
    sin2Lat = dUlon->sin2Lat[i];
    cosLat = dUlon->cosLat[i];
    for (int j = 0; j < dUlon->fieldLonLen; j++) {
      lon = dUlon->lon[j];
      dUlonArray[i][j] = consts->theta.Value() * 3 * sin2Lat*(sin(lon - B) + sin(lon + B));
      dUlonArray[i][j] += consts->e.Value() * 3 * cosLat*cosLat*(-7 * sin(2 * lon - B) + sin(2 * lon + B));
      dUlonArray[i][j] *= A;
    }
  }
}

inline void Solver::UpdateTotalPotential(void) {
  // Total time-dependent and static parts of the tidal potential, accurate to second order
  // in both eccentricity and obliquity
  double factor;
  // double factor2;
  double cosLat = 0;
  double cos2Lat = 0;
  double sin2Lat = 0;
  double sinLon = 0;
  double cosLon = 0;
  double sin2Lon = 0;
  double cos2Lon = 0;
  double lon = 0;
  double t_omega = consts->angVel.Value()*simulationTime;
  double e = consts->e.Value();
  double theta = consts->theta.Value();

  double cos_t_omega = cos(t_omega);
  double sin_t_omega = sin(t_omega);
  double cos_2t_omega = cos(2*t_omega);
  double cos_3t_omega = cos(3*t_omega);
  double cos_4t_omega = cos(4*t_omega);

  double A = 0;
  double B = 0;
  double C = 0;
  double D = 0;
  double E = 0;
  double F = 0;
  double G = 0;

  factor = 0.03125 * (consts->angVel.Value() * consts->angVel.Value() * consts->radius.Value() * consts->radius.Value());
  // factor2 = -0.5 * (consts->angVel.Value() * consts->angVel.Value() * consts->radius.Value() * consts->radius.Value());

  A = - (4 + 15 * e*e + 20. * e * cos_t_omega + 43 * e*e * cos_2t_omega);
  B = 2 * e * (4 + 25 * e * cos_t_omega) * sin_t_omega;
  C = - 2 * theta*theta * (2 + 3*e*e + 6*e*cos_t_omega + 9 * e * e *cos_2t_omega);
  D = -2 * (2 - 5*e*e + 6*e*cos_t_omega + 17 * e*e*cos_2t_omega);
  E = 4 * e * (4 + 17*e*cos_t_omega)*sin_t_omega;
  F = -(-2+theta*theta);


  for (int i = 0; i < dUlon->fieldLatLen; i++) {
    cosLat = dUlon->cosLat[i];
    sin2Lat = dUlon->sin2Lat[i];
    for (int j = 0; j < dUlon->fieldLonLen; j++) {
      sinLon = dUlon->sinLon[j];
      cosLon = dUlon->cosLon[j];
      sin2Lon = dUlon->sin2Lon[j];
      cos2Lon = dUlon->cos2Lon[j];
      lon = dUlon->lon[j];
      dUlonArray[i][j] = 12 * theta * sin2Lat * sin_t_omega * (A * sinLon + B * cosLon);
      dUlonArray[i][j] += 6*cosLat*cosLat * (C * sin(2 * (t_omega + lon)) + F * (D * sin2Lon + E *cos2Lon));
      dUlonArray[i][j] *= factor;

      //Static part
      // dUlonArray[i][j] -= factor2 * (3*cosLat*cosLat*sin2Lon);
    }
  }

  G = - (2 + 3 * e*e)*(-2 + 3*theta*theta) + 3*e*(4-7*theta*theta)*cos_t_omega;
  G += 6*(theta*theta + e*e*(3 - 7*theta*theta))*cos_2t_omega;
  G += 3*e*theta*theta * (7*cos_3t_omega + 17*e*cos_4t_omega);
  G *= -6.0;

  C *= -0.5;
  D *= -0.5;
  E *= 0.5;
  // F *= 0.5;


  for (int i = 0; i < dUlat->fieldLatLen; i++) {
    cosLat = dUlat->cosLat[i];
    sin2Lat = dUlat->sin2Lat[i];
    for (int j = 0; j < dUlat->fieldLonLen; j++) {
      sinLon = dUlat->sinLon[j];
      cosLon = dUlat->cosLon[j];
      sin2Lon = dUlat->sin2Lon[j];
      cos2Lon = dUlat->cos2Lon[j];
      lon = dUlat->lon[j];
      dUlatArray[i][j] = G * sin2Lat;
      dUlatArray[i][j] += 24 * theta * cos2Lat * sin_t_omega * (-A * cosLon + B * sinLon);
      dUlatArray[i][j] += -6*sin2Lat * (C * cos(2 * (t_omega + lon)) + F * (D * cos2Lon + E *sin2Lon));
      dUlatArray[i][j] *= factor;

      //Static part
      // dUlatArray[i][j] -= factor2 * (3*sin2Lat*cosLon*cosLon);
    }
  }
}

inline void Solver::InterpPole(Field * field) {
  for (int j = 0; j < field->fieldLonLen; j++) {
    // field->solution[0][j] = linearInterp1(field, 0, j);
    // field->solution[field->fieldLatLen - 1][j] = linearInterp1(field, field->fieldLatLen - 1, j);
    field->solution[0][j] = lagrangeInterp3(field, 0, j);
    field->solution[field->fieldLatLen - 1][j] = lagrangeInterp3(field, field->fieldLatLen - 1, j);
  }

};

void Solver::UpdateEastVel(){
  double coriolis = 0;
  double tidalForce = 0;

  double dSurfLon = 0;
  double surfHeight = 0;
  double eastEta = 0;
  double westEta = 0;

  double oceanLoadingEast = 0.0;
  double oceanLoadingWest = 0.0;
  double oceanLoadingTerm = 0;
  // double legendre = 0.0;
  // double loadingFactorEast = 0.0;
  // double loadingFactorWest = 0.0;
  // double gammaFactor = 0.0;
  // double loadK = 0.0;
  // double loadH = 0.0;
  // double mu_bar = 0.0;


  double alpha = consts->alpha.Value();
  double angVel = consts->angVel.Value();

  for (int i = 1; i < uLatLen - 1; i++) {
    for (int j = 0; j < uLonLen; j++) {
      if (j < vLonLen - 1) {
        vNEAvgArray[i][j] = 0.25*(vOldArray[i][j] + vOldArray[i - 1][j] + vOldArray[i][j + 1] + vOldArray[i - 1][j + 1]);
      }
      else {
        vNEAvgArray[i][j] = 0.25*(vOldArray[i][j] + vOldArray[i - 1][j] + vOldArray[i][0] + vOldArray[i - 1][0]);
      }
    }
  }


  switch (consts->fric_type) {
  case LINEAR:
    for (int i = 1; i < uLatLen - 1; i++) {
      for (int j = 0; j < uLonLen; j++) {
        uDissArray[i][j] = uOldArray[i][j] * alpha;
      }
    }
    break;

  case QUADRATIC:
    double alphah = 0.0;
    int i_h = 0;
    int j_h = 0;

    for (int i = 1; i < uLatLen - 1; i++) {
      i_h = i*2;
      for (int j = 0; j < uLonLen; j++) {
        j_h = j*2;
        alphah = alpha / (depthArray[i_h][j_h+1]+ etaUAvgArray[i][j]);
        // alphah = alpha / (depthArray[i_h][j_h+1]);
        uDissArray[i][j] = alphah * uOldArray[i][j] * sqrt(uOldArray[i][j]*uOldArray[i][j] + vNEAvgArray[i][j]*vNEAvgArray[i][j]);
      }
    }
    break;
  }

  double loveRadius = consts->loveReduct.Value() / consts->radius.Value();
  double gRadius = consts->g.Value() / consts->radius.Value();
  double coriolisFactor = 0;
  double tidalFactor = 0;
  double surfFactor = 0;

  double * uCosLat = uOld->cosLat;
  double * uSinLat = uOld->sinLat;

  double g = consts->g.Value();
  double r = consts->radius.Value();
  //
  // double oceanLoadingEastTotal = 0.0;
  // double oceanLoadingWestTotal = 0.0;


  int i_h = 0;
  int j_h = 0;

  for (int i = 1; i < uLatLen - 1; i++) {
    i_h = i*2;

    coriolisFactor =  2. * angVel*uSinLat[i];
    tidalFactor = loveRadius/uCosLat[i];
    surfFactor = gRadius/uCosLat[i];
    for (int j = 0; j < uLonLen; j++) {
      j_h = j*2;
      if (j != uLonLen - 1) eastEta = depthArray[i_h][j_h + 2] + etaOldArray[i][j+1];
      else eastEta = depthArray[i_h][0] + etaOldArray[i][0];

      westEta = depthArray[i_h][j_h] + etaOldArray[i][j];

      dSurfLon = (eastEta - westEta) / (etadLon);

      coriolis = coriolisFactor * vNEAvgArray[i][j];
      tidalForce = tidalFactor * dUlonArray[i][j];


      if (!loading) {
        surfHeight = surfFactor*dSurfLon;
        oceanLoadingTerm = 0.0;
      }
      else {
        if (j != uLonLen - 1) oceanLoadingEast = oceanLoadingArray[i][j+1];
        else oceanLoadingEast = oceanLoadingArray[i][0];

        oceanLoadingWest = oceanLoadingArray[i][j];

        oceanLoadingTerm = surfFactor*(oceanLoadingEast - oceanLoadingWest)/etadLon;

        surfHeight = 0.0;
      }


      uNewArray[i][j] = (coriolis - surfHeight - oceanLoadingTerm + tidalForce - uDissArray[i][j])*dt + uOldArray[i][j];
    }
  }

  for (int j = 0; j < uLonLen; j++) {
    uNewArray[0][j] = linearInterp1Array(u,uNewArray, 0, j);
    uNewArray[uLatLen - 1][j] = linearInterp1Array(u,uNewArray, uLatLen - 1, j);
    // uNewArray[0][j] = lagrangeInterp3ArrayCenter(u,uNewArray, 0, j);
    // uNewArray[uLatLen - 1][j] = lagrangeInterp3ArrayCenter(u,uNewArray, uLatLen - 1, j);
    //uNewArray[0][j] = 0.0;//uNewArray[1][j];
          //uNewArray[uLatLen - 1][j] = 0.0;//uNewArray[uLatLen - 2][j];
  }
  //if (tide!=OBLIQ) {
  double npoleSum = 0;
  double spoleSum = 0;
  for (int j = 0; j < uLonLen; j++) {
    npoleSum += uNewArray[0][j];
    spoleSum += uNewArray[uLatLen - 1][j];
  }
  npoleSum = npoleSum / uLonLen;
  spoleSum = spoleSum / uLonLen;

  for (int j = 0; j < uLonLen; j++) {
    uNewArray[0][j] = npoleSum;
    uNewArray[uLatLen - 1][j] = spoleSum;
  }

}

int Solver::UpdateNorthVel(){
  double coriolis = 0;
  double tidalForce = 0;
  double dSurfLat = 0;
  double surfHeight = 0;

  double northEta = 0;
  double southEta = 0;

  // double oceanLoadingNorth = 0.0;
  // double oceanLoadingSouth = 0.0;
  // double oceanLoadingNorthTotal = 0.0;
  // double oceanLoadingSouthTotal = 0.0;
  double oceanLoadingTerm = 0;
  // double loadingFactor = 0.0;
  // double legendreNorth = 0.0;
  // double legendreSouth = 0.0;
  // double gammaFactor = 0.0;
  // double loadK = 0.0;
  // double loadH = 0.0;
  // double mu_bar = 6.78e9/(1610.0*consts->g.Value()*consts->radius.Value());
  double loveRadius = consts->loveReduct.Value() / consts->radius.Value();
  double gRadius = consts->g.Value() / consts->radius.Value();
  double coriolisFactor = 0;

  double g = consts->g.Value();
  double r = consts->radius.Value();

  double alpha = consts->alpha.Value();
  double angVel = consts->angVel.Value();

  for (int i = 0; i < vLatLen; i++) {
    for (int j = 0; j < vLonLen; j++) {
      if (j > 0) {
        uSWAvgArray[i][j] = 0.25*(uOldArray[i][j] + uOldArray[i + 1][j] + uOldArray[i][j - 1] + uOldArray[i + 1][j - 1]);
      }
      else {
        uSWAvgArray[i][j] = 0.25*(uOldArray[i][j] + uOldArray[i + 1][j] + uOldArray[i][uLonLen - 1] + uOldArray[i + 1][uLonLen - 1]);
      }
    }

  }

  switch (consts->fric_type) {
  case LINEAR:
    for (int i = 0; i < vLatLen; i++) {
      for (int j = 0; j < vLonLen; j++) {
        vDissArray[i][j] = vOldArray[i][j] * alpha;
      }
    }
    break;

  case QUADRATIC:
    double alphah = 0.0;
    int i_h = 0;
    int j_h = 0;
    for (int i = 0; i < vLatLen; i++) {
      i_h = i*2;
      for (int j = 0; j < vLonLen; j++) {
        j_h = j*2;
        alphah = alpha / (depthArray[i_h+1][j_h] + etaVAvgArray[i][j]);
        // alphah = alpha / (depthArray[i_h+1][j_h]);
        vDissArray[i][j] = alphah * vOldArray[i][j] * sqrt(vOldArray[i][j] * vOldArray[i][j] + uSWAvgArray[i][j]*uSWAvgArray[i][j]);
      }
    }
    break;
  }


  int i_h = 0;
  int j_h = 0;

  for (int i = 0; i < vLatLen; i++) {
    coriolisFactor = 2. * angVel * v->sinLat[i];
    i_h = i*2;

    for (int j = 0; j < vLonLen; j++) {
      j_h = j*2;

      northEta = depthArray[i_h][j_h] + etaOldArray[i][j];
      southEta = depthArray[i_h+2][j_h] + etaOldArray[i+1][j];

      dSurfLat = (northEta - southEta) / etadLat;

      if (!loading) {
        surfHeight = gRadius*dSurfLat;
        oceanLoadingTerm = 0.0;
      }
      else {
        oceanLoadingTerm = gRadius*(oceanLoadingArray[i][j] - oceanLoadingArray[i+1][j])/etadLat;
        surfHeight = gRadius*dSurfLat;

        std::cout << surfHeight <<'\t'<<oceanLoadingTerm << std::endl;
        surfHeight = 0.0;
      }

      coriolis =  coriolisFactor*uSWAvgArray[i][j];

      tidalForce = loveRadius * dUlatArray[i][j];


      // std::cout << oceanLoadingTerm<<'\t'<<tidalForce<< << std::endl;

      vNewArray[i][j] = (-coriolis - surfHeight - oceanLoadingTerm + tidalForce - vDissArray[i][j])*dt + vOldArray[i][j];


    }
  }

}

void Solver::UpdateSurfaceHeight(){
  double vGrad = 0;
  double uGrad = 0;
  double northv = 0;
  double southv = 0;
  double eastu = 0;
  double westu = 0;

  double cosLat;
  double vdLat = v->dLat;
  double vdLon = v->dLon;
  // double interpLonLen = etaInterp->fieldLonLen;

  //
  // for (int i = 0; i < vLatLen; i++) {
  //   for (int j = 0; j < vLonLen; j++) {
  //     if (j > 0) {
  //       etaVAvgArray[i][j] = 0.25*(etaOldArray[i][j] + etaOldArray[i + 1][j] + etaInterpArray[i][j] + etaInterpArray[i][j - 1]);
  //     }
  //     else {
  //       etaVAvgArray[i][j] = 0.25*(etaOldArray[i][j] + etaOldArray[i + 1][j] + etaInterpArray[i][j] + etaInterpArray[i][vLonLen - 1]);
  //     }
  //   }
  // }
  //
  // for (int i = 1; i < uLatLen - 1; i++) {
  //   for (int j = 0; j < uLonLen; j++) {
  //     if (j < uLonLen - 1) {
  //       etaUAvgArray[i][j] = 0.25*(etaOldArray[i][j] + etaOldArray[i][j + 1] + etaInterpArray[i - 1][j] + etaInterpArray[i][j]);
  //     }
  //     else {
  //       etaUAvgArray[i][j] = 0.25*(etaOldArray[i][j] + etaOldArray[i][0] + etaInterpArray[i - 1][j] + etaInterpArray[i][j]);
  //     }
  //   }
  // }



  double r = consts->radius.Value();
  double hRadius = 0.0;

  int i_h = 0;
  int j_h = 0;

  for (int i = 1; i < etaLatLen-1; i++) {
    cosLat = eta->cosLat[i];
    i_h = i*2;
    for (int j = 0; j < etaLonLen; j++) {
      j_h = j*2;
      // northv = (etaVAvgArray[i - 1][j] + depthArray[i_h - 1][j_h]) * vNewArray[i - 1][j] * v->cosLat[i - 1];
      // southv = (etaVAvgArray[i][j] + depthArray[i_h + 1][j_h]) * vNewArray[i][j] * v->cosLat[i];
      northv = ( depthArray[i_h - 1][j_h]) * vNewArray[i - 1][j] * v->cosLat[i - 1];
      southv = ( depthArray[i_h + 1][j_h]) * vNewArray[i][j] * v->cosLat[i];
      // northv =  vNewArray[i - 1][j] * v->cosLat[i - 1];
      // southv =  vNewArray[i][j] * v->cosLat[i];

      vGrad = (northv - southv) / vdLat;


      if (j > 0) {
        // eastu = (etaUAvgArray[i][j] + depthArray[i_h][j_h + 1]) * uNewArray[i][j];
        // westu = (etaUAvgArray[i][j - 1] + depthArray[i_h][j_h - 1]) * uNewArray[i][j - 1];
        eastu = (depthArray[i_h][j_h + 1]) * uNewArray[i][j];
        westu = (depthArray[i_h][j_h - 1]) * uNewArray[i][j - 1];
        // eastu = uNewArray[i][j];
        // westu = uNewArray[i][j - 1];
      }
      else {
        // westu = (etaUAvgArray[i][uLonLen - 1] + depthArray[i_h][uLonLen*2 - 1]) * uNewArray[i][uLonLen - 1];
        // eastu = (etaUAvgArray[i][j] + depthArray[i_h][j_h + 1]) * uNewArray[i][j];
        eastu = (depthArray[i_h][j_h + 1]) * uNewArray[i][j];
        westu = (depthArray[i_h][uLonLen*2 - 1]) * uNewArray[i][uLonLen - 1];
        // eastu = uNewArray[i][j];
        // westu = uNewArray[i][uLonLen - 1];
      }

      uGrad = (eastu - westu) / vdLon;

      hRadius = 1.0 / r;

      // etaNewArray[i][j] = hRadius/cosLat*(-vGrad - uGrad)*dt + etaOldArray[i][j];

      etaNewArray[i][j] = hRadius/cosLat*(-vGrad - uGrad)*dt + etaOldArray[i][j];
    }
  }

  for (int j = 0; j < etaLonLen; j++) {
     //etaNewArray[0][j] = linearInterp1Array(eta,etaNewArray, 0, j);
     //etaNewArray[etaLatLen - 1][j] = linearInterp1Array(eta,etaNewArray, etaLatLen - 1, j);
    etaNewArray[0][j] = lagrangeInterp3ArrayCenter(eta,etaNewArray, 0, j);
    etaNewArray[etaLatLen - 1][j] = lagrangeInterp3ArrayCenter(eta,etaNewArray, etaLatLen - 1, j);
  }

  //Average eta out at poles
  double npoleSum = 0;
  double spoleSum = 0;
  for (int j = 0; j < etaLonLen; j++) {
    npoleSum += etaNewArray[0][j];
    spoleSum += etaNewArray[etaLatLen - 1][j];
  }
  npoleSum = npoleSum / etaLonLen;
  spoleSum = spoleSum / etaLonLen;

  for (int j = 0; j < etaLonLen; j++) {
    etaNewArray[0][j] = npoleSum;
    etaNewArray[etaLatLen - 1][j] = spoleSum;
  }
}


inline void Solver::InterpSurfaceHeight() {
  int interpLatLen = etaInterp->ReturnFieldLatLen();
  int interpLonLen = etaInterp->ReturnFieldLonLen();

  for (int i = 0; i < interpLatLen; i++) {
    for (int j = 0; j < interpLonLen; j++) {
      if (j < interpLonLen - 1) {
        etaInterpArray[i][j] = 0.25*(etaOldArray[i][j] + etaOldArray[i][j + 1] + etaOldArray[i + 1][j] + etaOldArray[i + 1][j + 1]);
      }
      else {
        etaInterpArray[i][j] = 0.25*(etaOldArray[i][j] + etaOldArray[i][0] + etaOldArray[i + 1][j] + etaOldArray[i + 1][0]);
      }
    }

  }
}

int Solver::ExtractSHCoeff(void) {
  double * fort_array;
  double * fort_harm_coeff;

  int coeff_num = 2*(l_max + 1)*(l_max + 1);

  int i_len = etaOld->ReturnFieldLatLen() - 1; //minus 1 required as SHTOOLS requires even samples in latitude
  int j_len = etaOld->ReturnFieldLonLen();

  int n = i_len*j_len;

  int i,j,k,l,m;

  fort_array = new double[n];
  fort_harm_coeff = new double[coeff_num];

  int count = 0;
  for (j = 0; j<j_len; j++) {
    for (i = 0; i<i_len; i++) {
      fort_array[count] = etaNewArray[i][j];
      if (etaNewArray[i][j] != etaNewArray[i][j]) {
        std::cout << "NAN" << std::endl;
        flag = true;
        return 0;
      }
      count++;
    }
  }

  extractshcoeff_(fort_array, &i_len, &l_max, fort_harm_coeff);

  count = 0;
  for (j=0; j<l_max+1; j++) {
    for (k=0; k<l_max+1; k++) {
      SH_cos_coeff[k][j] = fort_harm_coeff[count];
      SH_sin_coeff[k][j] = fort_harm_coeff[count+1];
      count+=2;
    }
  }

  count = 0;
  for (l=2; l<l_max+1; l++) {
    for (m=0; m<=l; m++) {
      shPower[l][m] = SH_cos_coeff[l][m]*SH_cos_coeff[l][m] + SH_sin_coeff[l][m]*SH_sin_coeff[l][m];
      if (shPower[l][m] > 0) count++;
    }
  }


  for (i=0; i<l_solve_len; i++) {
    delete[] lm_solve[i];
  }
  delete[] lm_solve;

  l_solve_len = count;
  lm_solve = new int*[l_solve_len];
  for (i=0; i<l_solve_len; i++) {
    lm_solve[i] = new int[2];
  }

  count = 0;
  for (l=2; l<l_max+1; l++) {
    for (m=0; m<=l; m++) {
      if (shPower[l][m] > 0) {
        lm_solve[count][0] = l;
        lm_solve[count][1] = m;
        count++;
      }
    }
  }

  delete[] fort_array;
  delete[] fort_harm_coeff;

}

int Solver::UpdateLoading(void) {
  double loading = 0.0;
  double loadingTotal = 0.0;
  double loadingFactor = 0.0;
  double omega = consts->angVel.Value();
  double omegaTime = omega*simulationTime;
  double lon;
  int i,j,l,m,k, degree;

  ExtractSHCoeff();

  for (j=0; j<etaLonLen; j++) {
    lon = eta->lon[j];
    for (m=0; m<l_max+1; m++){
      etaCosMLon[j][m] = cos(m*lon - omegaTime);
      etaSinMLon[j][m] = sin(m*lon - omegaTime);
    }
  }

  for (i = 0; i < etaLatLen; i++) {
    for (j = 0; j < etaLonLen; j++) {

      loadingTotal = 0.0;

      for (int k=0; k<l_solve_len; k++){
        degree = lm_solve[k][0];

        loading = 0.0;

        while (lm_solve[k][0] == degree){
          l = lm_solve[k][0];
          m = lm_solve[k][1];

          loadingFactor = etaCosMLon[j][m]*SH_cos_coeff[l][m] + etaSinMLon[j][m]*SH_sin_coeff[l][m];

          loading += loadingFactor*etaLegendreArray[i][l][m];

          k++;

          if (k >= l_solve_len) break;

        }

        loading *= gammaFactor[l];

        loadingTotal += loading;

        k--;
      }

      oceanLoadingArray[i][j] = loadingTotal;

    }
  }

  return 1;

}



int Solver::Explicit() {

  int outCount = 0;
  double timeStepCount = 0;
  int inc = (int) (consts->period.Value()/dt);

  InitialConditions();

  //Check for stability
  outstring << "Entering time loop:\n\n";
  outstring << "\t\t End time: \t" << consts->endTime.Value() / 86400.0 << " days\n";
  outstring << "\t\t Time step: \t" << dt << " seconds\n\n";
  Out->Write(OUT_MESSAGE, &outstring);


  energy->mass->UpdateMass();

  //Update cell energies and globally averaged energy
  energy->UpdateKinE(uNewArray,vNewArray);

  while (simulationTime <= consts->endTime.Value() && !energy->converged) {

    if (flag) {
      return -1;
    }

    timeStepCount+=dt;
    outputCount+=dt;

    UpdatePotential();


    if (!loading) {
      if (simulationTime > 0.1*consts->endTime.Value()) {
        // printf("Kicking in ocean loading\n");
        loading = true;
        // }
      }
    }

    if (loading) UpdateLoading();

    loading = false;

    simulationTime += dt;

    //Call SHTOOLS to find spherical harmonic expansion of etaNew.
    // if (loading) ExtractSHCoeff();
    // ExtractSHCoeff();

    //Solve for v
    UpdateNorthVel();

    //solve for u
    UpdateEastVel();

    //Solve for eta based on new u and v
    UpdateSurfaceHeight();



    for (int i = 0; i < vLatLen; i++) {
      for (int j = 0; j < vLonLen; j++) {
        vOldArray[i][j] = vNewArray[i][j];
      }
    }

    for (int i = 0; i < uLatLen; i++) {
      for (int j = 0; j < uLonLen; j++) {
        uOldArray[i][j] = uNewArray[i][j];
      }
    }

    for (int i = 0; i < etaLatLen; i++) {
      for (int j = 0; j < etaLonLen; j++) {
        etaOldArray[i][j] = etaNewArray[i][j];
      }
    }

    // InterpSurfaceHeight();

    // energy->mass->UpdateMass();

    energy->UpdateKinE(uNewArray,vNewArray);

    energy->UpdateDtKinEAvg();

    //Check for output
    if (timeStepCount >= consts->period.Value()) {
      orbitNumber++;
      timeStepCount -= consts->period.Value();

      printf("TIME: %.2f, number: %d, dissipation: %.3f\n", simulationTime/consts->period.Value(), output, energy->currentDissEAvg);

      energy->UpdateOrbitalKinEAvg(inc);

      // Check for convergence
      // energy->IsConverged();
      if (energy->converged) convergeCount++;

      outstring << std::fixed << std::setprecision(2) << simulationTime / 86400.0 << " days: \t" << 100 * (simulationTime / consts->endTime.Value()) << "%\t" << output;
      Out->Write(OUT_MESSAGE, &outstring);

      output++;
      outCount++;

      // DumpSolutions(-2, timeStepCount);
      outCount = 1;

      energy->timePos = 0; //Reset time position after energy data output

      // printf("%.5f\n",simulationTime);
      // std::cout << energy->converged << std::endl;

      DumpFields(output);
    }
    else if (timeStepCount >= consts->period.Value()*consts->outputTime.Value()*outCount) {
      output++;
      // printf("TIME: %f\n", simulationTime/consts->period.Value());
      outCount++;

      // DumpSolutions(1, timeStepCount);
      DumpFields(output);

    }
    iteration++;

  }

  // deallocate memory assigned to temporary fields
  delete etaNew;
  delete uNew;
  delete vNew;

  outstring << "\nSimulation complete.\n\nEnd time: \t\t" << simulationTime / 86400.0 << "\n";
  outstring << "Total iterations: \t" << iteration;
  Out->Write(OUT_MESSAGE, &outstring);

  return 1;

};
//
// void Solver::CatchExit(int sig, int output) {
//
// };

void Solver::DumpSolutions(int out_num, double time) {

  if (out_num == -1) {

    //Only for windows

    //remove(&(consts->path + SEP + "diss.txt")[0]);

    std::ofstream uLat(consts->path + SEP + "Grid" + SEP + "u_lat.txt", std::ofstream::out);
    std::ofstream uLon(consts->path + SEP + "Grid" + SEP + "u_lon.txt", std::ofstream::out);
    std::ofstream vLat(consts->path + SEP + "Grid" + SEP + "v_lat.txt", std::ofstream::out);
    std::ofstream vLon(consts->path + SEP + "Grid" + SEP + "v_lon.txt", std::ofstream::out);

    for (int j = 0; j < u->ReturnFieldLonLen(); j++) uLon << u->lon[j] * 1 / radConv << '\t';
    for (int i = 0; i < u->ReturnFieldLatLen(); i++) uLat << u->lat[i] * 1 / radConv << '\t';

    for (int j = 0; j < v->ReturnFieldLonLen(); j++) vLon << v->lon[j] * 1 / radConv << '\t';
    for (int i = 0; i < v->ReturnFieldLatLen(); i++) vLat << v->lat[i] * 1 / radConv << '\t';
  }
  else if (out_num == -2) {

  }
  else {
    if (energy->converged) DumpFields(out_num);
    // else if (out_num % 5 == 0) DumpFields(out_num); //dump every 5 orbits

    // DumpFields(out_num); //FOR DEBUGGING
  }
};

void Solver::ReadInitialConditions(void) {
  // hid_t init_file = H5Fopen("InitialConditions/init.h5", H5F_ACC_RDONLY, H5P_DEFAULT);
  //
  // hid_t displacement_h5 = H5Dopen(init_file, "displacement", H5P_DEFAULT);
  // hid_t northVel_h5 = H5Dopen(init_file, "north velocity", H5P_DEFAULT);
  // hid_t eastVel_h5 = H5Dopen(init_file, "east velocity", H5P_DEFAULT);
  //
  // H5Dread(displacement_h5, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, eta_1D);
  // H5Dread(northVel_h5, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, v_1D);
  // H5Dread(eastVel_h5, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, u_1D);

  for (int i=0; i< etaLatLen; i++) {
    for (int j=0; j<etaLonLen; j++) {
        etaOldArray[i][j] = 0.0;//eta_1D[i*etaLonLen + j];
        // oceanLoadingArray[i][j] = 0.0;
    }
  }

  for (int i=0; i< vLatLen; i++) {
    for (int j=0; j<vLonLen; j++) {
        vOldArray[i][j] = 0.0;//v_1D[i*vLonLen + j];
    }
  }

  for (int i=0; i< uLatLen; i++) {
    for (int j=0; j<uLonLen; j++) {
        uOldArray[i][j] = 0.0;//u_1D[i*uLonLen + j];
    }
  }

  double l_thin = -10.0 * radConv;
  double h_thick = consts->h.Value();
  double h_thin = 500.0;


  for (int i = 0; i < depth->ReturnFieldLatLen(); i++) {
    for (int j = 0; j < depth->ReturnFieldLonLen(); j++) {
      l_thin = 0.0;
      if (depth->lat[i] <= l_thin) depthArray[i][j] = -(h_thick-h_thin)*0.5*(sin(2*depth->lat[i] + pi*0.5) - 1) + h_thin;
      else depthArray[i][j] = h_thin;

      depthArray[i][j] = h_thick;

      // depthArray[i][j] = 0.0;
    }
  }

};

void Solver::DumpFields(int output_num) {
  std::string out = std::to_string(output_num);

  // printf("Outputing data number %d\n", output_num);

  double factor = 0;

  switch (consts->fric_type) {
    case LINEAR:
      factor = consts->alpha.Value();
      break;

    case QUADRATIC:
      factor = consts->alpha.Value()/consts->h.Value();
      break;
  }


  //-----------------Unravel solutions into 1D arrays---------------------------

  for (int i=0; i<etaLatLen;i++) {
    for (int j=0; j<etaLonLen; j++) {
      eta_1D[i*etaLonLen + j] = (float)etaNewArray[i][j];
      if (loading) eta_1D[i*etaLonLen + j] = (float)oceanLoadingArray[i][j];
    }
  }

  for (int i=0; i<uLatLen;i++) {
    for (int j=0; j<uLonLen; j++) {
      u_1D[i*uLonLen + j] = (float)uNewArray[i][j];
    }
  }

  for (int i=0; i<vLatLen;i++) {
    for (int j=0; j<vLonLen; j++) {
      v_1D[i*vLonLen + j] = (float)vNewArray[i][j];
    }
  }

  for (int i=0; i<etaLatLen;i++) {
    for (int j=0; j<etaLonLen; j++) {
      diss_1D[i*etaLonLen + j] = (float)energy->solution[i][j]*2.0*factor;
    }
  }


  for (int i=0; i<l_max+1; i++)
  {
    for (int j=0; j<l_max+1; j++)
    {
      harm_coeff_1D[i*harm_cols + j] = (float)SH_cos_coeff[i][j];
    }
  }

  for (int i=0; i<l_max+1; i++)
  {
    for (int j=0; j<l_max+1; j++)
    {
      harm_coeff_1D[(l_max+1)*(l_max+1) + i*harm_cols + j] = (float)SH_sin_coeff[i][j];
    }
  }

  diss_avg_1D[0] = energy->currentDissEAvg;

  start[0] = output_num-1;
  start[1] = 0;
  start[2] = 0;

  count[0] = 1;

  start_1D[0] = output_num - 1;

  start_harm[0] = output_num - 1;

  // count 1D is already set to 1.

  //----------------------------------------------------------------------------
  //------------------------WRITE ARRAYS TO DATA FILE---------------------------
  //----------------------------------------------------------------------------


  // ----------------------- Write displacement field --------------------------

  if (consts->field_displacement_output.Value()) {
    count[1] = eta_rows;
    count[2] = eta_cols;

    H5Sselect_hyperslab(data_space_eta, H5S_SELECT_SET, start, NULL, count, NULL);

    H5Dwrite(data_set_eta, H5T_NATIVE_FLOAT, mem_space_eta, data_space_eta, H5P_DEFAULT, eta_1D);
  }


  // ----------------------- Write velocity fields -----------------------------

  if (consts->field_velocity_output.Value()) {

    // ----------------------- Write north velocity field ------------------------

    count[1] = v_rows;
    count[2] = v_cols;

    H5Sselect_hyperslab(data_space_v, H5S_SELECT_SET, start, NULL, count, NULL);

    H5Dwrite(data_set_v, H5T_NATIVE_FLOAT, mem_space_v, data_space_v, H5P_DEFAULT, v_1D);

    // ----------------------- Write east velocity field -------------------------

    count[1] = u_rows;
    count[2] = u_cols;

    H5Sselect_hyperslab(data_space_u, H5S_SELECT_SET, start, NULL, count, NULL);

    H5Dwrite(data_set_u, H5T_NATIVE_FLOAT, mem_space_u, data_space_u, H5P_DEFAULT, u_1D);
  }

  // ----------------------- Write dissipated energy field ---------------------
  if (consts->field_diss_output.Value()) {

    count[1] = eta_rows;
    count[2] = eta_cols;

    H5Sselect_hyperslab(data_space_diss, H5S_SELECT_SET, start, NULL, count, NULL);

    H5Dwrite(data_set_diss, H5T_NATIVE_FLOAT, mem_space_diss, data_space_diss, H5P_DEFAULT, diss_1D);
  }

  // ------------------- Write global average dissipation ----------------------
  if (consts->diss.Value()) {

    H5Sselect_hyperslab(data_space_1D_avg, H5S_SELECT_SET, start_1D, NULL, count_1D, NULL);

    H5Dwrite(data_set_1D_avg, H5T_NATIVE_FLOAT, mem_space_1D_avg, data_space_1D_avg, H5P_DEFAULT, diss_avg_1D);
  }

  // ----------------------- Write SH coefficients ------------------------
  if (consts->sh_coeff_output.Value()) {
    H5Sselect_hyperslab(data_space_harm_coeff, H5S_SELECT_SET, start_harm, NULL, count_harm, NULL);

    H5Dwrite(data_set_harm_coeff, H5T_NATIVE_FLOAT, mem_space_harm_coeff, data_space_harm_coeff, H5P_DEFAULT, harm_coeff_1D);
  }
};

void Solver::CreateHDF5FrameWork(void) {
  #if _WIN32
      mkdir(&(consts->path + SEP + "Grid" + SEP)[0], NULL);

      mkdir(&(consts->path +  SEP + "DATA" + SEP)[0], NULL);

  #elif __linux__
      mkdir(&(consts->path +  SEP + "Grid" + SEP)[0], S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);

      mkdir(&(consts->path +  SEP + "DATA" + SEP)[0], S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);

  #endif


    eta_1D = new float[etaLatLen*etaLonLen];
    v_1D = new float[vLatLen*vLonLen];
    u_1D = new float[uLatLen*uLonLen];
    diss_1D = new float[etaLatLen*etaLonLen];
    diss_avg_1D = new float[1];
    harm_coeff_1D = new float[2*(l_max+1)*(l_max+1)];

    eta_rows = etaLatLen;
    eta_cols = etaLonLen;
    v_rows = vLatLen;
    v_cols = vLonLen;
    u_rows = uLatLen;
    u_cols = uLonLen;
    harm_rows = l_max+1;
    harm_cols = l_max+1;

    time_slices = (consts->endTime.Value()/consts->period.Value())/consts->outputTime.Value() + 1;

    rank_field = 3;
    hsize_t rank_size = 1;
    rank_1D = 1;
    rank_harm = 4;

    char dataFile[] = "DATA/data.h5";

    start = new hsize_t[3];
    count = new hsize_t[3];

    start_1D = new hsize_t[1];
    count_1D = new hsize_t[1];

    start_harm = new hsize_t[4];
    count_harm = new hsize_t[4];

    // Create HDF5 file
    file = H5Fcreate(dataFile, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

    dims_eta = new hsize_t[3];
    dims_eta[0] = 1;
    dims_eta[1] = eta_rows;
    dims_eta[2] = eta_cols;

    dims_u = new hsize_t[3];
    dims_u[0] = 1;
    dims_u[1] = u_rows;
    dims_u[2] = u_cols;

    dims_v = new hsize_t[3];
    dims_v[0] = 1;
    dims_v[1] = v_rows;
    dims_v[2] = v_cols;

    dims_1D_avg = new hsize_t[1];
    dims_1D_avg[0] = 1;

    dims_harm_coeff = new hsize_t[4];
    dims_harm_coeff[0] = 1;
    dims_harm_coeff[1] = 2;
    dims_harm_coeff[2] = harm_rows;
    dims_harm_coeff[3] = harm_cols;

    max_dims_eta = new hsize_t[3];
    max_dims_eta[0] = time_slices;
    max_dims_eta[1] = eta_rows;
    max_dims_eta[2] = eta_cols;

    max_dims_u = new hsize_t[3];
    max_dims_u[0] = time_slices;
    max_dims_u[1] = u_rows;
    max_dims_u[2] = u_cols;

    max_dims_v = new hsize_t[3];
    max_dims_v[0] = time_slices;
    max_dims_v[1] = v_rows;
    max_dims_v[2] = v_cols;

    max_dims_1D_avg = new hsize_t[1];
    max_dims_1D_avg[0] = time_slices;

    max_dims_harm_coeff = new hsize_t[4];
    max_dims_harm_coeff[0] = time_slices;
    max_dims_harm_coeff[1] = 2;
    max_dims_harm_coeff[2] = harm_rows;
    max_dims_harm_coeff[3] = harm_cols;

    if (consts->field_displacement_output.Value()) {
      data_space_eta = H5Screate_simple(rank_field, max_dims_eta, NULL); // 3D data space
      data_set_eta = H5Dcreate(file, "displacement", H5T_NATIVE_FLOAT, data_space_eta, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      mem_space_eta = H5Screate_simple(rank_field, dims_eta, NULL);
    }

    if (consts->field_velocity_output.Value()) {
      data_space_u = H5Screate_simple(rank_field, max_dims_u, NULL);
      data_space_v = H5Screate_simple(rank_field, max_dims_v, NULL);
      data_set_u = H5Dcreate(file, "east velocity", H5T_NATIVE_FLOAT, data_space_u, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      data_set_v = H5Dcreate(file, "north velocity", H5T_NATIVE_FLOAT, data_space_v, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      mem_space_u = H5Screate_simple(rank_field, dims_u, NULL);
      mem_space_v = H5Screate_simple(rank_field, dims_v, NULL);
    }

    if (consts->field_diss_output.Value()) {
      data_space_diss = H5Screate_simple(rank_field, max_dims_eta, NULL);
      data_set_diss = H5Dcreate(file, "dissipated energy", H5T_NATIVE_FLOAT, data_space_diss, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      mem_space_diss = H5Screate_simple(rank_field, dims_eta, NULL);
    }

    if (consts->diss.Value()) {
      data_space_1D_avg = H5Screate_simple(rank_1D, max_dims_1D_avg, NULL);
      data_set_1D_avg = H5Dcreate(file, "dissipated energy avg", H5T_NATIVE_FLOAT, data_space_1D_avg, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      mem_space_1D_avg = H5Screate_simple(rank_1D, dims_1D_avg, NULL);
    }

    if (consts->sh_coeff_output.Value()) {
      data_space_harm_coeff = H5Screate_simple(rank_harm, max_dims_harm_coeff, NULL);
      data_set_harm_coeff = H5Dcreate(file, "sh coefficients", H5T_NATIVE_FLOAT, data_space_harm_coeff, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      mem_space_harm_coeff = H5Screate_simple(rank_harm, dims_harm_coeff, NULL);
    }

    start[0] = 0;
    start[1] = 0;
    start[2] = 0;

    count[0] = 1;
    count[1] = eta_rows;
    count[2] = eta_cols;

    start_1D[0] = 0;
    count_1D[0] = 1;

    start_harm[0] = 0;
    start_harm[1] = 0;
    start_harm[2] = 0;
    start_harm[3] = 0;

    count_harm[0] = 1;
    count_harm[1] = 2;
    count_harm[2] = harm_rows;
    count_harm[3] = harm_cols;

    //-----------------------Write dataset attributes---------------------------

    hsize_t dims_coord[1];
    dims_coord[0] = 2;

    hid_t attr_space;
    hid_t attr;

    int data_set_size[2];
    float data_set_dx[2];


    //---------------------------Displacement-----------------------------------
    if (consts->field_displacement_output.Value()) {
      attr_space = H5Screate_simple(rank_size, dims_coord, NULL);

      attr = H5Acreate(data_set_eta, "nlat; nlon", H5T_NATIVE_INT, attr_space, H5P_DEFAULT, H5P_DEFAULT);

      data_set_size[0] = etaLatLen;
      data_set_size[1] = etaLonLen;

      H5Awrite(attr, H5T_NATIVE_INT, data_set_size);

      attr = H5Acreate(data_set_eta, "dlat; dlon", H5T_NATIVE_FLOAT, attr_space, H5P_DEFAULT, H5P_DEFAULT);

      data_set_dx[0] = (float)etadLat*1.0/radConv;
      data_set_dx[1] = (float)etadLon*1.0/radConv;

      H5Awrite(attr, H5T_NATIVE_FLOAT, data_set_dx);
    }

    //---------------------------Velocity---------------------------------------
    if (consts->field_velocity_output.Value()) {
      attr = H5Acreate(data_set_v, "nlat; nlon", H5T_NATIVE_INT, attr_space, H5P_DEFAULT, H5P_DEFAULT);

      data_set_size[0] = vLatLen;
      data_set_size[1] = vLonLen;

      H5Awrite(attr, H5T_NATIVE_INT, data_set_size);

      attr = H5Acreate(data_set_v, "dlat; dlon", H5T_NATIVE_FLOAT, attr_space, H5P_DEFAULT, H5P_DEFAULT);

      data_set_dx[0] = (float)vdLat*1.0/radConv;
      data_set_dx[1] = (float)vdLon*1.0/radConv;

      H5Awrite(attr, H5T_NATIVE_FLOAT, data_set_dx);

      attr = H5Acreate(data_set_u, "nlat; nlon", H5T_NATIVE_INT, attr_space, H5P_DEFAULT, H5P_DEFAULT);

      data_set_size[0] = uLatLen;
      data_set_size[1] = uLonLen;

      H5Awrite(attr, H5T_NATIVE_INT, data_set_size);

      attr = H5Acreate(data_set_u, "dlat; dlon", H5T_NATIVE_FLOAT, attr_space, H5P_DEFAULT, H5P_DEFAULT);

      data_set_dx[0] = (float)udLat*1.0/radConv;
      data_set_dx[1] = (float)udLon*1.0/radConv;

      H5Awrite(attr, H5T_NATIVE_FLOAT, data_set_dx);
    }

    //-------------------------Dissipated energy----------------------------------
    if (consts->field_diss_output.Value()) {
      attr = H5Acreate(data_set_diss, "nlat; nlon", H5T_NATIVE_INT, attr_space, H5P_DEFAULT, H5P_DEFAULT);

      data_set_size[0] = etaLatLen;
      data_set_size[1] = etaLonLen;

      H5Awrite(attr, H5T_NATIVE_INT, data_set_size);

      attr = H5Acreate(data_set_diss, "dlat; dlon", H5T_NATIVE_FLOAT, attr_space, H5P_DEFAULT, H5P_DEFAULT);

      data_set_dx[0] = (float)etadLat*1.0/radConv;
      data_set_dx[1] = (float)etadLon*1.0/radConv;

      H5Awrite(attr, H5T_NATIVE_FLOAT, data_set_dx);
    }

}
