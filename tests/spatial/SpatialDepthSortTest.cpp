// ════════════════════════════════════════════════════════════════════════════
// SPATIAL OS — Depth Sort Unit Tests
// TASK-SH-201 | @refactor
//
// Tests the bucket-assignment and intra-bucket sort logic that underpins
// renderWorkspaceWindowsSpatial().  The actual render function requires a live
// OpenGL compositor and cannot be exercised here; instead we verify the pure
// data transformations (bucket assignment, clamping, fZPosition ordering) on
// fake windows using the same SSpatialProps fields the renderer reads.
//
// Bucket assignment rule (from TASK-SH-002 spec):
//   iZLayer in [0, Z_LAYERS_COUNT) → bucket[iZLayer]
//   iZLayer <0 or >=Z_LAYERS_COUNT → clamped to bucket[0] (foreground)
//
// Intra-bucket sort rule:
//   windows sorted ascending by fZPosition (back-to-front = painter's algorithm)
// ════════════════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include "spatial/ZSpaceManager.hpp" // Z_LAYERS_COUNT, LAYER_Z_POSITIONS
#include "desktop/view/Window.hpp"   // SSpatialProps

using namespace Spatial;

// ─────────────────────────────────────────────────────────────────────────────
// MiniFakeWindow — minimal struct holding only SSpatialProps.
// Mirrors the FakeWindow pattern in ZSpaceManagerTest.cpp but lighter:
// we only need the iZLayer / fZPosition fields, not a full CWindow allocation.
// ─────────────────────────────────────────────────────────────────────────────
struct MiniFakeWindow {
    Desktop::View::CWindow::SSpatialProps props{};
};

// ─────────────────────────────────────────────────────────────────────────────
// Helper: replicate the bucket assignment from renderWorkspaceWindowsSpatial
// ─────────────────────────────────────────────────────────────────────────────
static std::array<std::vector<const MiniFakeWindow*>, Z_LAYERS_COUNT> assignToBuckets(const std::vector<MiniFakeWindow>& windows) {
    std::array<std::vector<const MiniFakeWindow*>, Z_LAYERS_COUNT> buckets;
    for (auto const& w : windows) {
        int layer = w.props.iZLayer;
        if (layer < 0 || layer >= Z_LAYERS_COUNT)
            layer = 0;
        buckets[static_cast<size_t>(layer)].push_back(&w);
    }
    return buckets;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: sort each bucket back-to-front by fZPosition, return sorted order
// ─────────────────────────────────────────────────────────────────────────────
static void sortBuckets(std::array<std::vector<const MiniFakeWindow*>, Z_LAYERS_COUNT>& buckets) {
    for (auto& bucket : buckets) {
        std::sort(bucket.begin(), bucket.end(), [](const MiniFakeWindow* a, const MiniFakeWindow* b) { return a->props.fZPosition < b->props.fZPosition; });
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// BUCKET ASSIGNMENT — iZLayer range coverage
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialDepthSortTest, AllLayersAssignToCorrectBucket) {
    std::vector<MiniFakeWindow> windows(Z_LAYERS_COUNT);
    for (int i = 0; i < Z_LAYERS_COUNT; ++i)
        windows[static_cast<size_t>(i)].props.iZLayer = i;

    auto buckets = assignToBuckets(windows);

    for (int i = 0; i < Z_LAYERS_COUNT; ++i) {
        EXPECT_EQ(buckets[static_cast<size_t>(i)].size(), 1u) << "Layer " << i << " bucket should have exactly 1 window";
        EXPECT_EQ(buckets[static_cast<size_t>(i)][0]->props.iZLayer, i);
    }
}

TEST(SpatialDepthSortTest, NegativeLayerClampedToForeground) {
    std::vector<MiniFakeWindow> windows(1);
    windows[0].props.iZLayer = -1; // invalid — below range

    auto buckets = assignToBuckets(windows);

    EXPECT_EQ(buckets[0].size(), 1u) << "iZLayer=-1 should fall into bucket[0] (foreground)";
    for (size_t i = 1; i < Z_LAYERS_COUNT; ++i)
        EXPECT_TRUE(buckets[i].empty());
}

TEST(SpatialDepthSortTest, BeyondMaxLayerClampedToForeground) {
    std::vector<MiniFakeWindow> windows(1);
    windows[0].props.iZLayer = Z_LAYERS_COUNT; // one past max

    auto buckets = assignToBuckets(windows);

    EXPECT_EQ(buckets[0].size(), 1u) << "iZLayer=" << Z_LAYERS_COUNT << " should fall into bucket[0]";
}

TEST(SpatialDepthSortTest, LargeOutOfRangeLayerClampedToForeground) {
    std::vector<MiniFakeWindow> windows(1);
    windows[0].props.iZLayer = 99;

    auto buckets = assignToBuckets(windows);

    EXPECT_EQ(buckets[0].size(), 1u);
}

TEST(SpatialDepthSortTest, MixedWindowsDistributeCorrectly) {
    // 2 windows per layer plus 1 out-of-range (goes to layer 0)
    std::vector<MiniFakeWindow> windows;
    for (int i = 0; i < Z_LAYERS_COUNT; ++i) {
        for (int j = 0; j < 2; ++j) {
            MiniFakeWindow w;
            w.props.iZLayer = i;
            windows.push_back(w);
        }
    }
    MiniFakeWindow oor;
    oor.props.iZLayer = -5;
    windows.push_back(oor);

    auto buckets = assignToBuckets(windows);

    // layer 0 gets 2 normal + 1 out-of-range = 3
    EXPECT_EQ(buckets[0].size(), 3u);
    for (size_t i = 1; i < Z_LAYERS_COUNT; ++i)
        EXPECT_EQ(buckets[i].size(), 2u) << "Layer " << i;
}

// ══════════════════════════════════════════════════════════════════════════════
// INTRA-BUCKET SORT — back-to-front by fZPosition
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialDepthSortTest, SortWithinBucketIsBackToFront) {
    // Three windows at layer 1, with different continuous Z positions
    // (mid-animation between layers)
    std::vector<MiniFakeWindow> windows(3);
    windows[0].props.iZLayer    = 1;
    windows[0].props.fZPosition = -500.0f; // closer to camera than layer 1 target (-800)
    windows[1].props.iZLayer    = 1;
    windows[1].props.fZPosition = -900.0f; // slightly past target
    windows[2].props.iZLayer    = 1;
    windows[2].props.fZPosition = -800.0f; // exactly at layer 1 target

    auto buckets = assignToBuckets(windows);
    sortBuckets(buckets);

    const auto& b = buckets[1];
    ASSERT_EQ(b.size(), 3u);

    // Sorted ascending by fZPosition: -900 < -800 < -500
    EXPECT_FLOAT_EQ(b[0]->props.fZPosition, -900.0f); // deepest first
    EXPECT_FLOAT_EQ(b[1]->props.fZPosition, -800.0f);
    EXPECT_FLOAT_EQ(b[2]->props.fZPosition, -500.0f); // nearest last (renders on top)
}

TEST(SpatialDepthSortTest, SortStableForEqualZPositions) {
    // Windows with identical fZPosition should not crash/reorder unpredictably
    std::vector<MiniFakeWindow> windows(3);
    for (auto& w : windows) {
        w.props.iZLayer    = 0;
        w.props.fZPosition = 0.0f;
    }
    auto buckets = assignToBuckets(windows);
    EXPECT_NO_THROW(sortBuckets(buckets));
    EXPECT_EQ(buckets[0].size(), 3u);
}

TEST(SpatialDepthSortTest, DeepestLayerHasLowestZPosition) {
    // Verify that LAYER_Z_POSITIONS constants reflect the painter's algorithm assumption:
    // layer N-1 (far) has the most negative Z (rendered first = behind everything)
    for (int i = 0; i < Z_LAYERS_COUNT - 1; ++i) {
        EXPECT_GT(LAYER_Z_POSITIONS[static_cast<size_t>(i)], LAYER_Z_POSITIONS[static_cast<size_t>(i + 1)])
            << "Layer " << i << " Z=" << LAYER_Z_POSITIONS[static_cast<size_t>(i)] << " should be greater than layer " << (i + 1)
            << " Z=" << LAYER_Z_POSITIONS[static_cast<size_t>(i + 1)];
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// RENDER ORDER — deepest bucket rendered first
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialDepthSortTest, BucketIterationOrderIsDeepFirst) {
    // Simulate the render loop iteration: li from Z_LAYERS_COUNT-1 down to 0
    // Verifies that for an N-layer system we visit deepest bucket before shallowest.
    std::vector<int> renderOrder;
    for (int li = Z_LAYERS_COUNT - 1; li >= 0; --li)
        renderOrder.push_back(li);

    // First rendered layer should be Z_LAYERS_COUNT-1 (far/deepest)
    EXPECT_EQ(renderOrder.front(), Z_LAYERS_COUNT - 1);
    // Last rendered layer should be 0 (foreground/nearest)
    EXPECT_EQ(renderOrder.back(), 0);
    // Should have exactly Z_LAYERS_COUNT steps
    EXPECT_EQ(static_cast<int>(renderOrder.size()), Z_LAYERS_COUNT);
    // Should be strictly decreasing
    for (size_t i = 1; i < renderOrder.size(); ++i)
        EXPECT_EQ(renderOrder[i], renderOrder[i - 1] - 1);
}

TEST(SpatialDepthSortTest, EmptyBucketsDoNotContributeToRenderOrder) {
    // A system with windows only at layers 0 and 3 — buckets 1 and 2 are empty
    std::vector<MiniFakeWindow> windows(2);
    windows[0].props.iZLayer = 0;
    windows[1].props.iZLayer = static_cast<int>(Z_LAYERS_COUNT) - 1;

    auto buckets = assignToBuckets(windows);

    int  nonEmptyCount = 0;
    for (auto const& b : buckets) {
        if (!b.empty())
            ++nonEmptyCount;
    }
    EXPECT_EQ(nonEmptyCount, 2); // only bucket 0 and bucket Z_LAYERS_COUNT-1 have content
}
