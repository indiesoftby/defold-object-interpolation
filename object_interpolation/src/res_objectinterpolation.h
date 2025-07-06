#ifndef DM_GAMESYS_RES_OBJECT_INTERPOLATION_H
#define DM_GAMESYS_RES_OBJECT_INTERPOLATION_H

#include "objectinterpolation_ddf.h" // generated from the objectinterpolation_ddf.proto

namespace dmObjectInterpolation
{
    struct ObjectInterpolationResource
    {
        dmObjectInterpolationDDF::ObjectInterpolationDesc* m_DDF;
    };
} // namespace dmObjectInterpolation

#endif
