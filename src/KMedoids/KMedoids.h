#ifndef _KMEDOIDS_H
#define _KMEDOIDS_H

/* a class KMedoids to perform k-medoid clustering */

#include "IOHandler.h"
#include "Initialization.h"
#include "Silhouette.h"
#include "ValidityMeasurement.h"

struct Parameter
{
	int initialization;
	bool isSample;
	Parameter(const int& initialization, const bool& isSample)
	: initialization(initialization), isSample(isSample)
	{}
	Parameter()
	{}
	~Parameter()
	{}
};


struct TimeRecorder
{
	std::vector<string> eventList;
	std::vector<string> timeList;
};



class KMedoids
{
public:
	KMedoids(const Parameter& pm,
			 const Eigen::MatrixXf& data,
			 const int& numOfClusters);

	~KMedoids();

	void getMedoids(FeatureLine& fline,
					const int& normOption,
					Silhouette& sil,
					TimeRecorder& tr) const;

	int numOfClusters;

private:
	int initialStates;	//1 is random initialization,
						//2 is chosen from samples
						//3 is chosen with k-medoids ++
	bool isSample;	//true means centroid should be from samples,
					//false means centroids are within domain by iterations
	Eigen::MatrixXf data;


	void getInitCenter(MatrixXf& initialCenter,
					   const MetricPreparation& object,
					   const int& normOption) const;

	void computeMedoids(MatrixXf& centerTemp, 
						const vector<vector<int> >& neighborVec, 
						const int& normOption, 
						const MetricPreparation& object) const;
};


#endif
