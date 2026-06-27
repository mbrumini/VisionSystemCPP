#pragma once

#include "measurement/MeasurementGeometry.h"
#include "processing/GeometryMeasurementPipeline.h"
#include "processing/thread/ThreadProfileExtractor.h"
#include "processing/thread/ThreadProfileRootAnalyzer.h"
#include "config/RecipeManager.h"

#include <QVector>

struct ThreadDiameterValues
{
  bool valid = false;
  double majorPx = 0.0;
  double minorPx = 0.0;
  double middlePx = 0.0;
  double pitchPx = 0.0;
  double phasePx = 0.0;
  int phaseSampleCount = 0;
  int grooveCount = 0;
  int sampleCount = 0;
  int topCrestCandidateCount = 0;
  int bottomCrestCandidateCount = 0;
  int topGrooveCandidateCount = 0;
  int bottomGrooveCandidateCount = 0;
  QString diagnostic;
  std::vector<ThreadGroovePoint> grooves;
  std::vector<ThreadCrestPoint> crests;
};

class ThreadProfileMeasurer
{
public:
  ThreadDiameterValues measureDiameters(const ThreadProfileResult& result) const;

  QVector<MeasurementResult> buildMeasurementResults(
    const ThreadInspectionSettings& settings,
    const ThreadDiameterValues& diameters,
    const CameraMeasurementCalibration& calibration) const;
};
