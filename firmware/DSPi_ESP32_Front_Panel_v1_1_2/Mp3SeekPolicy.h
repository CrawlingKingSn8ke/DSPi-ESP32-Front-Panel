#pragma once

#include <stdint.h>

// Pure, host-testable policy for cooperative unindexed MP3 seeks.
//
// Hardware evidence from v12.1 showed roughly 230k decoded PCM frames/second
// while scanning. Use a deliberately lower planning rate so valid work that
// already exceeded the former fixed 12-second timeout receives a proportional
// budget. The absolute cap still bounds a corrupt or pathologically slow file.
namespace DspiMp3SeekPolicy {

constexpr uint64_t kPlanningFramesPerSecond = 150000ULL;
constexpr uint32_t kBaseBudgetMs = 2500U;
constexpr uint32_t kMinimumBudgetMs = 4000U;
constexpr uint32_t kAbsoluteBudgetMs = 45000U;

struct Plan {
  uint64_t sourceFrame = 0;
  uint64_t targetFrame = 0;
  uint64_t scanFrames = 0;
  uint32_t budgetMs = kMinimumBudgetMs;
};

constexpr uint32_t budgetForScanFrames(uint64_t scanFrames)
{
  // Split before multiplying so even adversarial 64-bit frame counts cannot
  // overflow. The remainder calculation rounds up to the next millisecond.
  const uint64_t wholeSeconds = scanFrames / kPlanningFramesPerSecond;
  const uint64_t remainderFrames = scanFrames % kPlanningFramesPerSecond;
  if (wholeSeconds >= kAbsoluteBudgetMs / 1000U) {
    return kAbsoluteBudgetMs;
  }

  uint64_t scanMs = wholeSeconds * 1000ULL;
  scanMs +=
      (remainderFrames * 1000ULL + kPlanningFramesPerSecond - 1ULL) /
      kPlanningFramesPerSecond;
  uint64_t budget = scanMs + kBaseBudgetMs;
  if (budget < kMinimumBudgetMs) budget = kMinimumBudgetMs;
  if (budget > kAbsoluteBudgetMs) budget = kAbsoluteBudgetMs;
  return static_cast<uint32_t>(budget);
}

constexpr Plan candidatePlan(uint64_t sourceFrame, uint64_t targetFrame)
{
  Plan plan;
  plan.sourceFrame = sourceFrame;
  plan.targetFrame = targetFrame;
  // An isolated candidate starts at the beginning. This can cost more than a
  // live forward scan, but it guarantees that a failed attempt cannot damage
  // the decoder which is still responsible for current playback.
  plan.scanFrames = targetFrame;
  plan.budgetMs = budgetForScanFrames(plan.scanFrames);
  return plan;
}

static_assert(budgetForScanFrames(0) == kMinimumBudgetMs,
              "zero-distance seek must use the minimum bounded budget");
static_assert(budgetForScanFrames(3482352ULL) > 12000U,
              "logged reverse MP3 target must not inherit the old 12s limit");
static_assert(budgetForScanFrames(4225600ULL) > 12000U,
              "logged long forward MP3 target must exceed the old 12s limit");
static_assert(budgetForScanFrames(UINT64_MAX) == kAbsoluteBudgetMs,
              "adversarial frame counts must clamp without overflow");

}  // namespace DspiMp3SeekPolicy
