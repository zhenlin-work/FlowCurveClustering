#include "AHC.h"

/* default constructor */
AHC::AHC()
{

}

/* argument constructor with argc and argv */
AHC::AHC(const int& argc, char **argv)
{
	setDataset(argc, argv);
	setNormOption();

	/* very hard to decide whether needed to perform such pre-processing */
	object = MetricPreparation(ds.dataMatrix.rows(), ds.dataMatrix.cols());
	object.preprocessing(ds.dataMatrix, ds.dataMatrix.rows(), ds.dataMatrix.cols(), normOption);

	/* would store distance matrix instead because it would save massive time */
	struct timeval start, end;
	double timeTemp;
	gettimeofday(&start, NULL);

	if(!getDistanceMatrix(ds.dataMatrix, normOption, object))
	{
		std::cout << "Failure to compute distance matrix!" << std::endl;
	}

	gettimeofday(&end, NULL);
	timeTemp = ((end.tv_sec  - start.tv_sec) * 1000000u 
			   + end.tv_usec - start.tv_usec) / 1.e6;
	activityList.push_back("Distance matrix computing takes: ");
	timeList.push_back(to_string(timeTemp)+" s");
}

/* destructor */
AHC::~AHC()
{
	deleteDistanceMatrix(ds.dataMatrix.rows());
}

/* perform clustering function */
void AHC::performClustering()
{
	std::unordered_map<int, Ensemble> nodeMap;
	std::vector<DistNode> dNodeVec;
	std::vector<Ensemble> nodeVec;

	/* set the ditNode vector */
	setValue(dNodeVec);

	/* perform hiarchical clustering where within each step would merge two nodes */
	hierarchicalMerging(nodeMap, dNodeVec, nodeVec);

	vector<vector<int>> neighborVec(numberOfClusters);
	// element size for all groups
	vector<int> storage(numberOfClusters);

	// geometric center
	Eigen::MatrixXf centroid = Eigen::MatrixXf::Zero(numberOfClusters,ds.dataMatrix.cols());


	// set label information
	setLabel(nodeVec, neighborVec, storage, centroid);

	nodeVec.clear();

	extractFeatures(storage, neighborVec, centroid);
}


/* perform AHC merging by given a distance threshold */
void AHC::hierarchicalMerging(std::unordered_map<int, Ensemble>& nodeMap, std::vector<DistNode>& dNodeVec,
							  std::vector<Ensemble>& nodeVec)
{
	/* would store distance matrix instead because it would save massive time */
	struct timeval start, end;
	double timeTemp;
	gettimeofday(&start, NULL);

	const int Row = ds.dataMatrix.rows();

	for(int i=0;i<Row;++i)
	{
		nodeMap[i].element.push_back(i);
	}

	DistNode poped;

	/* find node-pair with minimal distance */
	float minDist = FLT_MAX;
	int target = -1;
	for (int i = 0; i < dNodeVec.size(); ++i)
	{
		if(dNodeVec[i].distance<minDist)
		{
			target = i;
			minDist = dNodeVec[i].distance;
		}
	}
	poped = dNodeVec[target];

	int index = Row, currentNumber;
	do
	{
		//create new node merged and input it into hash map
		vector<int> first = (nodeMap[poped.first]).element;
		vector<int> second = (nodeMap[poped.second]).element;

		/* index would be starting from Row */
		Ensemble newNode(index);
		newNode.element = first;
		newNode.element.insert(newNode.element.end(), second.begin(), second.end());
		nodeMap.insert(make_pair(index, newNode));

		//delete two original nodes
		nodeMap.erase(poped.first);
		nodeMap.erase(poped.second);

		/* the difficulty lies how to update the min-heap with linkage
		 * This would take 2NlogN.
		 * Copy all node-pairs that are not relevant to merged nodes to new vec.
		 * For relevant, would update the mutual distance by linkage
		 */

		/* how many clusters exist */
		currentNumber = nodeMap.size();

		target = -1, minDist = FLT_MAX;

		std::vector<DistNode> tempVec(currentNumber*(currentNumber-1)/2);
		int current = 0, i_first, i_second;
		for(int i=0;i<dNodeVec.size();++i)
		{
			i_first=dNodeVec[i].first, i_second=dNodeVec[i].second;
			/* not relevant, directly copied to new vec */
			if(i_first!=poped.first&&i_first!=poped.second&&i_second!=poped.first&&i_second!=poped.second)
			{
				tempVec[current]=dNodeVec[i];
				if(tempVec[current].distance<minDist)
				{
					target = current;
					minDist = tempVec[current].distance;
				}
				++current;
			}
		}

		for (auto iter=nodeMap.begin();iter!=nodeMap.end();++iter)
		{
			if((*iter).first!=newNode.index)
			{
				tempVec[current].first = (*iter).first;
				tempVec[current].second = newNode.index;
				tempVec[current].distance=getDistAtNodes(newNode.element,(*iter).second.element, linkageOption);
				if(tempVec[current].distance<minDist)
				{
					target = current;
					minDist = tempVec[current].distance;
				}
				++current;
			}
		}

		poped = tempVec[target];

		/* judge whether current is assigned to right value */
		assert(current==tempVec.size());
		dNodeVec.clear();
		dNodeVec = tempVec;
		tempVec.clear();
		++index;
	}while(nodeMap.size()!=numberOfClusters);	//merging happens whenever requested cluster is not met

	nodeVec=std::vector<Ensemble>(nodeMap.size());
	int tag = 0;
	for(auto iter=nodeMap.begin();iter!=nodeMap.end();++iter)
		nodeVec[tag++]=(*iter).second;

	gettimeofday(&end, NULL);
	timeTemp = ((end.tv_sec  - start.tv_sec) * 1000000u + end.tv_usec - start.tv_usec) / 1.e6;

	activityList.push_back("Hirarchical clustering for "+to_string(numberOfClusters)+" groups takes: ");
	timeList.push_back(to_string(timeTemp)+" s");
	/* task completed, would delete memory contents */
	dNodeVec.clear();
	nodeMap.clear();
	/* use alpha function to sort the group by its size */
	std::sort(nodeVec.begin(), nodeVec.end(), [](const Ensemble& e1, const Ensemble& e2)
	{return e1.element.size()<e2.element.size() ||(e1.element.size()==e2.element.size()&&e1.index<e2.index);});
}


/* perform group-labeling information */
void AHC::setLabel(const std::vector<Ensemble>& nodeVec, vector<vector<int> >& neighborVec,
			      vector<int>& storage, Eigen::MatrixXf& centroid)
{
// group tag by increasing order
	int groupID = 0;

	// element list for each group
	vector<int> eachContainment;

	// find group id and neighboring vec
	for(auto iter = nodeVec.begin(); iter!=nodeVec.end();++iter)
	{
		eachContainment = (*iter).element;
		neighborVec[groupID] = eachContainment;
	#pragma omp parallel num_threads(8)
		{
		#pragma omp for nowait
			for(int i=0;i<eachContainment.size();++i)
			{
				group[eachContainment[i]] = groupID;
			#pragma omp critical
				centroid.row(groupID) += ds.dataMatrix.row(eachContainment[i]);
			}
		}
		storage[groupID] = (*iter).element.size();
		centroid.row(groupID)/=eachContainment.size();
		++groupID;
		eachContainment.clear();
	}
}



/* extract features from datasets as representative curves */
void AHC::extractFeatures(const std::vector<int>& storage, const std::vector<std::vector<int> >& neighborVec,
		                  const Eigen::MatrixXf& centroid)
{
	const int& Row = ds.dataMatrix.rows();
	const int& Column = ds.dataMatrix.cols();

	std::cout << "Final group number information: " << std::endl;
	for (int i = 0; i < storage.size(); ++i)
	{
		std::cout << storage[i] << " ";
	}
	std::cout << std::endl;

	IOHandler::printClusters(ds.dataVec,group,storage,"norm"+to_string(normOption),ds.fullName,ds.dimension);

	struct timeval start, end;
	double timeTemp;

	gettimeofday(&start, NULL);
	Silhouette sil;
	sil.computeValue(normOption,ds.dataMatrix,ds.dataMatrix.rows(),ds.dataMatrix.cols(),group,object,numberOfClusters);
	gettimeofday(&end, NULL);
	timeTemp = ((end.tv_sec  - start.tv_sec) * 1000000u 
			   + end.tv_usec - start.tv_usec) / 1.e6;
	activityList.push_back("Silhouette calculation takes: ");
	timeList.push_back(to_string(timeTemp)+" s");

	/* compute the centroid coordinates of each clustered group */

	gettimeofday(&start, NULL);

	vector<vector<float> > closest(numberOfClusters);
	vector<vector<float> > furthest(numberOfClusters);

	/* extract the closest and furthest streamlines to centroid */

#pragma omp parallel for schedule(dynamic) num_threads(8)
	for (int i=0;i<numberOfClusters;++i)
	{
		float minDist = FLT_MAX;
		float maxDist = -10;
		int minIndex = -1, maxIndex = -1;
		const std::vector<int>& groupRow = neighborVec[i];
		const Eigen::VectorXf& eachCentroid = centroid.row(i);
		for (int j = 0; j < groupRow.size(); ++j)
		{
			float distance = getDisimilarity(eachCentroid,ds.dataMatrix,groupRow[j],normOption,object);
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
		closest[i] = ds.dataVec[minIndex];
		furthest[i] = ds.dataVec[maxIndex];
	}

	std::vector<std::vector<float> > center_vec(numberOfClusters, vector<float>(Column));
#pragma omp parallel for schedule(dynamic) num_threads(8)
	for (int i = 0; i < center_vec.size(); ++i)
	{
		for (int j = 0; j < Column; ++j)
		{
			center_vec[i][j] = centroid(i,j);
		}
	}

	float EntropyRatio;
	getEntropyRatio(storage, EntropyRatio);

	std::cout << "Entropy ratio is: " << EntropyRatio << std::endl;

	gettimeofday(&end, NULL);
	timeTemp = ((end.tv_sec  - start.tv_sec) * 1000000u 
			   + end.tv_usec - start.tv_usec) / 1.e6;
	activityList.push_back("Feature extraction takes: ");
	timeList.push_back(to_string(timeTemp)+" s");

	std::cout << "Finishing extracting features!" << std::endl;	

	stringstream ss;
	ss << "norm_" << normOption;

	string linkage = getLinkageStr();
	string normStr = getNormStr();
	string entropyStr = getEntropyStr(EntropyRatio);

	std::vector<float> closestRotation, furthestRotation;
	const float& closestAverage = getRotation(closest, closestRotation);
	const float& furthestAverage = getRotation(furthest, furthestRotation);

	IOHandler::printFeature(ds.dataName+"_AHC_"+linkage+"_closest_"+ss.str()+".vtk", closest, sil.sCluster,
			                closestRotation, ds.dimension);
	IOHandler::printFeature(ds.dataName+"_AHC_"+linkage+"_furthest_"+ss.str()+".vtk", furthest, sil.sCluster,
							furthestRotation, ds.dimension);
	IOHandler::printFeature(ds.dataName+"_AHC_"+linkage+"_centroid_"+ss.str()+".vtk", center_vec, sil.sCluster,ds.dimension);

	IOHandler::printToFull(ds.dataVec, sil.sData, "AHC_SValueLine_"+ss.str(), ds.fullName, ds.dimension);
	IOHandler::printToFull(ds.dataVec, group, sil.sCluster, "AHC_SValueCluster_"+ss.str(), ds.fullName, ds.dimension);

	activityList.push_back("numCluster is: ");
	timeList.push_back(to_string(numberOfClusters));

	activityList.push_back("Average Silhouette is: ");
	timeList.push_back(to_string(sil.sAverage));

	activityList.push_back("Average rotation of closest is: ");
	timeList.push_back(to_string(closestAverage));

	activityList.push_back("Average rotation of furthest is: ");
	timeList.push_back(to_string(furthestAverage));

	IOHandler::generateReadme(activityList,timeList);

	IOHandler::writeReadme("Linkage: "+linkage+", "+"norm option is "+normStr+", "+"entropy ratio is "+entropyStr);

	IOHandler::writeGroupSize(storage);
}

/* set dataset from user command */
void AHC::setDataset(const int& argc, char **argv)
{
	if(argc!=3)
	{
		std::cout << "Input argument should have 3!" << endl
		          << "./cluster inputFile_name(in dataset folder) "
		          << "data_dimension(3)" << endl;
		exit(1);
	}
	ds.strName = string("../dataset/")+string(argv[1]);
	ds.dataName = string(argv[1]);
	ds.dimension = atoi(argv[2]);

	int sampleOption;
    std::cout << "choose a sampling method for the dataset?" << std::endl
	    	  << "1.directly filling with last vertex; 2. uniform sampling." << std::endl;
	std::cin >> sampleOption;
	assert(sampleOption==1||sampleOption==2);

	IOHandler::readFile(ds.strName,ds.dataVec,ds.vertexCount,ds.dimension,ds.maxElements);

	ds.fullName = ds.strName+"_full.vtk";
	IOHandler::printVTK(ds.fullName, ds.dataVec, ds.vertexCount, ds.dimension);

	if(sampleOption==1)
		IOHandler::expandArray(ds.dataMatrix,ds.dataVec,ds.dimension,ds.maxElements);
	else if(sampleOption==2)
		IOHandler::sampleArray(ds.dataMatrix,ds.dataVec,ds.dimension,ds.maxElements);

	group = std::vector<int>(ds.dataMatrix.rows());

	std::cout << "---------------------------" << std::endl;
	std::cout << "Input linkage option: 0.single linkage, 1.complete linkage, 2.average linkage" << std::endl;
	std::cin >> linkageOption;
	assert(linkageOption==0||linkageOption==1||linkageOption==2);
}



/* set norm option, must be within 0-12 */
void AHC::setNormOption()
{
	std::cout << "Input a norm option 0-13!" << std::endl;
	std::cin >> normOption;
	std::cout << std::endl;
	/*  
		0: Euclidean Norm
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
		12: mean of closest point distance
		13: Hausdorff distance
	*/
	bool found = false;
	for (int i = 0; i < 14&&!found; ++i)
	{
		if(normOption==i)
		{
			found = true;
			break;
		}
	}
	if(!found)
	{
		std::cout << "Cannot find the norm!" << std::endl;
		exit(1);
	}

/* input target cluster number */
	const int& Row = ds.dataMatrix.rows();
	std::cout << "---------------------------------------" << std::endl;
	std::cout << "Input cluster number among [0, " << Row << "]: ";
	std::cin >> numberOfClusters;
	assert(numberOfClusters>0 && numberOfClusters<Row);
}


const float AHC::getDistAtNodes(const vector<int>& firstList, const vector<int>& secondList, const int& Linkage)
{
	const int& m = firstList.size();
	const int& n = secondList.size();
	assert(m!=0);
	assert(n!=0);
	/* 0: single linkage, min(x_i,y_j)
	 * 1: complete linkdage, max(x_i,y_j)
	 * 2: average linkage, sum/x_i/y_j
	 */
	float result, value;
	switch(Linkage)
	{
	case 0:	//single linkage
		{
			result = FLT_MAX;
		#pragma omp parallel for reduction(min:result) num_threads(8)
			for(int i=0;i<m;++i)
			{
				for(int j=0;j<n;++j)
				{
					if(distanceMatrix)
						value = distanceMatrix[firstList[i]][secondList[j]];
					else
						value = getDisimilarity(ds.dataMatrix, firstList[i], secondList[j], normOption, object);
					result = std::min(result, value);
				}
			}
		}
		break;

	case 1:	//complete linkage
		{
			result = FLT_MIN;
		#pragma omp parallel for reduction(max:result) num_threads(8)
			for(int i=0;i<m;++i)
			{
				for(int j=0;j<n;++j)
				{
					if(distanceMatrix)
						value = distanceMatrix[firstList[i]][secondList[j]];
					else
						value = getDisimilarity(ds.dataMatrix, firstList[i], secondList[j], normOption, object);
					result = std::max(result, value);
				}
			}
		}
		break;

	case 2:
		{
			result = 0;
		#pragma omp parallel for reduction(+:result) num_threads(8)
			for(int i=0;i<m;++i)
			{
				for(int j=0;j<n;++j)
				{
					if(distanceMatrix)
						value = distanceMatrix[firstList[i]][secondList[j]];
					else
						value = getDisimilarity(ds.dataMatrix, firstList[i], secondList[j], normOption, object);
					result+=value;
				}
			}
			result/=m*n;
		}
		break;

	default:
		std::cout << "error!" << std::endl;
		exit(1);
	}
	return result;
}


/* get string for linkage type */
string AHC::getLinkageStr()
{
	string result;
	switch(linkageOption)
	{
	case 0:
		result = "single";
		break;

	case 1:
		result = "complete";
		break;

	case 2:
		result = "average";
		break;
	}
	return result;
}


/* get entropy ratio, lower value tells dinstinguishable cluster while higher value tells a more uniformality. */
void AHC::getEntropyRatio(const std::vector<int>& storage, float& EntropyRatio)
{
	EntropyRatio = 0;
	const int& Row = ds.dataMatrix.rows();
	for (int i = 0; i < storage.size(); ++i)
	{
		float ratio = float(storage[i])/float(Row);
		EntropyRatio-=ratio*log2f(ratio);
	}
	EntropyRatio/=log2f(storage.size());
}



/* get norm string */
string AHC::getNormStr()
{
	stringstream ss;
	ss << normOption;
	return ss.str();
}


/* get entropy ratio string */ 
string AHC::getEntropyStr(const float& EntropyRatio)
{
	stringstream ss;
	ss << EntropyRatio;
	return ss.str();
}	


/* set a vector for min-heap */
void AHC::setValue(std::vector<DistNode>& dNodeVec)
{
	const int& Row = ds.dataMatrix.rows();
	dNodeVec = std::vector<DistNode>(Row*(Row-1)/2);
	int tag = 0;
	for(int i=0;i<Row-1;++i)
	{
		for(int j=i+1;j<Row;++j)
		{
			dNodeVec[tag].first = i;
			dNodeVec[tag].second = j;
			if(distanceMatrix)
				dNodeVec[tag].distance = distanceMatrix[i][j];
			else
				dNodeVec[tag].distance = getDisimilarity(ds.dataMatrix, i, j, normOption, object);
			++tag;
		}
	}
	assert(tag==dNodeVec.size());
}

