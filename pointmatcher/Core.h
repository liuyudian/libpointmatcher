// kate: replace-tabs off; indent-width 4; indent-mode normal
// vim: ts=4:sw=4:noexpandtab
/*

Copyright (c) 2010--2011,
François Pomerleau and Stephane Magnenat, ASL, ETHZ, Switzerland
You can contact the authors at <f dot pomerleau at gmail dot com> and
<stephane at magnenat dot net>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ETH-ASL BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef __POINTMATCHER_CORE_H
#define __POINTMATCHER_CORE_H

#ifndef EIGEN_USE_NEW_STDVECTOR
#define EIGEN_USE_NEW_STDVECTOR
#endif // EIGEN_USE_NEW_STDVECTOR
#define EIGEN2_SUPPORT
#include "Eigen/StdVector"
#include "Eigen/Eigen"
#include "Eigen/Geometry"
#include "nabo/nabo.h"
#include <stdexcept>
#include <limits>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdint.h>

#include "Histogram.h"
#include "Timer.h"

template<typename T>
struct PointMatcher
{
	typedef T ScalarType;
	typedef typename Eigen::Array<T, Eigen::Dynamic, 1> LineArray;
	typedef typename Eigen::Matrix<T, Eigen::Dynamic, 1> Vector;
	typedef typename Eigen::Matrix<T, 3, 1> Vector3;
	typedef std::vector<Vector, Eigen::aligned_allocator<Vector> > VectorVector;
	typedef typename Eigen::Quaternion<T> Quaternion;
	typedef std::vector<Quaternion, Eigen::aligned_allocator<Quaternion> > QuaternionVector;
	typedef typename Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> Matrix;
	typedef typename Eigen::Matrix<T, 3, 3> Matrix3;
	typedef typename Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> IntMatrix;
	typedef typename Nabo::NearestNeighbourSearch<T> NNS;
	typedef typename NNS::SearchType NNSearchType;
	
	typedef Matrix TransformationParameters;
	
	// input types
	struct DataPoints
	{
		typedef Matrix Features;
		typedef Matrix Descriptors;
		
		struct Label
		{
			std::string text;
			size_t span;
			Label(const std::string& text = "", const size_t span = 0):text(text), span(span) {}
		};
		typedef std::vector<Label> Labels;
		
		// Constructor
		DataPoints() {}
		// Constructor
		DataPoints(const Features& features, const Labels& featureLabels);
		// Constructor
		DataPoints(const Features& features, const Labels& featureLabels, const Descriptors& descriptors, const Labels& descriptorLabels);
		
		// Get descriptor by name
		// Return a matrix containing only the resquested descriptor
		Descriptors getDescriptorByName(const std::string& name) const;
		
		Features features;
		Labels featureLabels;
		Descriptors descriptors;
		Labels descriptorLabels;
	};
	
	// exception if point matcher does not converge
	struct ConvergenceError: std::runtime_error
	{
		ConvergenceError(const std::string& reason):runtime_error(reason) {}
	};
	
	// intermediate types
	
	struct Matches
	{
		typedef Matrix Dists;
		typedef IntMatrix Ids;
		// FIXME: shouldn't we have a matrix of pairs instead?
	
		Matches() {}
		Matches(const Dists& dists, const Ids ids);
		
		Dists dists;
		Ids ids;
		
		T getDistsQuantile(const T quantile) const;
	};

	typedef Matrix OutlierWeights;
	
	// type of processing bricks
	
	struct Transformation
	{
		virtual ~Transformation() {}
		virtual DataPoints compute(const DataPoints& input, const TransformationParameters& parameters) const = 0;
	};
	struct Transformations: public std::vector<Transformation*>
	{
		void apply(DataPoints& cloud, const TransformationParameters& parameters) const;
	};
	typedef typename Transformations::iterator TransformationsIt;
	typedef typename Transformations::const_iterator TransformationsConstIt;
	
	#include "Transformations.h"
	
	// ---------------------------------
	
	struct DataPointsFilter
	{
		virtual ~DataPointsFilter() {}
		virtual void init() {}
		virtual DataPoints filter(const DataPoints& input, bool& iterate) = 0;
	};
	
	struct DataPointsFilters: public std::vector<DataPointsFilter*>
	{
		void init();
		void apply(DataPoints& cloud, bool iterate);
	};
	typedef typename DataPointsFilters::iterator DataPointsFiltersIt;
	typedef typename DataPointsFilters::const_iterator DataPointsFiltersConstIt;
	
	#include "DataPointsFilters.h"
	
	// ---------------------------------
	
	struct Matcher
	{
		// FIXME: this is a rather ugly way to do stats
		unsigned long visitCounter;
		
		Matcher():visitCounter(0) {}
		
		void resetVisitCount() { visitCounter = 0; }
		unsigned long getVisitCount() const { return visitCounter; }
		virtual ~Matcher() {}
		virtual void init(const DataPoints& filteredReference, bool& iterate) = 0;
		virtual Matches findClosests(const DataPoints& filteredReading, const DataPoints& filteredReference, bool& iterate) = 0;
	};
	
	#include "Matchers.h"	
	
	// ---------------------------------
	
	struct FeatureOutlierFilter
	{
		virtual ~FeatureOutlierFilter() {}
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate) = 0;
	};
	
	struct DescriptorOutlierFilter
	{
		virtual ~DescriptorOutlierFilter() {}
		virtual OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate) = 0;
	};
	
	// Vector outlier filters
	template<typename F>
	struct OutlierFilters: public std::vector<F*>
	{
		typedef std::vector<F*> Vector;
		OutlierWeights compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const Matches& input, bool& iterate) const;
	};
	
	typedef OutlierFilters<FeatureOutlierFilter> FeatureOutlierFilters;
	typedef OutlierFilters<DescriptorOutlierFilter> DescriptorOutlierFilters;
	typedef typename FeatureOutlierFilters::const_iterator FeatureOutlierFiltersConstIt;
	typedef typename FeatureOutlierFilters::iterator FeatureOutlierFiltersIt;
	typedef typename DescriptorOutlierFilters::const_iterator DescriptorOutlierFiltersConstIt;
	typedef typename DescriptorOutlierFilters::iterator DescriptorOutlierFiltersIt;
	
	#include "OutlierFilters.h"

	// ---------------------------------
	
	struct ErrorMinimizer
	{
		struct ErrorElements
		{
			DataPoints reading;
			DataPoints reference;
			OutlierWeights weights;
			Matches matches;

			ErrorElements(const DataPoints& reading, const DataPoints reference, const OutlierWeights weights, const Matches matches);
		};
		
		ErrorMinimizer():pointUsedRatio(-1.),weightedPointUsedRatio(-1.) {}
		virtual ~ErrorMinimizer() {}
		virtual TransformationParameters compute(const DataPoints& filteredReading, const DataPoints& filteredReference, const OutlierWeights& outlierWeights, const Matches& matches, bool& iterate) = 0;
		T getPointUsedRatio() const { return pointUsedRatio; }
		T getWeightedPointUsedRatio() const { return weightedPointUsedRatio; }
		
	protected:
		// helper functions
		Matrix crossProduct(const Matrix& A, const Matrix& B);
		ErrorElements getMatchedPoints(const DataPoints& reading, const DataPoints& reference, const Matches& matches, const OutlierWeights& outlierWeights);
		
	protected:
		T pointUsedRatio;
		T weightedPointUsedRatio;
	};
	
	#include "ErrorMinimizers.h"
	
	// ---------------------------------
	
	struct TransformationChecker
	{
		Vector limits;
		Vector values;
		std::vector<std::string> limitNames;
		std::vector<std::string> valueNames;

		virtual ~TransformationChecker() {}
		virtual void init(const TransformationParameters& parameters, bool& iterate) = 0;
		virtual void check(const TransformationParameters& parameters, bool& iterate) = 0;
		
		static Vector matrixToAngles(const TransformationParameters& parameters);
	};
	
	// Vector of transformation checker
	struct TransformationCheckers: public std::vector<TransformationChecker*>
	{
		void init(const TransformationParameters& parameters, bool& iterate);
		void check(const TransformationParameters& parameters, bool& iterate);
	};
	typedef typename TransformationCheckers::iterator TransformationCheckersIt;
	typedef typename TransformationCheckers::const_iterator TransformationCheckersConstIt;

	#include "TransformationCheckers.h"

	// ---------------------------------
	
	struct Inspector
	{
		virtual void init() {};
		virtual void dumpFilteredReference(const DataPoints& filteredReference) {}
		virtual void dumpIteration(const size_t iterationCount, const TransformationParameters& parameters, const DataPoints& filteredReference, const DataPoints& reading, const Matches& matches, const OutlierWeights& featureOutlierWeights, const OutlierWeights& descriptorOutlierWeights, const TransformationCheckers& transformationCheckers) {}
		virtual void finish(const size_t iterationCount) {}
		virtual ~Inspector() {}
	};
	
	#include "Inspectors.h"

	// ---------------------------------
	
	// stuff common to all ICP algorithms
	struct ICPChainBase
	{
	public:
		DataPointsFilters readingDataPointsFilters;
		DataPointsFilters readingStepDataPointsFilters;
		DataPointsFilters keyframeDataPointsFilters;
		Transformations transformations;
		Matcher* matcher;
		FeatureOutlierFilters featureOutlierFilters;
		DescriptorOutlierFilters descriptorOutlierFilters;
		ErrorMinimizer* errorMinimizer;
		TransformationCheckers transformationCheckers;
		Inspector* inspector;
		T outlierMixingWeight;
		
		virtual ~ICPChainBase();
		
		//! Construct an ICP algorithm that works in most of the cases
		virtual void setDefault();
		
	protected:
		//! Protected contstructor, to prevent the creation of this object
		ICPChainBase();
		//! Clean chain up, empty all filters and delete associated objects
		void cleanup();
	};
	
	// ICP algorithm
	struct ICP: ICPChainBase
	{
		ICP();
		~ICP();
		
		TransformationParameters operator()(
			const DataPoints& readingIn,
			const DataPoints& referenceIn);

		TransformationParameters operator()(
			const DataPoints& readingIn,
			const DataPoints& referenceIn,
			const TransformationParameters& initialTransformationParameters);
		
		TransformationParameters compute(
			const DataPoints& readingIn,
			const DataPoints& referenceIn,
			const TransformationParameters& initialTransformationParameters);
	};
	
	// ICP sequence, with keyframing
	struct ICPSequence: ICPChainBase
	{
		T ratioToSwitchKeyframe;
		
		Histogram<double> keyFrameDuration;
		Histogram<double> convergenceDuration;
		Histogram<unsigned> iterationsCount;
		Histogram<unsigned> pointCountIn;
		Histogram<unsigned> pointCountReading;
		Histogram<unsigned> pointCountKeyFrame;
		Histogram<unsigned> pointCountTouched;
		Histogram<double> overlapRatio;
		
		// TODO: dim should be removed
		ICPSequence(const int dim, const std::string& filePrefix = "", const bool dumpStdErrOnExit = false);
		~ICPSequence();
		
		TransformationParameters operator()(const DataPoints& inputCloudIn);
	
		TransformationParameters getTransform() const { return keyFrameTransform * curTransform; }
		TransformationParameters getDeltaTransform() const { return lastTransformInv * getTransform(); }
		bool keyFrameCreatedAtLastCall() const { return keyFrameCreated; }
		bool hasKeyFrame() const { return (keyFrameCloud.features.cols() != 0); }
		
		virtual void setDefault();
		
		//! Drop current key frame, create a new one with inputCloud, reset transformations
		void resetTracking(DataPoints& inputCloud);
		
	private:
		bool keyFrameCreated; //!< true if the key frame was created at least once
		DataPoints keyFrameCloud; //!< point cloud of the keyframe
		TransformationParameters keyFrameTransform; //!< pose of keyframe
		TransformationParameters keyFrameTransformOffset; //!< offset for centered keyframe
		TransformationParameters curTransform; //!< transform of last frame wrt keyframe (last call to operator())
		TransformationParameters lastTransformInv; //!< inv of previous computed transform (using getTransform())
		
		//! Create a new key frame
		void createKeyFrame(DataPoints& inputCloud);
	};
	
	#include "Functions.h"
	
}; // PointMatcher

template<typename T>
void swapDataPoints(typename PointMatcher<T>::DataPoints& a, typename PointMatcher<T>::DataPoints& b)
{
	a.features.swap(b.features);
	swap(a.featureLabels, b.featureLabels);
	a.descriptors.swap(b.descriptors);
	swap(a.descriptorLabels, b.descriptorLabels);
}

#endif // __POINTMATCHER_CORE_H

