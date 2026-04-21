// comp_tag.h                                                         -*-C++-*-
#ifndef INCLUDED_SCN_COMP_TAG_H
#define INCLUDED_SCN_COMP_TAG_H

//@PURPOSE: Provide a component linking an entity to its name.
//
//@CLASSES:
//  eng::scn::comp::TagComponent: ECS component for entity name.
//
//@DESCRIPTION: This component stores the entity name.

// std
#include <string>

namespace eng::scn::comp {

struct TagComponent {
    std::string d_name = "Entity";
};

}  // close package namespace

#endif  // #ifndef INCLUDED_SCN_COMP_TAG_H
