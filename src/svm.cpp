#include "svm.hpp"
#include "gakco_core.hpp"
#include "dataset.hpp"
#include "shared.h"
#include "libsvm-code/libsvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <assert.h>
#include <thread>
#include <vector>
#include <iostream>

#define Malloc(type,n) (type *)malloc((n)*sizeof(type))

SVM::SVM(int g, int m, double C, double nu, double eps, 
	std::string kernel) {
	this->g = g;
	this->m = m;
	this->k = g - m;
	this->C = C;
	this->nu = nu;
	this->eps = eps;
	validate_args(g, m);
	if (!kernel.empty()) {
		if (kernel == "linear") {
			this->kernel_type = LINEAR;
			this->kernel_type_name = "linear";
		} else if (kernel == "gakco") {
			this->kernel_type = GAKCO;
			this->kernel_type_name = "gakco";
		} else if (kernel == "rbf"){
			this->kernel_type = RBF;
			this->kernel_type_name = "rbf";
		} else {
			printf("Error: kernel must be: 'linear', 'gakco', or 'rbf'\n");
			exit(1);
		}
	}
}

void SVM::fit(std::string train_file, std::string test_file, 
		std::string dict, bool quiet, std::string kernel_file) {

	this->quiet = quiet;
	int g = this->g;
	int k = this->k;
	int m = this->m;

	if ((this->kernel_type == LINEAR || this->kernel_type == RBF) && test_file.empty()) {
		printf("A test file must be provided for kernel type '%s'\n", this->kernel_type_name.c_str());
		exit(1);
	}

	/* Construct the train and test datasets */
	auto train_data = new Dataset(train_file, false, NULL, dict);
	train_data->collect_data(this->quiet);

	auto test_data = new Dataset(test_file, true, train_data->dict, "");
	test_data->collect_data(this->quiet);

	assert(train_data->dictionarySize == test_data->dictionarySize);

	if (this->g > train_data->minlen) {
		g_greater_than_shortest_err(g, train_data->minlen, train_file);
	}
	if (this->g > test_data->minlen) {
		g_greater_than_shortest_err(g, test_data->minlen, test_file);
	}
	
	long int maxlen_train = train_data->maxlen;
	long int maxlen_test = test_data->maxlen;
	long int minlen_train = train_data->minlen;
	long int minlen_test = test_data->minlen;
	long int n_str_train = train_data->nStr;
	long int n_str_test = test_data->nStr;
	long int total_str = n_str_train + n_str_test;

	this->n_str_train = n_str_train;
	this->n_str_test = n_str_test;
	this->test_labels = test_data->seqLabels;

	int **S = (int **) malloc(total_str * sizeof(int*));
	int *seq_lengths = (int *) malloc(total_str * sizeof(int));
	int *labels = (int *) malloc(total_str * sizeof(int));

	// copy references to train strings
	memcpy(S, train_data->S, n_str_train * sizeof(int*));
	memcpy(seq_lengths, train_data->seqLengths, n_str_train * sizeof(int));
	memcpy(labels, train_data->seqLabels, n_str_train * sizeof(int));

	// copy the references to the test strings
	memcpy(&S[n_str_train], test_data->S, n_str_test * sizeof(int*));
	memcpy(&seq_lengths[n_str_train], test_data->seqLengths, n_str_test * sizeof(int));
	memcpy(&labels[n_str_train], test_data->seqLabels, n_str_test * sizeof(int));
	
	/*Extract g-mers*/
	Features* features = extractFeatures(S, seq_lengths, total_str, g);
	int nfeat = (*features).n;
	int *feat = (*features).features;
	if(!this->quiet)
		printf("g = %d, k = %d, %d features\n", this->g, this->k, nfeat); 

	//now we can free the strings because we have the features
	train_data->free_strings();
	test_data->free_strings();

	//if this gakco is loading a kernel we don't need to calculate it
	// Eamon, need to check with you about how this part works.
	// Leaving it unimplemented for now
	/*
	if (this->loadkernel || this->loadmodel){
		this->kernel_features = features;
		this->nStr = nStr;
		this->labels = label;
		this->load_kernel(this->outputFilename);
		gakco_kernel_matrix = this->kernel;//for the svm train to access it
		return this->kernel;
	}
	*/

	kernel_params params;
	params.g = g;
	params.k = k;
	params.m = m;
	params.n_str_train = n_str_train;
	params.n_str_test = n_str_test;
	params.total_str = total_str;
	params.n_str_pairs = (total_str / 2) * (total_str + 1);
	params.features = features;
	params.dict_size = train_data->dictionarySize;
	params.num_threads = this->num_threads;
	params.num_mutex = this->num_mutex;
	params.quiet = quiet;

	/* Compute the kernel matrix */
	double *K = construct_kernel(&params);
	this->kernel = K;
	
	/* Write kernel matrix to disk if user provided a file name */
	if (!kernel_file.empty()) {
		printf("Writing kernel to %s...\n", kernel_file.c_str());
		FILE *kernelfile = fopen(kernel_file.c_str(), "w");
		for (int i = 0; i < total_str; ++i) {
			for (int j = 0; j < total_str; ++j) {
				fprintf(kernelfile, "%d:%e ", j + 1, tri_access(K,i,j));
			}
			fprintf(kernelfile, "\n");
		}
		fclose(kernelfile);
	}

	/* Train model */
	struct svm_parameter* svm_param = Malloc(svm_parameter, 1);
	svm_param->svm_type = this->svm_type;
	svm_param->kernel_type = this->kernel_type;
	svm_param->nu = this->nu;
	svm_param->cache_size = this->cache_size;
	svm_param->C = this->C;
	svm_param->nr_weight = this->nr_weight;
	svm_param->weight_label = this->weight_label;
	svm_param->weight = this->weight;
	svm_param->shrinking = this->h;
	svm_param->probability = this->probability;
	svm_param->eps = this->eps;
	svm_param->degree = 0;

	struct svm_model *model;
	model = train_model(K, labels, &params, svm_param);
	this->model = model;
}

void SVM::fit_from_arrays(std::vector<std::string> Xtrain, 
		std::vector<int> Ytrain,
		std::vector<std::string> Xtest,
		std::vector<int> Ytest,
		std::string kernel_file) {
	for (auto iter = Xtrain.begin(); iter != Xtrain.end(); iter++) {
		std::string str = *iter;
		std::cout << str << std::endl;
	}
	for (auto iter = Ytrain.begin(); iter != Ytrain.end(); iter++) {
		int label = *iter;
		std::cout << label << std::endl;
	}
	auto train_data = new ArrayDataset(Xtrain, Ytrain);
	train_data->read_data();
    train_data->numericize_seqs();
    char *dict = train_data->dict;
    auto test_data = new TestArrayDataset(Xtest, Ytest, dict);
    test_data->read_data();
    test_data->numericize_seqs();

    if (this->g > train_data->minlen) {
		throw std::runtime_error("g must be less than the shortest training sequence\n");
	}
	if (this->g > test_data->minlen) {
		throw std::runtime_error("g must be less than the shortest test sequence\n");
	}

	long int maxlen_train = train_data->maxlen;
	long int maxlen_test = test_data->maxlen;
	long int minlen_train = train_data->minlen;
	long int minlen_test = test_data->minlen;
	long int n_str_train = train_data->n_str;
	long int n_str_test = test_data->n_str;
	long int total_str = n_str_train + n_str_test;

	this->n_str_train = n_str_train;
	this->n_str_test = n_str_test;
	this->test_labels = test_data->labels.data();

	int **S = (int **) malloc(total_str * sizeof(int*));
	int *seq_lengths = (int *) malloc(total_str * sizeof(int));
	int *labels = (int *) malloc(total_str * sizeof(int));

	int *train_labels = train_data->labels.data();
	int *train_lengths = train_data->seqLengths.data();
	int *test_labels = test_data->labels.data();
	int *test_lengths = test_data->seqLengths.data();	

	// copy references to train strings
	memcpy(S, train_data->S, n_str_train * sizeof(int*));
	memcpy(seq_lengths, train_lengths, n_str_train * sizeof(int));
	memcpy(labels, train_labels, n_str_train * sizeof(int));

	// copy the references to the test strings
	memcpy(&S[n_str_train], test_data->S, n_str_test * sizeof(int*));
	memcpy(&seq_lengths[n_str_train], test_lengths, n_str_test * sizeof(int));
	memcpy(&labels[n_str_train], test_labels, n_str_test * sizeof(int));
	
	/*Extract g-mers*/
	Features* features = extractFeatures(S, seq_lengths, total_str, g);
	int nfeat = (*features).n;
	int *feat = (*features).features;
	if(!this->quiet)
		printf("g = %d, k = %d, %d features\n", this->g, this->k, nfeat); 

	//now we can free the strings because we have the features

	kernel_params params;
	params.g = g;
	params.k = k;
	params.m = m;
	params.n_str_train = n_str_train;
	params.n_str_test = n_str_test;
	params.total_str = total_str;
	params.n_str_pairs = (total_str / 2) * (total_str + 1);
	params.features = features;
	params.dict_size = train_data->dictionarySize;
	params.num_threads = this->num_threads;
	params.num_mutex = this->num_mutex;
	params.quiet = quiet;

	/* Compute the kernel matrix */
	double *K = construct_kernel(&params);
	this->kernel = K;
	
	/* Write kernel matrix to disk if user provided a file name */
	if (!kernel_file.empty()) {
		printf("Writing kernel to %s...\n", kernel_file.c_str());
		FILE *kernelfile = fopen(kernel_file.c_str(), "w");
		for (int i = 0; i < total_str; ++i) {
			for (int j = 0; j < total_str; ++j) {
				fprintf(kernelfile, "%d:%e ", j + 1, tri_access(K,i,j));
			}
			fprintf(kernelfile, "\n");
		}
		fclose(kernelfile);
	}

	/* Train model */
	struct svm_parameter* svm_param = Malloc(svm_parameter, 1);
	svm_param->svm_type = this->svm_type;
	svm_param->kernel_type = this->kernel_type;
	svm_param->nu = this->nu;
	svm_param->cache_size = this->cache_size;
	svm_param->C = this->C;
	svm_param->nr_weight = this->nr_weight;
	svm_param->weight_label = this->weight_label;
	svm_param->weight = this->weight;
	svm_param->shrinking = this->h;
	svm_param->probability = this->probability;
	svm_param->eps = this->eps;
	svm_param->degree = 0;

	struct svm_model *model;
	model = train_model(K, labels, &params, svm_param);
	this->model = model;

}

void SVM::predict(std::string predictions_file) {
	int n_str = this->total_str;
	int n_str_train = this->n_str_train;
	int n_str_test = this->n_str_test;
	printf("Predicting labels for %d sequences...\n", n_str_test);
	double *test_K = construct_test_kernel(n_str_train, n_str_test, this->kernel);
	int *test_labels = this->test_labels;
	printf("Test kernel constructed...\n");

	int num_sv = this->model->nSV[0] + this->model->nSV[1];
	printf("num_sv = %d\n", num_sv);
	struct svm_node *x = Malloc(struct svm_node, n_str_train + 1);
	int correct = 0;
	// aggregators for finding num of pos and neg samples for auc
	int pagg = 0, nagg = 0;
	double* neg = Malloc(double, n_str_test);
	double* pos = Malloc(double, n_str_test);
	int fp = 0, fn = 0; //counters for false postives and negatives
	int labelind = 0;
	for (int i =0; i < 2; i++){
		if (this->model->label[i] == 1)
			labelind = i;
	}

	FILE *labelfile;
	labelfile = fopen(predictions_file.c_str(), "w");

	int svcount = 0;
	for (int i = 0; i < n_str_test; i++) {
		if (this->kernel_type == GAKCO) {
			for (int j = 0; j < n_str_train; j++){
				x[j].index = j + 1;
				x[j].value = 0;
			}
			svcount = 0;
			for (int j = 0; j < n_str_train; j++){
				if (j == this->model->sv_indices[svcount] - 1){
					x[j].value = test_K[i * num_sv + svcount];
					svcount++;
				}
			}
			x[n_str_train].index = -1;
		} else if (this->kernel_type == LINEAR || this->kernel_type == RBF) {
			for (int j = 0; j < n_str_train; j++){
				x[j].index = j + 1;
				x[j].value = test_K[i * n_str_train + j];
			}
			x[n_str_train].index = -1;
		}

		double probs[2];
		double guess = svm_predict_probability(this->model, x, probs);
		if (test_labels[i] > 0) {
			pos[pagg] = probs[labelind];
			pagg += 1;
			if (guess < 0)
				fn++;
		} else {
			neg[nagg] = probs[labelind];
			nagg += 1;
			if (guess > 0)
				fp++;
		}

		fprintf(labelfile, "%d\n", (int)guess);
		//printf("guess = %f and test_labels[%d] = %d\n", guess, i, test_labels[i]);
		if ((guess < 0.0 && test_labels[i] < 0) || (guess > 0.0 && test_labels[i] > 0)){
			correct++;
		}
	}
	printf("\nAccuracy: %f\n", (double)correct / n_str_test);
	double auc = calculate_auc(pos, neg, pagg, nagg);
	printf("auc: %f\n", auc);
	if (!this->quiet) {
		printf("false positive: %d\tfn: %d\n", fp, fn);
		printf("num positive: %d\n", pagg);
		printf("percent positive: %f\n", ((double)pagg/(nagg+pagg)));
	}
}

void SVM::toString() {
	printf("g = %d, m = %d, C = %f, nu = %f, eps = %f, quiet = %d\n",
		this->g, this->m, this->C, this->nu, this->eps, this->quiet);
}