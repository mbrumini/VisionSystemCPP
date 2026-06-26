#include "processing/thread/ThreadProfileMeasurer.h"

#include "processing/thread/ThreadProfileRootAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
MeasurementResult makeThreadMeasurement(const QString& id,
                                        const QString& alias,
                                        const QString& type,
                                        double valuePixels,
                                        const ThreadMeasurementLimits& limits,
                                        const CameraMeasurementCalibration& calibration,
                                        int sampleCount = 0,
                                        int pointCount = 0,
                                        const QString& diagnostic = {})
{
  MeasurementResult result;
  result.id = id;
  result.alias = alias;
  result.type = type;
  result.sourceAId = QStringLiteral("thread");
  result.valuePixels = valuePixels;
  result.valid = valuePixels > 0.0;
  result.nominal = limits.nominal;
  result.min = limits.min;
  result.max = limits.max;
  result.hasNominal = limits.hasNominal;
  result.hasMin = limits.hasMin;
  result.hasMax = limits.hasMax;
  result.sampleCount = sampleCount;
  result.pointCount = pointCount;
  result.diagnostic = diagnostic;

  if (calibration.enabled &&
      calibration.pixelSizeXMm > 0.000001 &&
      calibration.pixelSizeYMm > 0.000001)
  {
    const double pixelSizeMm = (calibration.pixelSizeXMm + calibration.pixelSizeYMm) * 0.5;
    result.valueReal = valuePixels * pixelSizeMm;
    result.unit = QStringLiteral("mm");
    result.hasRealValue = true;
  }
  else
  {
    result.unit = QStringLiteral("px");
  }

  if (result.valid && result.hasRealValue && (result.hasMin || result.hasMax))
  {
    const bool belowMin = result.hasMin && result.valueReal < result.min;
    const bool aboveMax = result.hasMax && result.valueReal > result.max;
    result.judgement = (belowMin || aboveMax) ? QStringLiteral("NOK") : QStringLiteral("OK");
  }
  else if (!result.valid)
  {
    result.judgement = QStringLiteral("N/D");
  }

  return result;
}
}

ThreadDiameterValues ThreadProfileMeasurer::measureDiameters(const ThreadProfileResult& result) const
{
  ThreadDiameterValues values;
  const ThreadRootAnalysis analysis = ThreadProfileRootAnalyzer().analyze(result);
  values.grooves = analysis.grooves;
  values.crests = analysis.crests;
  values.grooveCount = analysis.grooveCount;
  values.sampleCount = analysis.sampleCount;
  values.topCrestCandidateCount = analysis.topCrestCandidateCount;
  values.bottomCrestCandidateCount = analysis.bottomCrestCandidateCount;
  values.topGrooveCandidateCount = analysis.topGrooveCandidateCount;
  values.bottomGrooveCandidateCount = analysis.bottomGrooveCandidateCount;
  if (!analysis.failureReason.isEmpty())
  {
    values.diagnostic =
      QStringLiteral("%1 samples=%2 topCrest=%3 bottomCrest=%4 topGroove=%5 bottomGroove=%6 validColumns=%7 warped=%8x%9")
        .arg(analysis.failureReason)
        .arg(analysis.sampleCount)
        .arg(analysis.topCrestCandidateCount)
        .arg(analysis.bottomCrestCandidateCount)
        .arg(analysis.topGrooveCandidateCount)
        .arg(analysis.bottomGrooveCandidateCount)
        .arg(result.validColumns)
        .arg(result.warpedWidth)
        .arg(result.warpedHeight);
  }
  if (!analysis.valid)
  {
    return values;
  }

  values.majorPx = analysis.majorPx;
  values.minorPx = analysis.minorPx;
  values.middlePx = analysis.middlePx;
  values.pitchPx = analysis.pitchPx;
  values.phasePx = analysis.phasePx;
  values.valid = true;
  return values;
}

QVector<MeasurementResult> ThreadProfileMeasurer::buildMeasurementResults(
  const ThreadInspectionSettings& settings,
  const ThreadDiameterValues& diameters,
  const CameraMeasurementCalibration& calibration) const
{
  QVector<MeasurementResult> results;
  if (!settings.enabled || !settings.hasExtractionRoi)
  {
    return results;
  }

  const auto append = [&](const QString& id,
                          const QString& alias,
                          const QString& type,
                          const ThreadMeasurementLimits& limits,
                          double valuePixels,
                          int sampleCount = 0,
                          int pointCount = 0) {
    if (!limits.enabled)
    {
      return;
    }

    MeasurementResult measurement =
      makeThreadMeasurement(
        id,
        alias,
        type,
        diameters.valid ? valuePixels : 0.0,
        limits,
        calibration,
        sampleCount,
        pointCount,
        diameters.valid ? QString() : diameters.diagnostic);
    if (!diameters.valid)
    {
      measurement.valid = false;
      measurement.judgement = QStringLiteral("N/D");
      measurement.sampleCount = diameters.sampleCount;
      measurement.pointCount = pointCount;
    }
    results.append(measurement);
  };

  append(QStringLiteral("thread_major"),
         settings.majorDiameter.alias.isEmpty() ? QStringLiteral("Diametro esterno") : settings.majorDiameter.alias,
         QStringLiteral("thread_major_diameter"),
         settings.majorDiameter,
         diameters.majorPx,
         static_cast<int>(diameters.crests.size()),
         static_cast<int>(diameters.crests.size() * 2));
  append(QStringLiteral("thread_minor"),
         settings.minorDiameter.alias.isEmpty() ? QStringLiteral("Diametro interno") : settings.minorDiameter.alias,
         QStringLiteral("thread_minor_diameter"),
         settings.minorDiameter,
         diameters.minorPx,
         static_cast<int>(diameters.grooves.size()),
         static_cast<int>(diameters.grooves.size() * 2));

  if (diameters.valid && diameters.middlePx > 0.0)
  {
    ThreadMeasurementLimits middleLimits;
    middleLimits.enabled = true;
    MeasurementResult middle = makeThreadMeasurement(
      QStringLiteral("thread_pitch_diameter"),
      QStringLiteral("Diametro medio filetto"),
      QStringLiteral("thread_pitch_diameter"),
      diameters.middlePx,
      middleLimits,
      calibration,
      static_cast<int>(std::min(diameters.crests.size(), diameters.grooves.size())),
      static_cast<int>((diameters.crests.size() + diameters.grooves.size()) * 2));
    results.append(middle);
  }

  append(QStringLiteral("thread_pitch"),
         settings.pitchLength.alias.isEmpty() ? QStringLiteral("Passo filetto") : settings.pitchLength.alias,
         QStringLiteral("thread_pitch"),
         settings.pitchLength,
         diameters.pitchPx);
  append(QStringLiteral("thread_phase"),
         settings.phaseOffset.alias.isEmpty() ? QStringLiteral("Fase filetto") : settings.phaseOffset.alias,
         QStringLiteral("thread_phase"),
         settings.phaseOffset,
         diameters.phasePx);

  return results;
}
