#include "Loss.h"
#include "Math.h"
#include <cmath>

double SquaredLoss(int m, float x[], float y[], float t1, float t2) 
{
	// Basic 2 parameter squared loss function for linear regressions.
	// Hypothesis takes the form of y = t1 + t2 * x
	// This function assumes x and y are of size m and makes no attempt to sanitize input.

	float* output =  new float[m];

	for (int i = 0; i < m; i++)
	{
		output[i] = (t1 + (x[i] * t2)) - y[i];
		output[i] = pow(output[i], 2);
	}

	double sum = double(Sum(output, m));
	delete[](output);
	sum *= double(1.0 / (2.0 * double(m)));

	return sum;
}

tuple OptimizeSquaredLoss(int m, float x[], float y[], int maxIterationsPerParameter)
{
	tuple output = { 0,0 };
	double minimumLoss = double(m);

	double loss = 0.0;

	float t1, t2;

	for ( int i = 0 - int(maxIterationsPerParameter / 2); i < int(maxIterationsPerParameter / 2); i++)
	{
		for (int j = 0 - int(maxIterationsPerParameter / 2); j < int(maxIterationsPerParameter / 2); j++)
		{
			t1 = float(i) / float(10);
			t2 = float(j) / float(10);
			loss = SquaredLoss(m, x, y, t1, t2);
			if (loss < minimumLoss)
			{
				minimumLoss = loss;
				output.x = t1;
				output.y = t2;
			}
		}
	}

	return output;

}