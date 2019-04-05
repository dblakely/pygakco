# iGakco-SVM Python Package

[![Build Status](https://travis-ci.org/pybind/igakco.svg?branch=master)](https://travis-ci.org/pybind/igakco)
[![Build status](https://ci.appveyor.com/api/projects/status/57nnxfm4subeug43/branch/master?svg=true)](https://ci.appveyor.com/project/dean0x7d/cmake-example/branch/master)

A software package for the iGakco-SVM algorithm. It uses [pybind11](https://github.com/pybind/pybind11) to provide a Python interface to the C++ backend. SVM optimization is performed with LIBSVM.


## Prerequisites

**On Unix (Linux, OS X)**

* A compiler with C++11 support
* CMake >= 2.8.12

**On Windows**

* Visual Studio 2015 (required for all Python versions, see notes below)
* CMake >= 3.1


## Installation (Linux and MacOS)
**With pip**
```
pip install -i https://test.pypi.org/simple/ igakco-test
```

**From source**

Clone this repository:
```
git clone --recursive https://github.com/dblakely/pygakco.git
```
The `--recursive` flag is to make sure the Pybind11 library is cloned as well. Then run:

`pip install ./igakco`

or

`pip3 install ./igakco`

## Tutorial
Example usage:
```
from igakco import SVM
svm = SVM(g=7, m=5, C=0.7)
svm.fit(train_file="1.1.train.fasta", test_file="1.1.test.fasta", quiet=False, kernel_file="output.txt")
svm.predict("predictions.txt")
```
This will use the provided parameters to build train and test kernel matrices and train an SVM classifier. The `predict` call will write the predicted labels of the provided `test_file` to `predictions.txt`.

## Documentation
Constructor:
* `g` (required)
* `m` (required)
* `C` (optional, default=1.0)
* `nu` (optional, default=0.5)
* `eps` (optional, default = 0.001) - LIBSVM epsilon parameter
* `kernel` (optional, default = 'linear'). Options: linear, gakco, rbf

Fit:
* `train_file` (required)
* `test_file` (required)
* `dict` (optional). A dictionary file for the sequences in the train and test files. Default behavior is to infer the dictionary from the files.
* `quiet` (optional, default=false). Whether to be verbose.
* `kernel_file` (optional). If provided, the kernel matrix will be printed to the provided file. Otherwise, kernel matrix will not be saved.

Predict:
* `predictions_file` (required). File where predictions will be written. Format is one prediction, a single number, per line.

## Special notes for Windows

**Compiler requirements**

Pybind11 requires a C++11 compliant compiler, i.e Visual Studio 2015 on Windows.
This applies to all Python versions, including 2.7. Unlike regular C extension
modules, it's perfectly fine to compile a pybind11 module with a VS version newer
than the target Python's VS version.

**Runtime requirements**

The Visual C++ 2015 redistributable packages are a runtime requirement for this
project. It can be found [here][vs2015_runtime]. If you use the Anaconda Python
distribution, you can add `vs2015_runtime` as a platform-dependent runtime
requirement for you package: see the `conda.recipe/meta.yaml` file in this example.


## License

[FAQ]: 
