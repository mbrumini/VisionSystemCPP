#pragma once

#include "processing/thread/ThreadProfileExtractor.h"
#include "processing/thread/ThreadProfileRootAnalyzer.h"

#include <QString>

#include <vector>

struct ThreadPhaseAngleSample
{
  double alongPx = 0.0;
  double angleDegrees = 0.0;
};

struct ThreadPhaseAnalysis
{
  bool valid = false;
  double phaseDegrees = 0.0;
  int sampleCount = 0;
  QString diagnostic;
  std::vector<ThreadPhaseAngleSample> angleSamples;
};

class ThreadPhaseAnalyzer
{
public:
  ThreadPhaseAnalysis analyze(
    const ThreadProfileResult& result,
    const std::vector<ThreadCrestPoint>& crests) const;
};
