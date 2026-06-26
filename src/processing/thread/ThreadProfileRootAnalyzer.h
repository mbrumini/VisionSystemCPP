#pragma once

#include "processing/thread/ThreadProfileExtractor.h"

#include <QString>

#include <vector>

struct ThreadGroovePoint
{
  size_t columnIndex = 0;
  double alongPx = 0.0;
  double topY = 0.0;
  double bottomY = 0.0;
  double diameterPx = 0.0;
  double imageTopX = 0.0;
  double imageTopY = 0.0;
  double imageBottomX = 0.0;
  double imageBottomY = 0.0;
};

struct ThreadCrestPoint
{
  size_t columnIndex = 0;
  double alongPx = 0.0;
  double topY = 0.0;
  double bottomY = 0.0;
  double diameterPx = 0.0;
  double imageTopX = 0.0;
  double imageTopY = 0.0;
  double imageBottomX = 0.0;
  double imageBottomY = 0.0;
};

struct ThreadRootAnalysis
{
  bool valid = false;
  double majorPx = 0.0;
  double minorPx = 0.0;
  double middlePx = 0.0;
  double pitchPx = 0.0;
  double phasePx = 0.0;
  int grooveCount = 0;
  int sampleCount = 0;
  int topCrestCandidateCount = 0;
  int bottomCrestCandidateCount = 0;
  int topGrooveCandidateCount = 0;
  int bottomGrooveCandidateCount = 0;
  QString failureReason;
  std::vector<ThreadGroovePoint> grooves;
  std::vector<ThreadCrestPoint> crests;
};

class ThreadProfileRootAnalyzer
{
public:
  ThreadRootAnalysis analyze(const ThreadProfileResult& result) const;
};
