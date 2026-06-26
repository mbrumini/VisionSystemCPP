#include "processing/thread/ThreadProfileRootAnalyzer.h"

#include "processing/thread/ThreadInspectionUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
struct ProfileSample
{
  double alongPx = 0.0;
  double topY = 0.0;
  double bottomY = 0.0;
  double topRadialPx = 0.0;
  double bottomRadialPx = 0.0;
};

void assignImagePoints(
  const ThreadProfileResult& result,
  double alongPx,
  double topY,
  double bottomY,
  double& imageTopX,
  double& imageTopY,
  double& imageBottomX,
  double& imageBottomY)
{
  if (result.warpedWidth < 2 || result.warpedHeight < 2)
  {
    return;
  }

  const cv::Point2f topImage =
    threadWarpedPointToImage(result.imageQuad, alongPx, topY, result.warpedWidth, result.warpedHeight);
  const cv::Point2f bottomImage =
    threadWarpedPointToImage(result.imageQuad, alongPx, bottomY, result.warpedWidth, result.warpedHeight);
  imageTopX = topImage.x;
  imageTopY = topImage.y;
  imageBottomX = bottomImage.x;
  imageBottomY = bottomImage.y;
}

double robustMedian(std::vector<double> values)
{
  if (values.empty())
  {
    return 0.0;
  }

  std::sort(values.begin(), values.end());
  return values[values.size() / 2];
}

double arithmeticMean(const std::vector<double>& values)
{
  if (values.empty())
  {
    return 0.0;
  }

  double sum = 0.0;
  for (double value : values)
  {
    sum += value;
  }
  return sum / static_cast<double>(values.size());
}

std::vector<double> consecutiveSpacings(const std::vector<double>& positions)
{
  std::vector<double> spacings;
  if (positions.size() < 2)
  {
    return spacings;
  }

  spacings.reserve(positions.size() - 1);
  for (size_t i = 1; i < positions.size(); ++i)
  {
    const double spacing = positions[i] - positions[i - 1];
    if (spacing > 0.5)
    {
      spacings.push_back(spacing);
    }
  }
  return spacings;
}

std::vector<double> invertedProfile(const std::vector<double>& values)
{
  std::vector<double> inverted;
  inverted.reserve(values.size());
  for (double value : values)
  {
    inverted.push_back(-value);
  }
  return inverted;
}

bool plausibleThreadDiameter(double diameterPx, const ThreadProfileResult& result)
{
  const double maxDiameterPx = result.warpedHeight > 0
    ? static_cast<double>(result.warpedHeight) * 1.05
    : 10000.0;
  return std::isfinite(diameterPx) && diameterPx > 0.5 && diameterPx <= maxDiameterPx;
}

struct IndexedProminence
{
  int sampleIndex = 0;
  double alongPx = 0.0;
  double prominence = 0.0;
};

double estimatePitchPx(const std::vector<IndexedProminence>& crestPeaks, int fallbackRadius)
{
  if (crestPeaks.size() < 2)
  {
    return static_cast<double>(std::max(12, fallbackRadius * 5));
  }

  std::vector<double> positions;
  positions.reserve(crestPeaks.size());
  for (const IndexedProminence& peak : crestPeaks)
  {
    positions.push_back(peak.alongPx);
  }
  std::sort(positions.begin(), positions.end());

  const double sameApexSpacingPx = static_cast<double>(std::max(3, fallbackRadius * 2));
  std::vector<double> spacings;
  spacings.reserve(positions.size() - 1);
  for (double spacing : consecutiveSpacings(positions))
  {
    if (spacing >= sameApexSpacingPx)
    {
      spacings.push_back(spacing);
    }
  }
  if (spacings.empty())
  {
    return static_cast<double>(std::max(12, fallbackRadius * 5));
  }

  return robustMedian(spacings);
}

std::vector<int> selectSpacedPeaks(std::vector<IndexedProminence> candidates, double minSpacingPx)
{
  std::sort(candidates.begin(), candidates.end(), [](const IndexedProminence& left, const IndexedProminence& right) {
    return left.prominence > right.prominence;
  });

  std::vector<IndexedProminence> selected;
  selected.reserve(candidates.size());
  for (const IndexedProminence& candidate : candidates)
  {
    bool tooClose = false;
    for (const IndexedProminence& kept : selected)
    {
      if (std::abs(candidate.alongPx - kept.alongPx) < minSpacingPx)
      {
        tooClose = true;
        break;
      }
    }
    if (!tooClose)
    {
      selected.push_back(candidate);
    }
  }

  std::sort(selected.begin(), selected.end(), [](const IndexedProminence& left, const IndexedProminence& right) {
    return left.alongPx < right.alongPx;
  });

  std::vector<int> indices;
  indices.reserve(selected.size());
  for (const IndexedProminence& peak : selected)
  {
    indices.push_back(peak.sampleIndex);
  }
  return indices;
}

bool radialMaxProminence(
  const std::vector<double>& radial,
  int index,
  int radius,
  double& prominence)
{
  if (index < radius || index + radius >= static_cast<int>(radial.size()))
  {
    return false;
  }

  const double value = radial[static_cast<size_t>(index)];
  double maxNeighbor = -std::numeric_limits<double>::max();
  double minNeighbor = std::numeric_limits<double>::max();
  for (int offset = -radius; offset <= radius; ++offset)
  {
    if (offset == 0)
    {
      continue;
    }
    const double neighbor = radial[static_cast<size_t>(index + offset)];
    maxNeighbor = std::max(maxNeighbor, neighbor);
    minNeighbor = std::min(minNeighbor, neighbor);
  }

  if (value + 1e-3 < maxNeighbor)
  {
    return false;
  }

  prominence = value - minNeighbor;
  return prominence > 0.0;
}

std::vector<IndexedProminence> collectRadialPeakCandidates(
  const std::vector<ProfileSample>& samples,
  const std::vector<double>& radial,
  int radius,
  double minProminence)
{
  std::vector<IndexedProminence> candidates;
  candidates.reserve(samples.size() / 10);
  if (samples.size() < static_cast<size_t>(radius * 2 + 1))
  {
    return candidates;
  }

  for (int i = radius; i + radius < static_cast<int>(samples.size()); ++i)
  {
    double prominence = 0.0;
    if (radialMaxProminence(radial, i, radius, prominence) && prominence >= minProminence)
    {
      candidates.push_back({i, samples[static_cast<size_t>(i)].alongPx, prominence});
    }
  }
  return candidates;
}

std::vector<IndexedProminence> collectAdaptiveRadialPeakCandidates(
  const std::vector<ProfileSample>& samples,
  const std::vector<double>& radial,
  int radius,
  double minProminence,
  double radialSwingPx)
{
  std::vector<IndexedProminence> candidates =
    collectRadialPeakCandidates(samples, radial, radius, minProminence);
  if (!candidates.empty())
  {
    return candidates;
  }

  const int relaxedRadius = std::clamp(radius / 2, 2, 8);
  const double relaxedProminence = std::max(0.25, radialSwingPx * 0.06);
  return collectRadialPeakCandidates(samples, radial, relaxedRadius, relaxedProminence);
}

std::vector<IndexedProminence> collectProfileExtremaCandidates(
  const std::vector<ProfileSample>& samples,
  bool useTopProfile,
  bool wantMaximum,
  int radius,
  double minProminence)
{
  std::vector<IndexedProminence> candidates;
  candidates.reserve(samples.size() / 10);
  if (samples.size() < static_cast<size_t>(radius * 2 + 1))
  {
    return candidates;
  }

  const auto profileValue = [useTopProfile](const ProfileSample& sample) {
    return useTopProfile ? sample.topY : sample.bottomY;
  };

  for (int i = radius; i + radius < static_cast<int>(samples.size()); ++i)
  {
    const double value = profileValue(samples[static_cast<size_t>(i)]);
    double maxNeighbor = -std::numeric_limits<double>::max();
    double minNeighbor = std::numeric_limits<double>::max();
    bool isExtremum = true;
    for (int offset = -radius; offset <= radius; ++offset)
    {
      if (offset == 0)
      {
        continue;
      }

      const double neighbor = profileValue(samples[static_cast<size_t>(i + offset)]);
      maxNeighbor = std::max(maxNeighbor, neighbor);
      minNeighbor = std::min(minNeighbor, neighbor);
      if (wantMaximum && value + 1e-3 < neighbor)
      {
        isExtremum = false;
      }
      if (!wantMaximum && value - 1e-3 > neighbor)
      {
        isExtremum = false;
      }
    }

    if (!isExtremum)
    {
      continue;
    }

    const double prominence = wantMaximum ? value - minNeighbor : maxNeighbor - value;
    if (prominence >= minProminence)
    {
      candidates.push_back({i, samples[static_cast<size_t>(i)].alongPx, prominence});
    }
  }

  return candidates;
}

std::vector<IndexedProminence> collectSegmentExtremaCandidates(
  const std::vector<ProfileSample>& samples,
  bool useTopProfile,
  bool wantMaximum,
  double segmentWidthPx)
{
  std::vector<IndexedProminence> candidates;
  candidates.reserve(samples.size() / 20);
  if (samples.size() < 16 || segmentWidthPx <= 2.0)
  {
    return candidates;
  }

  const auto profileValue = [useTopProfile](const ProfileSample& sample) {
    return useTopProfile ? sample.topY : sample.bottomY;
  };

  const double firstAlong = samples.front().alongPx;
  const double lastAlong = samples.back().alongPx;
  for (double segmentStart = firstAlong; segmentStart < lastAlong; segmentStart += segmentWidthPx)
  {
    const double segmentEnd = std::min(lastAlong, segmentStart + segmentWidthPx);
    int bestIndex = -1;
    double bestValue = wantMaximum ? -std::numeric_limits<double>::max() : std::numeric_limits<double>::max();
    double minValue = std::numeric_limits<double>::max();
    double maxValue = -std::numeric_limits<double>::max();

    for (int i = 0; i < static_cast<int>(samples.size()); ++i)
    {
      const ProfileSample& sample = samples[static_cast<size_t>(i)];
      if (sample.alongPx < segmentStart || sample.alongPx >= segmentEnd)
      {
        continue;
      }

      const double value = profileValue(sample);
      minValue = std::min(minValue, value);
      maxValue = std::max(maxValue, value);
      if ((wantMaximum && value > bestValue) || (!wantMaximum && value < bestValue))
      {
        bestValue = value;
        bestIndex = i;
      }
    }

    if (bestIndex < 0)
    {
      continue;
    }

    candidates.push_back({
      bestIndex,
      samples[static_cast<size_t>(bestIndex)].alongPx,
      std::max(0.1, maxValue - minValue)
    });
  }

  return candidates;
}

ThreadCrestPoint makeCrestPoint(
  const ThreadProfileResult& result,
  double topAlongPx,
  double topY,
  double bottomAlongPx,
  double bottomCrestY)
{
  ThreadCrestPoint crest;
  crest.columnIndex = static_cast<size_t>(std::llround(topAlongPx));
  crest.alongPx = topAlongPx;
  crest.topY = topY;
  crest.bottomY = bottomCrestY;
  crest.diameterPx = crest.bottomY - crest.topY;
  if (result.warpedWidth >= 2 && result.warpedHeight >= 2)
  {
    const cv::Point2f topImage = threadWarpedPointToImage(
      result.imageQuad, topAlongPx, topY, result.warpedWidth, result.warpedHeight);
    const cv::Point2f bottomImage = threadWarpedPointToImage(
      result.imageQuad, bottomAlongPx, bottomCrestY, result.warpedWidth, result.warpedHeight);
    crest.imageTopX = topImage.x;
    crest.imageTopY = topImage.y;
    crest.imageBottomX = bottomImage.x;
    crest.imageBottomY = bottomImage.y;
  }
  return crest;
}

ThreadGroovePoint makeGroovePoint(
  const ThreadProfileResult& result,
  double topAlongPx,
  double topY,
  double bottomAlongPx,
  double bottomY)
{
  ThreadGroovePoint groove;
  groove.columnIndex = static_cast<size_t>(std::llround(topAlongPx));
  groove.alongPx = topAlongPx;
  groove.topY = topY;
  groove.bottomY = bottomY;
  groove.diameterPx = groove.bottomY - groove.topY;
  if (result.warpedWidth >= 2 && result.warpedHeight >= 2)
  {
    const cv::Point2f topImage = threadWarpedPointToImage(
      result.imageQuad, topAlongPx, topY, result.warpedWidth, result.warpedHeight);
    const cv::Point2f bottomImage = threadWarpedPointToImage(
      result.imageQuad, bottomAlongPx, bottomY, result.warpedWidth, result.warpedHeight);
    groove.imageTopX = topImage.x;
    groove.imageTopY = topImage.y;
    groove.imageBottomX = bottomImage.x;
    groove.imageBottomY = bottomImage.y;
  }
  return groove;
}

double interpolateProfileY(
  const std::vector<ProfileSample>& samples,
  double alongPx,
  bool useTopProfile)
{
  if (samples.empty())
  {
    return 0.0;
  }

  const double clampedAlong = std::clamp(alongPx, samples.front().alongPx, samples.back().alongPx);
  const auto after = std::lower_bound(
    samples.begin(),
    samples.end(),
    clampedAlong,
    [](const ProfileSample& sample, double value) { return sample.alongPx < value; });

  if (after == samples.begin())
  {
    return useTopProfile ? after->topY : after->bottomY;
  }
  if (after == samples.end())
  {
    const ProfileSample& previous = samples.back();
    return useTopProfile ? previous.topY : previous.bottomY;
  }

  const ProfileSample& right = *after;
  const ProfileSample& left = *(after - 1);
  const double span = right.alongPx - left.alongPx;
  if (span <= 1e-6)
  {
    return useTopProfile ? left.topY : left.bottomY;
  }

  const double t = (clampedAlong - left.alongPx) / span;
  const double leftY = useTopProfile ? left.topY : left.bottomY;
  const double rightY = useTopProfile ? right.topY : right.bottomY;
  return leftY + t * (rightY - leftY);
}

double refineParabolicPeakOffset(double leftValue, double centerValue, double rightValue)
{
  const double denominator = leftValue - 2.0 * centerValue + rightValue;
  if (std::abs(denominator) < 1e-6)
  {
    return 0.0;
  }

  return std::clamp(0.5 * (leftValue - rightValue) / denominator, -1.0, 1.0);
}

bool refineTopCrestApex(
  const std::vector<ProfileSample>& samples,
  int approximateIndex,
  int halfWindow,
  double& topAlongPx,
  double& topY)
{
  if (samples.empty())
  {
    return false;
  }

  const int start = std::max(0, approximateIndex - halfWindow);
  const int end = std::min(static_cast<int>(samples.size()) - 1, approximateIndex + halfWindow);
  if (start >= end)
  {
    return false;
  }

  int topIndex = start;
  for (int i = start + 1; i <= end; ++i)
  {
    if (samples[static_cast<size_t>(i)].topY < samples[static_cast<size_t>(topIndex)].topY)
    {
      topIndex = i;
    }
  }

  topAlongPx = samples[static_cast<size_t>(topIndex)].alongPx;
  topY = samples[static_cast<size_t>(topIndex)].topY;
  if (topIndex > start && topIndex < end)
  {
    const double offset = refineParabolicPeakOffset(
      samples[static_cast<size_t>(topIndex - 1)].topY,
      samples[static_cast<size_t>(topIndex)].topY,
      samples[static_cast<size_t>(topIndex + 1)].topY);
    topAlongPx += offset;
    topY = interpolateProfileY(samples, topAlongPx, true);
  }

  return true;
}

bool refineBottomCrestApex(
  const std::vector<ProfileSample>& samples,
  double expectedAlongPx,
  int halfWindow,
  double& bottomAlongPx,
  double& bottomCrestY)
{
  if (samples.empty())
  {
    return false;
  }

  const auto nearest = std::lower_bound(
    samples.begin(),
    samples.end(),
    expectedAlongPx,
    [](const ProfileSample& sample, double value) { return sample.alongPx < value; });

  int centerIndex = 0;
  if (nearest == samples.end())
  {
    centerIndex = static_cast<int>(samples.size()) - 1;
  }
  else if (nearest == samples.begin())
  {
    centerIndex = 0;
  }
  else
  {
    const double rightDistance = nearest->alongPx - expectedAlongPx;
    const double leftDistance = expectedAlongPx - (nearest - 1)->alongPx;
    centerIndex = static_cast<int>(rightDistance < leftDistance ? nearest - samples.begin() : nearest - 1 - samples.begin());
  }

  const int start = std::max(0, centerIndex - halfWindow);
  const int end = std::min(static_cast<int>(samples.size()) - 1, centerIndex + halfWindow);
  if (start >= end)
  {
    return false;
  }

  int bottomIndex = start;
  for (int i = start + 1; i <= end; ++i)
  {
    if (samples[static_cast<size_t>(i)].bottomY > samples[static_cast<size_t>(bottomIndex)].bottomY)
    {
      bottomIndex = i;
    }
  }

  bottomAlongPx = samples[static_cast<size_t>(bottomIndex)].alongPx;
  bottomCrestY = samples[static_cast<size_t>(bottomIndex)].bottomY;
  if (bottomIndex > start && bottomIndex < end)
  {
    const double offset = refineParabolicPeakOffset(
      samples[static_cast<size_t>(bottomIndex - 1)].bottomY,
      samples[static_cast<size_t>(bottomIndex)].bottomY,
      samples[static_cast<size_t>(bottomIndex + 1)].bottomY);
    bottomAlongPx += offset;
    bottomCrestY = interpolateProfileY(samples, bottomAlongPx, false);
  }

  return true;
}

bool refineTopGrooveApex(
  const std::vector<ProfileSample>& samples,
  int approximateIndex,
  int halfWindow,
  double& topAlongPx,
  double& topY)
{
  if (samples.empty())
  {
    return false;
  }

  const int start = std::max(0, approximateIndex - halfWindow);
  const int end = std::min(static_cast<int>(samples.size()) - 1, approximateIndex + halfWindow);
  if (start >= end)
  {
    return false;
  }

  int topIndex = start;
  for (int i = start + 1; i <= end; ++i)
  {
    if (samples[static_cast<size_t>(i)].topY > samples[static_cast<size_t>(topIndex)].topY)
    {
      topIndex = i;
    }
  }

  topAlongPx = samples[static_cast<size_t>(topIndex)].alongPx;
  topY = samples[static_cast<size_t>(topIndex)].topY;
  if (topIndex > start && topIndex < end)
  {
    const double offset = refineParabolicPeakOffset(
      samples[static_cast<size_t>(topIndex - 1)].topY,
      samples[static_cast<size_t>(topIndex)].topY,
      samples[static_cast<size_t>(topIndex + 1)].topY);
    topAlongPx += offset;
    topY = interpolateProfileY(samples, topAlongPx, true);
  }

  return true;
}

bool refineBottomGrooveApex(
  const std::vector<ProfileSample>& samples,
  int approximateIndex,
  int halfWindow,
  double& bottomAlongPx,
  double& bottomY)
{
  if (samples.empty())
  {
    return false;
  }

  const int start = std::max(0, approximateIndex - halfWindow);
  const int end = std::min(static_cast<int>(samples.size()) - 1, approximateIndex + halfWindow);
  if (start >= end)
  {
    return false;
  }

  int bottomIndex = start;
  for (int i = start + 1; i <= end; ++i)
  {
    if (samples[static_cast<size_t>(i)].bottomY < samples[static_cast<size_t>(bottomIndex)].bottomY)
    {
      bottomIndex = i;
    }
  }

  bottomAlongPx = samples[static_cast<size_t>(bottomIndex)].alongPx;
  bottomY = samples[static_cast<size_t>(bottomIndex)].bottomY;
  if (bottomIndex > start && bottomIndex < end)
  {
    const double offset = refineParabolicPeakOffset(
      -samples[static_cast<size_t>(bottomIndex - 1)].bottomY,
      -samples[static_cast<size_t>(bottomIndex)].bottomY,
      -samples[static_cast<size_t>(bottomIndex + 1)].bottomY);
    bottomAlongPx += offset;
    bottomY = interpolateProfileY(samples, bottomAlongPx, false);
  }

  return true;
}

double estimateTopBottomPhaseOffsetPx(
  const std::vector<ProfileSample>& samples,
  const std::vector<double>& topAlongPositions,
  const std::vector<double>& bottomRadial,
  double pitchPx,
  int searchRadius)
{
  if (topAlongPositions.empty() || pitchPx <= 0.5)
  {
    return 0.0;
  }

  std::vector<IndexedProminence> bottomCandidates;
  bottomCandidates.reserve(samples.size() / 10);
  const double minProminence = std::max(0.5, (*std::max_element(bottomRadial.begin(), bottomRadial.end()) -
                                                 *std::min_element(bottomRadial.begin(), bottomRadial.end())) *
                                                0.15);
  for (int i = searchRadius; i + searchRadius < static_cast<int>(samples.size()); ++i)
  {
    double prominence = 0.0;
    if (radialMaxProminence(bottomRadial, i, searchRadius, prominence) && prominence >= minProminence)
    {
      bottomCandidates.push_back({i, samples[static_cast<size_t>(i)].alongPx, prominence});
    }
  }

  if (bottomCandidates.empty())
  {
    return 0.0;
  }

  std::vector<double> phaseOffsets;
  phaseOffsets.reserve(topAlongPositions.size());
  for (double topAlongPx : topAlongPositions)
  {
    double bestOffset = 0.0;
    double bestProminence = -1.0;
    for (const IndexedProminence& candidate : bottomCandidates)
    {
      const double bottomAlongPx = samples[static_cast<size_t>(candidate.sampleIndex)].alongPx;
      const double offset = bottomAlongPx - topAlongPx;
      if (offset < -pitchPx * 0.15 || offset > pitchPx * 0.95)
      {
        continue;
      }

      if (candidate.prominence > bestProminence)
      {
        bestProminence = candidate.prominence;
        bestOffset = offset;
      }
    }

    if (bestProminence > 0.0)
    {
      phaseOffsets.push_back(bestOffset);
    }
  }

  if (phaseOffsets.empty())
  {
    return 0.0;
  }

  return robustMedian(phaseOffsets);
}
}

ThreadRootAnalysis ThreadProfileRootAnalyzer::analyze(const ThreadProfileResult& result) const
{
  ThreadRootAnalysis analysis;
  if (!result.valid)
  {
    analysis.failureReason = result.error.isEmpty() ? QStringLiteral("invalid_profile") : result.error;
    return analysis;
  }

  std::vector<ProfileSample> samples;
  samples.reserve(result.columns.size());

  for (size_t index = 0; index < result.columns.size(); ++index)
  {
    const ThreadProfileColumn& column = result.columns[index];
    if (!column.valid)
    {
      continue;
    }

    samples.push_back({column.warpedAlong, column.warpedCrestY, column.warpedRootY, 0.0, 0.0});
  }

  if (samples.size() < 16)
  {
    analysis.sampleCount = static_cast<int>(samples.size());
    analysis.failureReason = QStringLiteral("insufficient_samples");
    return analysis;
  }
  analysis.sampleCount = static_cast<int>(samples.size());

  std::vector<double> axisCandidates;
  axisCandidates.reserve(samples.size());
  for (const ProfileSample& sample : samples)
  {
    axisCandidates.push_back(0.5 * (sample.topY + sample.bottomY));
  }
  const double axisY = robustMedian(std::move(axisCandidates));

  std::vector<double> topRadial;
  std::vector<double> bottomRadial;
  topRadial.reserve(samples.size());
  bottomRadial.reserve(samples.size());
  for (ProfileSample& sample : samples)
  {
    sample.topRadialPx = axisY - sample.topY;
    sample.bottomRadialPx = sample.bottomY - axisY;
    topRadial.push_back(sample.topRadialPx);
    bottomRadial.push_back(sample.bottomRadialPx);
  }

  const double maxTopRadial = *std::max_element(topRadial.begin(), topRadial.end());
  const double minTopRadial = *std::min_element(topRadial.begin(), topRadial.end());
  const double radialSwingPx = std::max(1.0, maxTopRadial - minTopRadial);

  const int searchRadius = std::clamp(static_cast<int>(samples.size()) / 60, 5, 18);
  const double minProminence = std::max(0.8, radialSwingPx * 0.2);

  std::vector<IndexedProminence> crestCandidates =
    collectAdaptiveRadialPeakCandidates(samples, topRadial, searchRadius, minProminence, radialSwingPx);
  if (crestCandidates.size() < 2)
  {
    crestCandidates = collectProfileExtremaCandidates(
      samples,
      true,
      false,
      std::clamp(searchRadius / 2, 2, 8),
      std::max(0.25, radialSwingPx * 0.06));
  }
  if (crestCandidates.size() < 2)
  {
    crestCandidates = collectSegmentExtremaCandidates(
      samples,
      true,
      false,
      std::clamp(static_cast<double>(samples.size()) / 20.0, 18.0, 80.0));
  }
  analysis.topCrestCandidateCount = static_cast<int>(crestCandidates.size());

  const double pitchEstimatePx = estimatePitchPx(crestCandidates, searchRadius);
  const double minPeakSpacingPx = std::max(10.0, pitchEstimatePx * 0.45);
  const int refineHalfWindow = std::clamp(static_cast<int>(std::lround(pitchEstimatePx * 0.35)), 4, 24);
  const std::vector<int> crestSampleIndices = selectSpacedPeaks(std::move(crestCandidates), minPeakSpacingPx);

  struct RefinedTopCrest
  {
    double alongPx = 0.0;
    double topY = 0.0;
  };
  struct RefinedBottomCrest
  {
    double alongPx = 0.0;
    double bottomY = 0.0;
  };
  struct RefinedTopGroove
  {
    double alongPx = 0.0;
    double topY = 0.0;
  };
  struct RefinedBottomGroove
  {
    double alongPx = 0.0;
    double bottomY = 0.0;
  };

  std::vector<RefinedTopCrest> refinedTopCrests;
  refinedTopCrests.reserve(crestSampleIndices.size());
  for (int crestSample : crestSampleIndices)
  {
    RefinedTopCrest crest;
    if (!refineTopCrestApex(samples, crestSample, refineHalfWindow, crest.alongPx, crest.topY))
    {
      continue;
    }

    refinedTopCrests.push_back(crest);
  }

  if (refinedTopCrests.empty())
  {
    analysis.failureReason = QStringLiteral("no_top_crests");
    return analysis;
  }

  std::vector<IndexedProminence> bottomCandidates =
    collectAdaptiveRadialPeakCandidates(samples, bottomRadial, searchRadius, minProminence, radialSwingPx);
  if (bottomCandidates.size() < 2)
  {
    bottomCandidates = collectProfileExtremaCandidates(
      samples,
      false,
      true,
      std::clamp(searchRadius / 2, 2, 8),
      std::max(0.25, radialSwingPx * 0.06));
  }
  if (bottomCandidates.size() < 2)
  {
    bottomCandidates = collectSegmentExtremaCandidates(
      samples,
      false,
      true,
      std::clamp(static_cast<double>(samples.size()) / 20.0, 18.0, 80.0));
  }
  analysis.bottomCrestCandidateCount = static_cast<int>(bottomCandidates.size());
  const std::vector<int> bottomSampleIndices = selectSpacedPeaks(std::move(bottomCandidates), minPeakSpacingPx);

  std::vector<RefinedBottomCrest> refinedBottomCrests;
  refinedBottomCrests.reserve(bottomSampleIndices.size());
  for (int bottomSample : bottomSampleIndices)
  {
    RefinedBottomCrest crest;
    if (!refineBottomCrestApex(
          samples,
          samples[static_cast<size_t>(bottomSample)].alongPx,
          refineHalfWindow,
          crest.alongPx,
          crest.bottomY))
    {
      continue;
    }

    refinedBottomCrests.push_back(crest);
  }

  if (refinedBottomCrests.empty())
  {
    analysis.failureReason = QStringLiteral("no_bottom_crests");
    return analysis;
  }

  const std::vector<double> invertedTopRadial = invertedProfile(topRadial);
  const std::vector<double> invertedBottomRadial = invertedProfile(bottomRadial);
  std::vector<IndexedProminence> topGrooveCandidates =
    collectAdaptiveRadialPeakCandidates(samples, invertedTopRadial, searchRadius, minProminence, radialSwingPx);
  if (topGrooveCandidates.size() < 2)
  {
    topGrooveCandidates = collectProfileExtremaCandidates(
      samples,
      true,
      true,
      std::clamp(searchRadius / 2, 2, 8),
      std::max(0.25, radialSwingPx * 0.06));
  }
  if (topGrooveCandidates.size() < 2)
  {
    topGrooveCandidates = collectSegmentExtremaCandidates(
      samples,
      true,
      true,
      std::clamp(static_cast<double>(samples.size()) / 20.0, 18.0, 80.0));
  }
  analysis.topGrooveCandidateCount = static_cast<int>(topGrooveCandidates.size());
  std::vector<IndexedProminence> bottomGrooveCandidates =
    collectAdaptiveRadialPeakCandidates(samples, invertedBottomRadial, searchRadius, minProminence, radialSwingPx);
  if (bottomGrooveCandidates.size() < 2)
  {
    bottomGrooveCandidates = collectProfileExtremaCandidates(
      samples,
      false,
      false,
      std::clamp(searchRadius / 2, 2, 8),
      std::max(0.25, radialSwingPx * 0.06));
  }
  if (bottomGrooveCandidates.size() < 2)
  {
    bottomGrooveCandidates = collectSegmentExtremaCandidates(
      samples,
      false,
      false,
      std::clamp(static_cast<double>(samples.size()) / 20.0, 18.0, 80.0));
  }
  analysis.bottomGrooveCandidateCount = static_cast<int>(bottomGrooveCandidates.size());

  const std::vector<int> topGrooveSampleIndices =
    selectSpacedPeaks(std::move(topGrooveCandidates), minPeakSpacingPx);
  const std::vector<int> bottomGrooveSampleIndices =
    selectSpacedPeaks(std::move(bottomGrooveCandidates), minPeakSpacingPx);

  std::vector<RefinedTopGroove> refinedTopGrooves;
  refinedTopGrooves.reserve(topGrooveSampleIndices.size());
  for (int topGrooveSample : topGrooveSampleIndices)
  {
    RefinedTopGroove groove;
    if (refineTopGrooveApex(samples, topGrooveSample, refineHalfWindow, groove.alongPx, groove.topY))
    {
      refinedTopGrooves.push_back(groove);
    }
  }

  std::vector<RefinedBottomGroove> refinedBottomGrooves;
  refinedBottomGrooves.reserve(bottomGrooveSampleIndices.size());
  for (int bottomGrooveSample : bottomGrooveSampleIndices)
  {
    RefinedBottomGroove groove;
    if (refineBottomGrooveApex(samples, bottomGrooveSample, refineHalfWindow, groove.alongPx, groove.bottomY))
    {
      refinedBottomGrooves.push_back(groove);
    }
  }

  std::vector<double> crestDiameters;
  const size_t pairedCount = std::min(refinedTopCrests.size(), refinedBottomCrests.size());
  for (size_t pairIndex = 0; pairIndex < pairedCount; ++pairIndex)
  {
    const RefinedTopCrest& topCrest = refinedTopCrests[pairIndex];
    const RefinedBottomCrest& bottomCrest = refinedBottomCrests[pairIndex];

    if (topCrest.topY + 1e-3 >= bottomCrest.bottomY)
    {
      continue;
    }

    ThreadCrestPoint crest =
      makeCrestPoint(result, topCrest.alongPx, topCrest.topY, bottomCrest.alongPx, bottomCrest.bottomY);
    if (!plausibleThreadDiameter(crest.diameterPx, result))
    {
      continue;
    }

    analysis.crests.push_back(crest);
    crestDiameters.push_back(crest.diameterPx);
  }

  if (analysis.crests.empty())
  {
    analysis.failureReason = QStringLiteral("no_paired_crests");
    return analysis;
  }

  analysis.majorPx = arithmeticMean(crestDiameters);

  std::vector<double> grooveDiameters;
  const size_t pairedGrooveCount = std::min(refinedTopGrooves.size(), refinedBottomGrooves.size());
  for (size_t pairIndex = 0; pairIndex < pairedGrooveCount; ++pairIndex)
  {
    const RefinedTopGroove& topGroove = refinedTopGrooves[pairIndex];
    const RefinedBottomGroove& bottomGroove = refinedBottomGrooves[pairIndex];
    if (topGroove.topY + 1e-3 >= bottomGroove.bottomY)
    {
      continue;
    }

    ThreadGroovePoint groove =
      makeGroovePoint(result, topGroove.alongPx, topGroove.topY, bottomGroove.alongPx, bottomGroove.bottomY);
    if (!plausibleThreadDiameter(groove.diameterPx, result))
    {
      continue;
    }

    analysis.grooves.push_back(groove);
    grooveDiameters.push_back(groove.diameterPx);
  }
  if (!grooveDiameters.empty())
  {
    analysis.minorPx = arithmeticMean(grooveDiameters);
  }
  if (analysis.majorPx > 0.5 && analysis.minorPx > 0.5)
  {
    analysis.middlePx = (analysis.majorPx + analysis.minorPx) * 0.5;
  }

  std::vector<double> crestCenters;
  for (const ThreadCrestPoint& crest : analysis.crests)
  {
    crestCenters.push_back(crest.alongPx);
  }
  std::sort(crestCenters.begin(), crestCenters.end());

  const std::vector<double> crestSpacings = consecutiveSpacings(crestCenters);
  if (!crestSpacings.empty())
  {
    analysis.pitchPx = robustMedian(crestSpacings);
  }
  else if (pitchEstimatePx > 0.5)
  {
    analysis.pitchPx = pitchEstimatePx;
  }

  std::vector<double> phaseOffsets;
  phaseOffsets.reserve(pairedCount);
  for (size_t pairIndex = 0; pairIndex < pairedCount; ++pairIndex)
  {
    phaseOffsets.push_back(refinedBottomCrests[pairIndex].alongPx - refinedTopCrests[pairIndex].alongPx);
  }
  if (!phaseOffsets.empty())
  {
    analysis.phasePx = std::abs(robustMedian(std::move(phaseOffsets)));
  }

  analysis.grooveCount = static_cast<int>(analysis.grooves.size());
  analysis.valid = analysis.crests.size() >= 2 && analysis.majorPx > 0.5 && analysis.pitchPx > 0.5;
  if (!analysis.valid)
  {
    analysis.failureReason = analysis.crests.size() < 2
      ? QStringLiteral("insufficient_crests")
      : QStringLiteral("invalid_pitch");
  }
  return analysis;
}
