/*
 * superviseddescent: A C++11 implementation of the supervised descent
 *                    optimisation method
 * File: examples/simple_function.cpp
 *
 * Copyright 2014, 2015 Patrik Huber
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "superviseddescent/superviseddescent.hpp"
#include "superviseddescent/regressors.hpp"

#include "opencv2/core/core.hpp"

#include <iostream>
#include <vector>
#include <cassert>

using namespace superviseddescent;
using cv::Mat;
using std::vector;

// Convenience-helper to generate floating point values in a range, similar to std::iota.
template<typename ForwardIterator, typename T>
void strided_iota(ForwardIterator first, ForwardIterator last, T value, T stride)
{
	while (first != last) {
		*first++ = value;
		value += stride;
	}
};

// Overload that returns a cv::Mat directly.
Mat strided_iota(float startInterval, float stepSize, int numValues)
{
	vector<float> values(numValues);
	strided_iota(std::begin(values), std::next(std::begin(values), numValues), startInterval, stepSize);
	return Mat(values, true); // copy the data from the std::vector to the cv::Mat
};

// Convenience-helper to std::transform a cv::Mat. For CV_32FC1 matrices only.
// Returns the result in a new cv::Mat.
template<class Function>
Mat transform(Mat m, Function f)
{
	assert(m.type() == CV_32FC1);
	auto numValues = m.rows;
	Mat x_tr(numValues, 1, CV_32FC1);
	{
		vector<float> values(numValues);
		std::transform(m.begin<float>(), m.end<float>(), begin(values), f);
		x_tr = Mat(values, true);
	}
	return x_tr;
};

// Calculate the normalised least squares residual.
double normalisedLeastSquaresResidual(const Mat& prediction, const Mat& groundtruth)
{
	return cv::norm(prediction, groundtruth, cv::NORM_L2) / cv::norm(groundtruth, cv::NORM_L2);
};

/**
 * This app demonstrates learning of the descent direction from data for
 * the simple function sin(x).
 *
 * It generates test data, ground truth and starting parameters, and then
 * learns a model using ten linear regressors in series.
 * After learning the model, test data with a finer resolution is generated
 * and the model is tested on it.
 */
int main(int argc, char *argv[])
{
	// The function that we're going to use:
	auto h = [](Mat value, size_t, int) { return std::sin(value.at<float>(0)); };
	// Its inverse, which we use to generate labels:
	auto h_inv = [](float value) {
		if (value >= 1.0f) // our upper border of y is 1.0f, but it can be a bit larger due to floating point representation. asin then returns NaN.
			return std::asin(1.0f);
		else
			return std::asin(value);
	};

	// Generate values in the interval [-1:0.2:1]:
	float startInterval = -1.0f;
	float stepSize = 0.2f;
	int numValues = 11; 
	Mat y_tr = strided_iota(startInterval, stepSize, numValues);
	
	// Calculate the inverse function values (the ground truth):
	Mat x_tr = transform(y_tr, h_inv);
	
	// Start at a fixed value x0 = c = 0.5
	Mat x0 = 0.5f * Mat::ones(numValues, 1, CV_32FC1); 

	// Create 10 linear regressors in series, default-constructed (= no regularisation):
	vector<LinearRegressor<>> regressors(10);
	
	SupervisedDescentOptimiser<LinearRegressor<>> supervisedDescentModel(regressors);
	
	// Train the model. We'll also specify an optional callback function:
	std::cout << "Training the model, printing the residual after each learned regressor: " << std::endl;
	auto printResidual = [&](const cv::Mat& currentPredictions) {
		std::cout << normalisedLeastSquaresResidual(currentPredictions, x_tr) << std::endl;
	};
	supervisedDescentModel.train(x_tr, x0, y_tr, h, printResidual);

	// Test the trained model on test data with finer resolution [-1:0.05:1]:
	float startIntervalTest = -1.0f;
	float stepSizeTest = 0.05f;
	int numValuesTest = 41;
	Mat y_ts = strided_iota(startIntervalTest, stepSizeTest, numValuesTest);

	// Calculate the inverse function values (the ground truth) of the test data:
	Mat x_ts_gt = transform(y_ts, h_inv); // the inverse of y_ts

	// Start at a fixed value x0 = c = 0.5
	Mat x0_ts = 0.5f * Mat::ones(numValuesTest, 1, CV_32FC1);

	// Test the learned model on the test data:
	Mat predictions = supervisedDescentModel.test(x0_ts, y_ts, h);
	double testResidual = normalisedLeastSquaresResidual(predictions, x_ts_gt);
	std::cout << "Normalised least squares residual on the test set: " << testResidual << std::endl;
	
	return EXIT_SUCCESS;
}
