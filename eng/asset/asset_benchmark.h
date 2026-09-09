// asset_benchmark.h                                                  -*-C++-*-
#ifndef INCLUDED_ENG_ASSET_BENCHMARK_H
#define INCLUDED_ENG_ASSET_BENCHMARK_H

//@PURPOSE: Provide a repeatable measurement of the cost of loading a model.
//
//@CLASSES:
//  eng::asset::LoadBenchmark: Timed, repeated load of a model into a CSV row.
//
//@DESCRIPTION: This component provides 'eng::asset::LoadBenchmark', which
// loads a model a number of times and appends a single row describing the
// result to a CSV file, so that the cost of loading a given scene can be
// followed across the commits of the engine.
//
// Each run is given a brand new 'eng::asset::AssetManager': reusing one would
// have every run after the first hit its model and texture caches and measure
// nothing at all. The GPU is left to drain between two runs, so that the work
// of one is not billed to the next.
//
// The row carries far more than a duration. A measurement that cannot be
// attributed is not worth keeping, so the identity of the build (commit,
// dirty state, configuration, compiler) and of the machine (OS, CPU, GPU) is
// recorded alongside it; a Debug row and a Release row otherwise sit in the
// same column and make the history meaningless. The duration itself is split
// into the phases the profiler measured, because a total that grew tells that
// something regressed but never what, and it is reported as a minimum over
// the runs rather than as a single sample: benchmark noise is additive, which
// makes the fastest run the most faithful estimate of what the code costs.
// The size of the workload is recorded too, so that a scene that grew is not
// mistaken for an engine that slowed down.
//
// The first run is reported separately as the cold one: it reads the file
// from the drive where the following ones find it in the page cache of the
// operating system, and the two are not comparable.
//
/// Usage
///-----
//..
//  asset::LoadBenchmark::Config config;
//  config.scenePath = "models/bistro/bistro.gltf";
//  config.runs      = 5;
//
//  asset::LoadBenchmark::run(config, context, renderer);
//..

// std
#include <cstdint>
#include <string>

// Forward declarations
namespace eng::rhi {
class ContextProtocol;
}  // close package namespace

namespace eng::rnd {
class Renderer;
}  // close package namespace

namespace eng::asset {

// ===================
// class LoadBenchmark
// ===================

/// This utility loads a model repeatedly and reports the cost of doing so.
class LoadBenchmark {
  public:
    // TYPES

    /// Parameters of a benchmark session.
    struct Config {
        /// Path of the model to load, as given to the asset manager.
        std::string scenePath;

        /// Path of the CSV file the result is appended to. The file is
        /// created, header included, if it does not exist yet.
        std::string csvPath = "time.csv";

        /// Number of times the model is loaded.
        uint32_t runs = 3;
    };

    // CLASS METHODS

    /// Load the model described by the specified 'config' the configured
    /// number of times using the specified 'context' and 'renderer', and
    /// append one row summarizing the session to the configured CSV file.
    /// Return 'true' on success, and 'false' if the model could not be
    /// loaded or the file could not be written.
    static bool run(const Config&         config,
                    rhi::ContextProtocol* context,
                    rnd::Renderer*        renderer);
};

}  // close package namespace

#endif  // INCLUDED_ENG_ASSET_BENCHMARK_H
