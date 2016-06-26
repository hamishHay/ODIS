#include "vector"
#include "field.h"
#include "mesh.h"
#include "mathRoutines.h"
#include <math.h>
#include <iostream>

using namespace std;

Field::Field(Mesh *mesh, int latStagg, int lonStagg) {
	grid = mesh;

	dLat = grid->dLat*2;
	dLon = grid->dLon*2;

	fieldLonLen = grid->ReturnLonLen() / 2;

	if (latStagg) fieldLatLen = (grid->ReturnLatLen() - 1 )/ 2;
	else fieldLatLen = 1 + (grid->ReturnLatLen() - 1)/ 2;
	//if (lonStagg) fieldLonLen--;


	solution = new double*[fieldLatLen];
	for (int i = 0; i < fieldLatLen; i++) {
		solution[i] = new double[fieldLonLen];
		for (int j = 0; j < fieldLonLen; j++) {
			solution[i][j] = 0; //Initial Guess
		}
	}

	lat = new double[fieldLatLen];
	for (int i = 0; i < fieldLatLen; i++) {
		if (latStagg) lat[i] = grid->lat[i * 2 + 1];
		else lat[i] = grid->lat[i * 2];
	}

	// Make sure last value is 90 exactly
	if (!latStagg) lat[fieldLatLen-1] = -90.0;

	lon = new double[fieldLonLen];
	for (int j = 0; j < fieldLonLen; j++) {
		if (lonStagg) lon[j] = grid->lon[j * 2 + 1];
		else lon[j] = grid->lon[j * 2];
	}

	//populate opp pointer
	//fieldLonLen must be an even number
	opp.resize(fieldLonLen);
	for (int i = 0; i < fieldLonLen; i++) {
		if (i < fieldLonLen / 2) opp[i] = fieldLonLen / 2 + i;
		else opp[i] = i - fieldLonLen / 2;
	}

	cosLat = new double[fieldLatLen];
	sinLat = new double[fieldLatLen];
	cos2Lat = new double[fieldLatLen];
	sin2Lat = new double[fieldLatLen];
	for (int i = 0; i < fieldLatLen; i++) {
		lat[i] *= radConv;
		cosLat[i] = cos(lat[i]);
		sinLat[i] = sin(lat[i]);
		cos2Lat[i] = cos(2*lat[i]);
		sin2Lat[i] = sin(2*lat[i]);
	}
	cosLon = new double[fieldLonLen];
	sinLon = new double[fieldLonLen];
	cos2Lon = new double[fieldLonLen];
	sin2Lon = new double[fieldLonLen];
	for (int j = 0; j < fieldLonLen; j++) {
		lon[j] *= radConv;
		sinLon[j] = sin(lon[j]);
		cosLon[j] = cos(lon[j]);
		sin2Lon[j] = sin(2*lon[j]);
		cos2Lon[j] = cos(2*lon[j]);
	}
	dLat *= radConv;
	dLon *= radConv;
};

int Field::ReturnFieldLatLen(){
	return fieldLatLen;
};

int Field::ReturnFieldLonLen(){
	return fieldLonLen;
};

double Field::SouthWestAvg(int i, int j) {
	if (j > 0) {
		return 0.25*(solution[i][j] + solution[i + 1][j] + solution[i][j - 1] + solution[i + 1][j - 1]);
	}
	else {
		return 0.25*(solution[i][j] + solution[i + 1][j] + solution[i][fieldLonLen - 1] + solution[i + 1][fieldLonLen-1]);
	}
};

double Field::NorthEastAvg(int i, int j) {
	if (j < fieldLonLen-1) {
		return 0.25*(solution[i][j] + solution[i - 1][j] + solution[i][j + 1] + solution[i - 1][j + 1]);
	}
	else {
		return 0.25*(solution[i][j] + solution[i - 1][j] + solution[i][0] + solution[i - 1][0]);
	}
};
