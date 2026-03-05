// model_importer.h                                                   -*-C++-*-
#ifndef INCLUDED_ENGINE_ASSET_MODEL_LOADER_H
#define INCLUDED_ENGINE_ASSET_MODEL_LOADER_H

// std
#include <memory>

namespace engine::asset {
struct ModelData;

class ModelImporter {
  public:
    virtual ~ModelImporter() = default;

    virtual std::shared_ptr<ModelData> load(const std::string& filepath) = 0;
};

}  // close engine::asset namespace

#endif  // INCLUDED_ENGINE_ASSET_MODEL_LOADER_H
