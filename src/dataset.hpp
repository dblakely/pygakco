#ifndef DATASET_H
#define DATASET_H

#include <string>
#include "shared.h"

class Dataset {
public:
	std::string filename;
	std::string dictFileName;
	bool is_test_set;
	char *dict;
	int dictionarySize;
	int *seqLabels = (int *) malloc(MAXNSTR * sizeof(int));
	int *seqLengths = (int *) malloc(MAXNSTR * sizeof(int));
	long int nStr = 0;
	long int maxlen = 0;
	long int minlen = STRMAXLEN;
	int **S;

	Dataset(std::string filename, bool is_test_set, char *dict, std::string dictFileName);
	void collect_data(bool quiet);
	void readDict();
	void parseDict();
	void free_strings();
};

#endif