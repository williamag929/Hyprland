// ════════════════════════════════════════════════════════════════════════════
// SPATIAL OS — SpatialConfig Unit Tests
// TASK-SH-109 | @refactor
//
// SpatialConfig reads from disk, so tests use a TempConfigFile RAII helper
// that writes a string to a mkstemp-allocated path and unlinks on destruction.
// All tests run in the archlinux CI container where POSIX temp files are safe.
// ════════════════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <fcntl.h>  // open, O_WRONLY, O_TRUNC
#include <unistd.h> // mkstemp, close, unlink, write

#include "spatial/SpatialConfig.hpp"

using namespace Spatial;

// ─────────────────────────────────────────────────────────────────────────────
// TempConfigFile — writes a string to a uniquely-named temp file on disk.
// The file is unlinked when the object goes out of scope.
// ─────────────────────────────────────────────────────────────────────────────
class TempConfigFile {
  public:
    explicit TempConfigFile(const std::string& content) {
        // mkstemp requires a writable template buffer
        char      tmpl[] = "/tmp/spatial_test_XXXXXX";
        const int fd     = ::mkstemp(tmpl);
        EXPECT_NE(fd, -1) << "mkstemp failed: " << ::strerror(errno);
        if (fd != -1) {
            m_path                = tmpl;
            const ssize_t written = ::write(fd, content.c_str(), static_cast<ssize_t>(content.size()));
            EXPECT_EQ(written, static_cast<ssize_t>(content.size()));
            ::close(fd);
        }
    }

    ~TempConfigFile() {
        if (!m_path.empty())
            ::unlink(m_path.c_str());
    }

    TempConfigFile(const TempConfigFile&)                             = delete;
    TempConfigFile&                  operator=(const TempConfigFile&) = delete;

    [[nodiscard]] const std::string& path() const {
        return m_path;
    }
    [[nodiscard]] bool valid() const {
        return !m_path.empty();
    }

  private:
    std::string m_path;
};

// ══════════════════════════════════════════════════════════════════════════════
// LIFECYCLE — isLoaded() / loadFromFile() / reload()
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialConfig, NotLoadedByDefault) {
    SpatialConfig cfg;
    EXPECT_FALSE(cfg.isLoaded());
}

TEST(SpatialConfig, LoadNonExistentFileReturnsFalse) {
    SpatialConfig cfg;
    EXPECT_FALSE(cfg.loadFromFile("/tmp/spatial_this_file_does_not_exist_xyz987.conf"));
    EXPECT_FALSE(cfg.isLoaded());
}

TEST(SpatialConfig, LoadValidFileReturnsTrue) {
    TempConfigFile tmp("# empty config\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    EXPECT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_TRUE(cfg.isLoaded());
}

TEST(SpatialConfig, ReloadReturnsFalseBeforeFirstLoad) {
    SpatialConfig cfg;
    EXPECT_FALSE(cfg.reload());
    EXPECT_FALSE(cfg.isLoaded());
}

TEST(SpatialConfig, ReloadReturnsTrueAfterSuccessfulLoad) {
    TempConfigFile tmp("# empty\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_TRUE(cfg.reload());
    EXPECT_TRUE(cfg.isLoaded());
}

TEST(SpatialConfig, ReloadAfterFileDeletedReturnsFalse) {
    char      tmpl[] = "/tmp/spatial_reload_XXXXXX";
    const int fd     = ::mkstemp(tmpl);
    ASSERT_NE(fd, -1);
    ::close(fd);

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmpl));

    ::unlink(tmpl); // delete the file before reloading

    EXPECT_FALSE(cfg.reload());
    EXPECT_FALSE(cfg.isLoaded());
}

// ══════════════════════════════════════════════════════════════════════════════
// DEFAULT VALUES — no spatial section
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialConfig, DefaultsWhenSectionAbsent) {
    TempConfigFile tmp("# no spatial section here\nsome_other_key = 42\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));

    EXPECT_EQ(cfg.getZLayerCount(), 4);
    EXPECT_FLOAT_EQ(cfg.getZLayerStep(), 800.0f);
    EXPECT_FLOAT_EQ(cfg.getZAnimationStiffness(), 200.0f);
    EXPECT_FLOAT_EQ(cfg.getZAnimationDamping(), 20.0f);
    EXPECT_FLOAT_EQ(cfg.getZFOVDegrees(), 60.0f);
    EXPECT_FLOAT_EQ(cfg.getZNearPlane(), 0.1f);
    EXPECT_FLOAT_EQ(cfg.getZFarPlane(), 10000.0f);
}

// ══════════════════════════════════════════════════════════════════════════════
// PARSING — valid values
// ══════════════════════════════════════════════════════════════════════════════

static const std::string kFullSection = R"(
spatial {
    z_layers = 3
    z_layer_step = 600
    z_animation_stiffness = 150
    z_animation_damping = 15
    z_fov_degrees = 45
    z_near_plane = 0.5
    z_far_plane = 5000
}
)";

TEST(SpatialConfig, ParsesAllValuesFromSection) {
    TempConfigFile tmp(kFullSection);
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));

    EXPECT_EQ(cfg.getZLayerCount(), 3);
    EXPECT_FLOAT_EQ(cfg.getZLayerStep(), 600.0f);
    EXPECT_FLOAT_EQ(cfg.getZAnimationStiffness(), 150.0f);
    EXPECT_FLOAT_EQ(cfg.getZAnimationDamping(), 15.0f);
    EXPECT_FLOAT_EQ(cfg.getZFOVDegrees(), 45.0f);
    EXPECT_FLOAT_EQ(cfg.getZNearPlane(), 0.5f);
    EXPECT_FLOAT_EQ(cfg.getZFarPlane(), 5000.0f);
}

TEST(SpatialConfig, ParsesPartialSection_MissingKeysKeepDefaults) {
    TempConfigFile tmp("spatial {\n    z_layers = 2\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));

    EXPECT_EQ(cfg.getZLayerCount(), 2);
    EXPECT_FLOAT_EQ(cfg.getZLayerStep(), 800.0f); // default
    EXPECT_FLOAT_EQ(cfg.getZFOVDegrees(), 60.0f); // default
}

TEST(SpatialConfig, ParsesValueWithTrailingSemicolon) {
    TempConfigFile tmp("spatial {\n    z_layers = 3;\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_EQ(cfg.getZLayerCount(), 3);
}

TEST(SpatialConfig, ParsesValueWithLeadingAndTrailingWhitespace) {
    TempConfigFile tmp("spatial {\n    z_fov_degrees   =   90   \n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZFOVDegrees(), 90.0f);
}

TEST(SpatialConfig, InlineCommentsStripped) {
    TempConfigFile tmp("spatial {\n    z_layers = 5 # five layers\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_EQ(cfg.getZLayerCount(), 5);
}

TEST(SpatialConfig, FloatValue) {
    TempConfigFile tmp("spatial {\n    z_layer_step = 1200.5\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZLayerStep(), 1200.5f);
}

TEST(SpatialConfig, UnknownKeysIgnored) {
    TempConfigFile tmp("spatial {\n    z_layers = 4\n    unknown_key = hello\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    EXPECT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_EQ(cfg.getZLayerCount(), 4);
}

// ══════════════════════════════════════════════════════════════════════════════
// ERROR RECOVERY — malformed values (P0: no crash, default kept)
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialConfig, MalformedIntegerKeepsDefault) {
    TempConfigFile tmp("spatial {\n    z_layers = auto\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    EXPECT_TRUE(cfg.loadFromFile(tmp.path())); // must not throw or crash
    EXPECT_EQ(cfg.getZLayerCount(), 4);        // default kept
}

TEST(SpatialConfig, MalformedFloatKeepsDefault) {
    TempConfigFile tmp("spatial {\n    z_fov_degrees = not_a_number\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    EXPECT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZFOVDegrees(), 60.0f);
}

TEST(SpatialConfig, EmptyValueKeepsDefault) {
    TempConfigFile tmp("spatial {\n    z_layers =\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    EXPECT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_EQ(cfg.getZLayerCount(), 4);
}

TEST(SpatialConfig, OutOfRangeFloatStringKeepsDefault) {
    // 3.5e38 > FLT_MAX (~3.4e38) — stof either throws std::out_of_range or returns HUGE_VALF;
    // validateAndClamp() catches both via !std::isfinite() and resets to default 800.
    TempConfigFile tmp("spatial {\n    z_layer_step = 3.5e38\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    EXPECT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZLayerStep(), 800.0f);
}

// ══════════════════════════════════════════════════════════════════════════════
// RANGE VALIDATION — validateAndClamp() (P0)
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialConfig, ZLayersZeroClampsToOne) {
    TempConfigFile tmp("spatial {\n    z_layers = 0\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_EQ(cfg.getZLayerCount(), 1);
}

TEST(SpatialConfig, ZLayersNegativeClampsToOne) {
    TempConfigFile tmp("spatial {\n    z_layers = -3\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_EQ(cfg.getZLayerCount(), 1);
}

TEST(SpatialConfig, ZLayersTooLargeClampsToSixteen) {
    TempConfigFile tmp("spatial {\n    z_layers = 100\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_EQ(cfg.getZLayerCount(), 16);
}

TEST(SpatialConfig, ZLayerStepZeroResetsToDefault) {
    TempConfigFile tmp("spatial {\n    z_layer_step = 0\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZLayerStep(), 800.0f);
}

TEST(SpatialConfig, ZLayerStepNegativeResetsToDefault) {
    TempConfigFile tmp("spatial {\n    z_layer_step = -100\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZLayerStep(), 800.0f);
}

TEST(SpatialConfig, ZAnimationStiffnessBelowOneClampsToOne) {
    TempConfigFile tmp("spatial {\n    z_animation_stiffness = 0.5\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZAnimationStiffness(), 1.0f);
}

TEST(SpatialConfig, ZAnimationDampingNegativeClampsToZero) {
    TempConfigFile tmp("spatial {\n    z_animation_damping = -5\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZAnimationDamping(), 0.0f);
}

TEST(SpatialConfig, ZFovZeroResetsToSixty) {
    TempConfigFile tmp("spatial {\n    z_fov_degrees = 0\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZFOVDegrees(), 60.0f);
}

TEST(SpatialConfig, ZFovOneEightyResetsToSixty) {
    TempConfigFile tmp("spatial {\n    z_fov_degrees = 180\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZFOVDegrees(), 60.0f);
}

TEST(SpatialConfig, ZFovAboveOneEightyResetsToSixty) {
    TempConfigFile tmp("spatial {\n    z_fov_degrees = 270\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZFOVDegrees(), 60.0f);
}

TEST(SpatialConfig, ZNearPlaneZeroResetsToDefault) {
    TempConfigFile tmp("spatial {\n    z_near_plane = 0\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZNearPlane(), 0.1f);
}

TEST(SpatialConfig, ZFarPlaneEqualToNearResetsToDefault) {
    TempConfigFile tmp("spatial {\n    z_near_plane = 1.0\n    z_far_plane = 1.0\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZFarPlane(), 10000.0f);
}

TEST(SpatialConfig, ZFarPlaneLessThanNearResetsToDefault) {
    TempConfigFile tmp("spatial {\n    z_near_plane = 5.0\n    z_far_plane = 1.0\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZFarPlane(), 10000.0f);
}

TEST(SpatialConfig, ValidNearAndFarKept) {
    TempConfigFile tmp("spatial {\n    z_near_plane = 0.5\n    z_far_plane = 8000\n}\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_FLOAT_EQ(cfg.getZNearPlane(), 0.5f);
    EXPECT_FLOAT_EQ(cfg.getZFarPlane(), 8000.0f);
}

// ══════════════════════════════════════════════════════════════════════════════
// HOT-RELOAD — values update after file content changes
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialConfig, ReloadPicksUpChangedValues) {
    // Write initial config
    char      tmpl[] = "/tmp/spatial_reload2_XXXXXX";
    const int fd     = ::mkstemp(tmpl);
    ASSERT_NE(fd, -1);
    const std::string first = "spatial {\n    z_layers = 2\n}\n";
    ::write(fd, first.c_str(), first.size());
    ::close(fd);

    SpatialConfig cfg;
    ASSERT_TRUE(cfg.loadFromFile(tmpl));
    EXPECT_EQ(cfg.getZLayerCount(), 2);

    // Overwrite with new content
    const int fd2 = ::open(tmpl, O_WRONLY | O_TRUNC);
    ASSERT_NE(fd2, -1);
    const std::string second = "spatial {\n    z_layers = 4\n}\n";
    ::write(fd2, second.c_str(), second.size());
    ::close(fd2);

    EXPECT_TRUE(cfg.reload());
    EXPECT_EQ(cfg.getZLayerCount(), 4);

    ::unlink(tmpl);
}

// ══════════════════════════════════════════════════════════════════════════════
// STRUCTURAL PARSE ERRORS
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialConfig, MissingOpenBraceReturnsFalse) {
    // "spatial" keyword present but no opening brace
    TempConfigFile tmp("spatial\n    z_layers = 4\n");
    ASSERT_TRUE(tmp.valid());

    SpatialConfig cfg;
    EXPECT_FALSE(cfg.loadFromFile(tmp.path()));
    EXPECT_FALSE(cfg.isLoaded());
}

// ══════════════════════════════════════════════════════════════════════════════
// DEBUG
// ══════════════════════════════════════════════════════════════════════════════

TEST(SpatialConfig, DebugPrintDoesNotCrash) {
    SpatialConfig cfg;
    EXPECT_NO_THROW(cfg.debugPrint());

    TempConfigFile tmp(kFullSection);
    ASSERT_TRUE(tmp.valid());
    ASSERT_TRUE(cfg.loadFromFile(tmp.path()));
    EXPECT_NO_THROW(cfg.debugPrint());
}
