/*
 * @brief The original birch C++ code is directly borrowed from github and is hard to documentation. We only
 * provide basic documentation for calling the functions directly from the code
 * @author Lieyu Shi
 */


#ifndef _CLUSTERANALYSIS_H_
#define _CLUSTERANALYSIS_H_

#include "CFTree.h"
#include "IOHandler.h"
#include "Silhouette.h"
//#include "item_type.h"
#include "ValidityMeasurement.h"
#include <string>
#include <sstream>
#include <fstream>
#include <climits>
#include <string.h>
#include <time.h>
#include <sstream>


/*
 * @brief The vector to store the calculation time and status
 */
std::vector<string> activityList;
std::vector<double> timeList;

/*
 * @brief whether it is PBF dataset
 */
bool isPBF;

/*
 * @brief whether read cluster from local file
 */
bool readCluster;

/*
 * @brief whether the data set is pathline or not
 */
bool isPathlines;

/*
 * @brief The MetricPreparation object used to calculate the distance matrix
 */
template<boost::uint32_t dim>
MetricPreparation CFTree<dim>::object = MetricPreparation();

/*
 * @brief The norm option for BIRCH clustering
 */
template<boost::uint32_t dim>
int CFTree<dim>::normOption = -1;

/*
 * @brief The total number of nodes in the BIRCH clustering
 */
template<boost::uint32_t dim>
int CFTree<dim>::totalNodes = 0;

/*
 * @brief The CFT tree with defined size
 */
typedef CFTree<4824u> cftree_type;

/*
 * @brief The distance threshold for the BIRCH clustering input
 */
cftree_type::float_type birch_threshold;


/*
 * @brief The struct to record the vertex count and max dimension of the data set
 */
struct FileIndex
{
	int vertexCount, maxElement;
	FileIndex()
	{}

	~FileIndex()
	{}
};


/*
 * @brief Get user input from console and perform related operations
 *
 * @param[in] argc Count of argument
 * @param[in] argv Argv* string of argument
 * @param[out] trajectories The input line coordinates
 * @param[out] equalArray The matrix of coordinates after sampling with equal size of vertices
 * @param[out] dimension The max dimension
 * @param[out] fi An FileIndex object
 */
template<boost::uint32_t dim>
void getUserInput(const int& argc, 
				  char **argv,
				  std::vector<std::vector<float> >& trajectories,
				  Eigen::MatrixXf& equalArray,
				  std::vector<item_type<dim> >& items,
				  int& dimension,
				  FileIndex& fi)
{
	if( argc != 3 )
	{
		std::cout << "usage: birch (input-file) (dimension)" << std::endl;
		exit(1);
	}
	int samplingMethod;
	stringstream ss;
	ss << "../dataset/" << argv[1];

	/* get the bool tag for isPBF */
	std::cout << "It is a PBF dataset? 1.Yes, 0.No" << std::endl;
	int PBFjudgement;
	std::cin >> PBFjudgement;
	assert(PBFjudgement==1||PBFjudgement==0);
	isPBF = (PBFjudgement==1);

	// whether it is pathline
	std::cout << "It is a pathline dataset? 1.Yes, 0.No" << std::endl;
	std::cin >> PBFjudgement;
	assert(PBFjudgement==1||PBFjudgement==0);
	isPathlines = (PBFjudgement==1);

	// how to get input number of clusters
	std::cout << "---------------------------" << std::endl;
	std::cout << "Choose cluster number input method: 0.user input, 1.read from file: " << std::endl;
	int clusterInput;
	std::cin >> clusterInput;
	assert(clusterInput==0||clusterInput==1);
	readCluster = (clusterInput==1);

	// sampling
	if(isPathlines)
		samplingMethod = 1;
	else
	{
		std::cout << "Please choose the sampling method? " << endl
				  << "1.filling, 2.uniform sampling." << std::endl;
		std::cin >> samplingMethod;
	}
	assert(samplingMethod==1||samplingMethod==2);

	dimension = atoi(argv[2]);

	struct timeval start, end;
	double timeTemp;

	// read coordinates from the txt file
	gettimeofday(&start, NULL);
	IOHandler::readFile(ss.str(), trajectories, fi.vertexCount, dimension, fi.maxElement);
	gettimeofday(&end, NULL);
	timeTemp = ((end.tv_sec  - start.tv_sec) * 1000000u 
			   + end.tv_usec - start.tv_usec) / 1.e6;
	activityList.push_back("Read file takes: ");
	timeList.push_back(timeTemp);

	// perform sampling on the data sets
	gettimeofday(&start, NULL);
	if(samplingMethod==1)
		IOHandler::expandArray(equalArray,trajectories,dimension,
							   fi.maxElement);
	else if(samplingMethod==2)
		IOHandler::sampleArray(equalArray,trajectories,dimension,
							   fi.maxElement);
	gettimeofday(&end, NULL);
	timeTemp = ((end.tv_sec  - start.tv_sec) * 1000000u 
			   + end.tv_usec - start.tv_usec) / 1.e6;
	activityList.push_back("Pre-processing takes: ");
	timeList.push_back(timeTemp);
}


/*
 * @brief Find the maximal number of clusters
 *
 * @param[in] fname The file name as input
 * @param[out] items T type of object to be updated
 */
template<typename T>
static void print_items( const std::string fname, T& items )
{
	struct _compare_item_id
	{
		bool operator()( const item_type<3>& lhs, const item_type<3>& rhs ) 
		const { return lhs.cid() < rhs.cid(); }
	};

	// find the max group index
	int maxGroup = INT_MIN;
	int belongGroup;
	for( std::size_t i = 0 ; i < items.size() ; i++ )
	{
//		for( std::size_t d = 0 ; d < cftree_type::fdim ; d++ )
//			fout << items[i].item[d] << " ";
		belongGroup = items[i].cid();
		if(belongGroup>maxGroup)
			maxGroup=belongGroup;
	}
}


/*
 * @brief Load items into the global tree object
 *
 * @param[in] matrixData The matrix of the line coordinates as input
 * @param[out] items The vector information to be updated
 */
template<boost::uint32_t dim>
static void load_items( const Eigen::MatrixXf& matrixData,
						std::vector<item_type<dim> >& items)
{
    items.resize(matrixData.rows());
#pragma omp parallel for schedule(static) num_threads(8)
    for (int i = 0; i < items.size(); ++i)
    {
    	const Eigen::VectorXf& eachRow = matrixData.row(i);
    	cftree_type::item_vec_type item(eachRow.data(), eachRow.data()+eachRow.size());
		items[i] = &(item[0]);
    }
}


/*
 * @brief Get the max distance
 *
 * @param[in] equalArray The coordinate matrix
 * @param[in] object The MetricPreparation object for distance matrix computation
 * @param[in] normOption The norm
 * @return The max distance
 */
const float getMaxDist(const Eigen::MatrixXf& equalArray, 
					   const MetricPreparation& object, 
					   const int& normOption)
{
	// find the maximal distance value
	const float& Percentage = 0.1;
	const int& chosen = int(Percentage*equalArray.rows());
	float result = -0.1;
#pragma omp parallel for reduction(max:result) num_threads(8)
	for (int i = 0; i < chosen; ++i)
	{
		for (int j = 0; j < equalArray.rows(); ++j)
		{
			if(i==j)
				continue;

			float dist;
			if(distanceMatrix)
				dist = distanceMatrix[i][j];
			else
				dist = getDisimilarity(equalArray.row(i), equalArray.row(j),i,j,normOption,object);
			if(dist>result)
				result=dist;
		}
	}	
	return result;
}


/*
 * @brief Perform the binary search to find the distance threshold to get approximately the clusters
 *
 * @param[in] object The MetricPreparation object for distance matrix computation
 * @param[in] normOption the norm option
 * @param[out] items The item vector type
 * @param[in] distThreshold The distance threshold
 * @param[out] maxGroup The finalized cluster number
 * @param[out] item_cids The cluster formalized
 */
template<boost::uint32_t dim>
void getBirchClusterTrial(const MetricPreparation& object,
						  const int& normOption,
						  std::vector<item_type<dim> >& items,
						  const float& distThreshold,
						  int& maxGroup,
						  std::vector<int>& item_cids)
{
	cftree_type tree(distThreshold, 0);
	tree.cftree_type::object = object;
	tree.cftree_type::normOption = normOption;
	tree.cftree_type::totalNodes = items.size();

	// phase 1 and 2: building, compacting when overflows memory limit
	for( std::size_t i = 0 ; i < items.size() ; i++ )
	{
		if(&(items[i][0]))
			tree.insert((float_type*)(&(items[i][0])));
	}

	// phase 2 or 3: compacting? or clustering?
	// merging overlayed sub-clusters by rebuilding true
	std::cout << "Curve dimensionality is: " << cftree_type::fdim << std::endl;

	tree.rebuild(false);

	// phase 3: clustering sub-clusters using the existing clustering algorithm
	//cftree_type::cfentry_vec_type entries;
	std::vector<CFEntry<4824u> > entries;
	
	item_cids.clear();

	tree.cluster( entries );

	// phase 4: redistribution

	// @comment ts - it is also possible to another clustering algorithm hereafter
	//				for example, we have k initial points for k-means clustering algorithm
	//tree.redist_kmeans( items, entries, 0 );
    tree.redist(items.begin(), items.end(), entries, item_cids);
    maxGroup = INT_MIN;

    // assign the group labels
    for (std::size_t i = 0; i < item_cids.size(); i++)
    {
    	int& itemCID = items[i].cid(); 
        itemCID = item_cids[i];
        if(maxGroup<itemCID)
        	maxGroup=itemCID;
    }
}
								

/*
 * @brief Perform birch clustering with iterative binary search for the optimal distance threshold
 *
 * @param[out] items The item vector
 * @param[in] argv The data set name and position
 * @param[out] trajectories The read-in data set
 * @param[in] fi The FileIndex
 * @param[out] equalArray The matrix coordinate
 * @param[in] dimension The max dimension
 * @param[out] item_cids The cluster labels
 * @param[out] maxGroup The max group
 * @param[out] normOption The norm option
 * @param[out] fullName The .vtk file position
 * @param[out] object The MetricPreparation class object
 */
template<boost::uint32_t dim>
void getBirchClustering(std::vector<item_type<dim> >& items,
						char **argv,
						std::vector<std::vector<float> >& trajectories,
						const FileIndex& fi,
						Eigen::MatrixXf& equalArray,
						const int& dimension,
						std::vector<int>& item_cids,
						int& maxGroup,
						int& normOption,
						string& fullName,
						MetricPreparation& object)
{
	// select norm
	std::cout << std::endl;
	if(isPathlines)
	{
		std::cout << "Choose a norm from 0-17!" << std::endl;
		std::cin >> normOption;
		assert(normOption>=0 && normOption<=17);
	}
	else
	{
		std::cout << "Choose a norm from 0-16!" << std::endl;
		std::cin >> normOption;
		assert(normOption>=0 && normOption<=16);
	}

	/*  0: Euclidean Norm
		1: Fraction Distance Metric
		2: piece-wise angle average
		3: Bhattacharyya metric for rotation
		4: average rotation
		5: signed-angle intersection
		6: normal-direction multivariate distribution
		7: Bhattacharyya metric with angle to a fixed direction
		8: Piece-wise angle average \times standard deviation
		9: normal-direction multivariate un-normalized distribution
		10: x*y/|x||y| borrowed from machine learning
		11: cosine similarity
		12: Mean-of-closest point distance (MCP)
		13: Hausdorff distance min_max(x_i,y_i)
		14: Signature-based measure from http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=6231627
		15: Procrustes distance take from http://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=6787131
		16: entropy-based distance metric taken from http://vis.cs.ucdavis.edu/papers/pg2011paper.pdf
		17: time-series MCP distance from https://www.sciencedirect.com/science/article/pii/S0097849318300128
			for pathlines only
	*/

	struct timeval start, end;
	double timeTemp;
	gettimeofday(&start, NULL);

	// create MetricPreparation object
	object = MetricPreparation(equalArray.rows(), equalArray.cols());
	object.preprocessing(equalArray, equalArray.rows(), equalArray.cols(), normOption);

	// load the array into B+ tree
	load_items(equalArray, items);
	std::cout << items.size() << " items loaded" << std::endl;

	/* if the dataset is not PBF, then should record distance matrix for Gamma matrix compution */
	if(!isPBF)
	{
		deleteDistanceMatrix(equalArray.rows());

		std::ifstream distFile(("../dataset/"+to_string(normOption)).c_str(), ios::in);
		if(distFile.fail())	// file does not exist, calculate distance matrix and store it into file
		{
			distFile.close();
			getDistanceMatrix(equalArray, normOption, object);
			std::ofstream distFileOut(("../dataset/"+to_string(normOption)).c_str(), ios::out);
			for(int i=0;i<equalArray.rows();++i)
			{
				for(int j=0;j<equalArray.rows();++j)
				{
					distFileOut << distanceMatrix[i][j] << " ";
				}
				distFileOut << std::endl;
			}
			distFileOut.close();
		}
		else	// file exists, directly read in from the file
		{
			std::cout << "read distance matrix..." << std::endl;

			distanceMatrix = new float*[equalArray.rows()];
		#pragma omp parallel for schedule(static) num_threads(8)
			for (int i = 0; i < equalArray.rows(); ++i)
			{
				distanceMatrix[i] = new float[equalArray.rows()];
			}

			int i=0, j;
			string line;
			stringstream ss;
			while(getline(distFile, line))
			{
				j=0;
				ss.str(line);
				while(ss>>line)
				{
					if(i==j)
						distanceMatrix[i][j]=0;
					else
						distanceMatrix[i][j] = std::atof(line.c_str());
					++j;
				}
				++i;
				ss.str("");
				ss.clear();
			}
			distFile.close();
		}
	}
	// get max distance
	const float distThreshold = getMaxDist(equalArray, object, normOption);

	// get the input cluster number
	int requiredClusters;
	if(readCluster)	// from the file "cluster_number"
	{
		std::unordered_map<int,int> clusterMap;
		IOHandler::readClusteringNumber(clusterMap, "cluster_number");
		requiredClusters = clusterMap[normOption];
	}
	else	// or from user input on the console
	{
		std::cout << "Enter approximate number of clusters: " << std::endl;
		std::cin >> requiredClusters;
	}
	const int& upperClusters = requiredClusters*1.2;
	const int& lowerClusters = requiredClusters*0.8;

	std::cout << "Sampled max distance is: " << distThreshold << std::endl;

	float left = 0, right, middle;
	if(normOption==15)
		right = 0.2;
	else
		right = 0.5;

	// 10 times of binary search on the distance
	int iteration = 0;
	while(true&&iteration<10)
	{
		std::cout << "Iteration for birch clustering: " << ++iteration
				  << std::endl;
		middle = (left+right)/2.0;
		std::cout << "Weight is " << middle << std::endl;
		getBirchClusterTrial(object,normOption,items,middle*distThreshold,maxGroup,item_cids);
		std::cout << maxGroup << std::endl;
		if(maxGroup<=upperClusters && maxGroup>=lowerClusters)
			break;
		else if(maxGroup>upperClusters)
			left = middle;
		else if(maxGroup<lowerClusters)
			right = middle;
	}
	birch_threshold = middle;

	// finish the birch clustering
	gettimeofday(&end, NULL);
	timeTemp = ((end.tv_sec  - start.tv_sec) * 1000000u 
			   + end.tv_usec - start.tv_usec) / 1.e6;
	activityList.push_back("Birch clustering takes: ");
	timeList.push_back(timeTemp);

    std::cout << "Max group is: " << maxGroup << std::endl;
    //print_items(argc >= 4 ? ss.str().c_str() : "item_cid.txt", items);
    stringstream ss;
    ss << "../dataset/" << argv[1] << "_full.vtk";
    fullName = ss.str();

    IOHandler::printVTK(fullName, trajectories, fi.vertexCount, dimension); 
}


/*
 * @brief Get the cluster analysis for Birch clustering result
 *
 * @param trajectories: The read-in data set
 * @param fi: The FileIndex
 * @param equalArray: The matrix coordinate
 * @param dimension: The max dimension
 * @param item_cids: The cluster labels
 * @param maxGroup: The max group
 * @param normOption: The norm option
 * @param fullName: The .vtk file position
 * @param object: The MetricPreparation class object
 */
void getClusterAnalysis(const vector<vector<float> >& trajectories,
						const FileIndex& fi,
						const MatrixXf& equalArray,
						const int& dimension, 
					    vector<int>& item_cids, 
					    const int& maxGroup, 
					    const int& normOption,
					    const string& fullName,
					    const MetricPreparation& object)
{
	// get the size of clusters
	int numClusters = maxGroup+1;
	std::vector<int> container(numClusters,0);
	for (int i = 0; i < item_cids.size(); ++i)
		++container[item_cids[i]];

	int increasingOrder[numClusters];
	std::multimap<int,int> groupMap;

	for (int i = 0; i < numClusters; ++i)
		groupMap.insert(std::pair<int,int>(container[i],i));

	// find how many clusters are formed
	std::fill(container.begin(), container.end(), 0);
	int groupNo = 0;
	for (std::multimap<int,int>::iterator it=groupMap.begin();it!=groupMap.end();++it)
	{
		if(it->first>0)
		{
			increasingOrder[it->second] = groupNo;
			container[groupNo] = it->first;
			++groupNo;
		}
	}

	numClusters = groupNo;
	/* compute balanced Entropy value for the clustering algorithm */
	const int& Row = equalArray.rows();
	float entropy = 0.0, probability;
	for(int i=0;i<container.size();++i)
	{
		probability = float(container[i])/float(Row);
		entropy+=probability*log2f(probability);
	}
	entropy = -entropy/log2f(numClusters);

	// re-assign the cluster label in ascending order
#pragma omp parallel for schedule(static) num_threads(8)
	for (int i = 0; i < item_cids.size(); ++i)
		item_cids[i]=increasingOrder[item_cids[i]];

	std::vector<std::vector<int> > storage(numClusters);
	for (int i = 0; i < item_cids.size(); ++i)
		storage[item_cids[i]].push_back(i);

	/* record labeling information */
	// IOHandler::generateGroups(storage);


	IOHandler::printClusters(trajectories,item_cids,container,"norm"+to_string(normOption),
			fullName,dimension);

	struct timeval start, end;
	double timeTemp;

	// calculate the normalized validity measurement
	ValidityMeasurement vm;
	vm.computeValue(normOption, equalArray, item_cids, object, isPBF);
	activityList.push_back("Validity measure is: ");
	timeList.push_back(vm.f_c);

	// calculate the silhouette, gamma statistics and DB index
	gettimeofday(&start, NULL);
	Silhouette sil;
	sil.computeValue(normOption,equalArray,equalArray.rows(),equalArray.cols(),item_cids,
			object,numClusters,isPBF);
	gettimeofday(&end, NULL);
	timeTemp = ((end.tv_sec  - start.tv_sec) * 1000000u 
			   + end.tv_usec - start.tv_usec) / 1.e6;
	activityList.push_back("Silhouette calculation takes: ");
	timeList.push_back(timeTemp);

	/* compute the centroid coordinates of each clustered group */
	Eigen::MatrixXf centroid = MatrixXf::Zero(numClusters,equalArray.cols());
	vector<vector<float> > cenVec(numClusters);
#pragma omp parallel for schedule(static) num_threads(8)
	for (int i=0;i<numClusters;++i)
	{
		const std::vector<int>& groupRow = storage[i];
		for (int j = 0; j < groupRow.size(); ++j)
		{
			centroid.row(i)+=equalArray.row(groupRow[j]);
		}		
		centroid.row(i)/=groupRow.size();
		const Eigen::VectorXf& vec = centroid.row(i);
		cenVec[i] = vector<float>(vec.data(), vec.data()+equalArray.cols());
	}

	// calculate the closest and further representative for each cluster
	vector<vector<float> > closest(numClusters);
	vector<vector<float> > furthest(numClusters);
#pragma omp parallel for schedule(static) num_threads(8)
	for (int i=0;i<numClusters;++i)
	{
		float minDist = FLT_MAX;
		float maxDist = -10;
		int minIndex = -1, maxIndex = -1;
		const std::vector<int>& groupRow = storage[i];
		const Eigen::VectorXf& eachCentroid = centroid.row(i);
		for (int j = 0; j < groupRow.size(); ++j)
		{
			float distance = getDisimilarity(eachCentroid,equalArray,groupRow[j],normOption,object);
			if(minDist>distance)
			{
				minDist = distance;
				minIndex = groupRow[j];
			}
			if(maxDist<distance)
			{				
				maxDist = distance;
				maxIndex = groupRow[j];
			}
		}
		closest[i] = trajectories[minIndex];
		furthest[i] = trajectories[maxIndex];
	}

	// print the representative in .vtk format
	std::cout << "Finishing extracting features!" << std::endl;	
	IOHandler::printFeature("norm"+to_string(normOption)+"_closest.vtk", 
			closest, sil.sCluster, dimension);
	IOHandler::printFeature("norm"+to_string(normOption)+"_furthest.vtk",
			furthest, sil.sCluster, dimension);
	IOHandler::printFeature("norm"+to_string(normOption)+"_centroid.vtk", 
			cenVec, sil.sCluster,dimension);

	IOHandler::printToFull(trajectories, sil.sData, 
			"norm"+to_string(normOption)+"_SValueLine", fullName, dimension);
	IOHandler::printToFull(trajectories, item_cids, sil.sCluster, 
		      "norm"+to_string(normOption)+"_SValueCluster", fullName, dimension);

	IOHandler::generateReadme(activityList,timeList,normOption,
				numClusters,sil.sAverage,birch_threshold);

	/* print entropy value for the clustering algorithm */
	IOHandler::writeReadme(entropy,sil,"For norm "+to_string(normOption));

	/* measure closest and furthest rotation */
	std::vector<float> closestRot, furthestRot;
	const float& closestAverage = getRotation(closest, closestRot);
	const float& furthestAverage = getRotation(furthest, furthestRot);

	IOHandler::writeReadme(closestAverage, furthestAverage);
}


/*
 * @brief Get randome float (0,1)
 * @return A float value
 */
static float randf()
{
	return float(rand()/(double)RAND_MAX);
}


#endif
