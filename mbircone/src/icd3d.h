
#ifndef ICD_H
#define ICD_H

#include "MBIRModularUtilities3D.h"


void ICDStep3DCone(struct Sino *sino, struct Image *img, struct SysMatrix *A, struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams, struct ReconAux *reconAux);

void prepareICDInfo(long int j_x, long int j_y, long int j_z, struct ICDInfo3DCone *icdInfo, struct Image *img, struct ReconAux *reconAux, struct ReconParams *reconParams);

void extractNeighbors( struct ICDInfo3DCone *icdInfo, struct Image *img, struct ReconParams *reconParams);

void computeTheta1Theta2ForwardTerm(struct Sino *sino, struct SysMatrix *A, struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams);

void computeTheta1Theta2PriorTermQGGMRF(struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams);

void computeTheta1Theta2PriorTermProxMap(struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams);

float surrogateCoeffQGGMRF(float Delta, struct ReconParams *reconParams);

void updateErrorSinogram(struct Sino *sino, struct SysMatrix *A, struct ICDInfo3DCone *icdInfo);

void updateIterationStats(struct ReconAux *reconAux, struct ICDInfo3DCone *icdInfo, struct Image *img);

void resetIterationStats(struct ReconAux *reconAux);


void RandomAux_ShuffleOrderXYZ(struct RandomAux *aux, struct ImageParams *params);

void indexExtraction3D(long int j_xyz, long int *j_x, long int N_x, long int *j_y, long int N_y, long int *j_z, long int N_z);



float MAPCost3D(struct Sino *sino, struct Image *img, struct ReconParams *reconParams);

float MAPCostForward(struct Sino *sino);

float MAPCostPrior_QGGMRF(struct Image *img, struct ReconParams *reconParams);

float MAPCostPrior_ProxMap(struct Image *img, struct ReconParams *reconParams);

float MAPCostPrior_QGGMRFSingleVoxel_HalfNeighborhood(struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams);

float QGGMRFPotential(float delta, struct ReconParams *reconParams);


void partialZipline_computeStartStopIndex(long int *j_z_start, long int *j_z_stop, long int indexZiplines, long int numVoxelsPerZipline, long int N_z);

void prepareICDInfoRandGroup(long int j_x, long int j_y, struct RandomZiplineAux *randomZiplineAux, struct ICDInfo3DCone *icdInfo, struct Image *img, struct ReconParams *reconParams, struct ReconAux *reconAux);

void computeDeltaXjAndUpdate(struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams, struct Image *img, struct ReconAux *reconAux);

void computeDeltaXjAndUpdateGroup(struct ICDInfo3DCone *icdInfo, struct RandomZiplineAux *randomZiplineAux, struct ReconParams *reconParams, struct Image *img, struct ReconAux *reconAux);

void updateIterationStatsGroup(struct ReconAux *reconAux, struct ICDInfo3DCone *icdInfoArray, struct RandomZiplineAux *randomZiplineAux, struct Image *img, struct ReconParams *reconParams);

void disp_iterationInfo(struct ReconAux *reconAux, struct ReconParams *reconParams, int itNumber, int MaxIterations, float cost, float relUpdate, float stopThresholdChange, float weightScaler_value, float voxelsPerSecond, float ticToc_iteration, float weightedNormSquared_e, float ratioUpdated, float totalEquits);

float computeRelUpdate(struct ReconAux *reconAux, struct ReconParams *reconParams, struct Image *img);

/* * * * * * * * * * * * parallel * * * * * * * * * * * * **/
void prepareParallelAux(struct ParallelAux *parallelAux, long int N_M_max);

void freeParallelAux(struct ParallelAux *parallelAux);

void ICDStep3DConeGroup(struct Sino *sino, struct Image *img, struct SysMatrix *A, struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams, struct RandomZiplineAux *randomZiplineAux, struct ParallelAux *parallelAux, struct ReconAux *reconAux);

void computeTheta1Theta2ForwardTermGroup(struct Sino *sino, struct SysMatrix *A, struct ICDInfo3DCone *icdInfo, struct RandomZiplineAux *randomZiplineAux, struct ParallelAux *parallelAux, struct ReconParams *reconParams);

void computeTheta1Theta2PriorTermQGGMRFGroup(struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams, struct RandomZiplineAux *randomZiplineAux);

void updateErrorSinogramGroup(struct Sino *sino, struct SysMatrix *A, struct ICDInfo3DCone *icdInfo, struct RandomZiplineAux *randomZiplineAux);

void computeTheta1Theta2PriorTermProxMapGroup(struct ICDInfo3DCone *icdInfo, struct ReconParams *reconParams, struct RandomZiplineAux *randomZiplineAux);

/* * * * * * * * * * * * time aux ICD * * * * * * * * * * * * **/

void speedAuxICD_reset(struct SpeedAuxICD *speedAuxICD);

void speedAuxICD_update(struct SpeedAuxICD *speedAuxICD, long int incrementNumber);

void speedAuxICD_computeSpeed(struct SpeedAuxICD *speedAuxICD);

/* * * * * * * * * * * * NHICD * * * * * * * * * * * * **/

int NHICD_isVoxelHot(struct ReconParams *reconParams, struct Image *img, long int j_x, long int j_y, long int j_z, float lastChangeThreshold);

int NHICD_activatePartialUpdate(struct ReconParams *reconParams, float relativeWeightedForwardError);

int NHICD_checkPartialZiplineHot(struct ReconAux *reconAux, long int j_x, long int j_y, long int indexZiplines, struct Image *img);

void NHICD_checkPartialZiplinesHot(struct ReconAux *reconAux, long int j_x, long int j_y, struct ReconParams *reconParams, struct Image *img);

void updateNHICDStats(struct ReconAux *reconAux, long int j_x, long int j_y, struct Image *img, struct ReconParams *reconParams);

#endif

