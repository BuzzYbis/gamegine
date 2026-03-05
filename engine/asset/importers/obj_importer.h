// obj_importer.h                                                     -*-C++-*-
#ifndef INCLUDED_ENGINE_ASSET_OBJ_IMPORTER_H
#define INCLUDED_ENGINE_ASSET_OBJ_IMPORTER_H

// asset
#include <asset/importers/model_importer.h>

// std
#include <string>

namespace engine::asset {

class ObjImporter : public ModelImporter {
  public:
    ObjImporter()           = default;
    ~ObjImporter() override = default;

    std::shared_ptr<ModelData> load(const std::string& filepath) override;
};

}  // close engine::asset namespace

#endif  // INCLUDED_ENGINE_ASSET_OBJ_IMPORTER_H
