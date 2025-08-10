#ifndef DM_GAMESYS_COMP_OBJECT_INTERPOLATION_H
#define DM_GAMESYS_COMP_OBJECT_INTERPOLATION_H

#include "res_objectinterpolation.h"

namespace dmObjectInterpolation
{
    enum ObjectInterpolationApplyTransform
    {
        APPLY_TRANSFORM_NONE = 0,
        APPLY_TRANSFORM_TARGET = 1,
    };

    void SetInterpolationEnabled(bool enabled);
    bool IsInterpolationEnabled();

    struct ObjectInterpolationComponent
    {
        ObjectInterpolationResource*      m_Resource;
        ObjectInterpolationApplyTransform m_ApplyTransform;
        dmGameObject::HInstance           m_Instance;

        // Target object to interpolate into if APPLY_TRANSFORM_TARGET is set
        dmGameObject::HInstance m_TargetInstance;

        // Position and rotation of the object at the start of the interpolation
        dmVMath::Point3 m_FromPosition;
        dmVMath::Quat   m_FromRotation;

        // Calculated interpolated position and rotation for the current frame
        // Result of lerp/slerp between m_FromPosition and the current position/rotation of the object
        dmVMath::Point3 m_NextPosition;
        dmVMath::Quat   m_NextRotation;

        // Flags
        uint8_t m_Enabled : 1;
        uint8_t m_AddedToUpdate : 1;
        uint8_t m_ResetFixedTime : 1;
    };
} // namespace dmObjectInterpolation

#endif // DM_GAMESYS_COMP_OBJECT_INTERPOLATION_H
