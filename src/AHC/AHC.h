/* Standard agglomerative hierarchical clustering methods.
 * Assume input object number is N, then form a N*N distance matrix to be stored.
 * Basic procedures are
 * 	1. store distance matrix
 * 	2. min-heap to sort mutual distance
 * 	3. every time merge two nodes with smallest distance and update the min-heap
 * 	4. merge until only one cluster or given cluster number is obtained
 */

/* Performance and memory usage analysis
 * 1. Performance
 * 		N*N (distance matrix) + 2N*N logN (build min-heap) + 2N*N*log(N*N) (min-heap update)
 * 2. Memory usage
 * 		N*N (distance matrix) + N*N(min-heap)
 *
 * Would expect this algorithm to be super time-consuming and memory-wasting
 */

#ifndef _AHC_H_
#define _AHC_H_

#include "Predefined.h"
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <string>
#include <algorithm>

class AHC
{

public:

/* default constructor */
	AHC();

/* argument constructor with argc and argv */
	AHC(const int& argc, char **argv);

/* destructor */
	~AHC();

/* perform clustering function */
	void performClustering();

private:
/* extract features from datasets as representative curves */
	void extractFeatures(const std::vector<int>& storage, const std::vector<std::vector<int> >& neighborVec,
            			 const Eigen::MatrixXf& centroid);

/* metric preparation object to be stored ahead of time */
	MetricPreparation object;

/* input norm option */
	int normOption;

/* the tag to tell whether it's a PBF or not */
	bool isPBF;

/* group information */
	std::vector<int> group;

/* activityList vector to store event */
	std::vector<string> activityList;

/* timeList vector to store time information */
	std::vector<string> timeList;

/* store dataset information */
	DataSet ds;

/* how many clusters to be needed */
	int numberOfClusters;

/* k-means initialization option */
	int initializationOption;

/* linkage choice */
	int linkageOption;

/* set dataset from user command */
	void setDataset(const int& argc, char **argv);

/* set norm option, must be within 0-12 */
	void setNormOption();

/* compute distance between two clusters based on likage type */
	const float getDistAtNodes(const vector<int>& firstList, const vector<int>& secondList, const int& Linkage);

/* perform AHC merging by given a distance threshold */
	void hierarchicalMerging(std::unordered_map<int, Ensemble>& nodeMap, std::vector<DistNode>& dNodeVec,
			  std::vector<Ensemble>& nodeVec);

/* perform group-labeling information */
	void setLabel(const std::vector<Ensemble>& nodeVec, vector<vector<int> >& neighborVec,
			      vector<int>& storage, Eigen::MatrixXf& centroid);

/* get string for linkage type */
	string getLinkageStr();	

/* get entropy ratio */
	void getEntropyRatio(const std::vector<int>& storage, float& EntropyRatio);

/* get norm string */
	string getNormStr();

/* get entropy ratio string */ 
	string getEntropyStr(const float& EntropyRatio);	

/* set a vector for min-heap */
	void setValue(std::vector<DistNode>& dNodeVec);

};

#endif
