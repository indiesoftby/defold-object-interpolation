#ifndef DM_GAMESYS_COMP_OBJECT_INTERPOLATION_H
#define DM_GAMESYS_COMP_OBJECT_INTERPOLATION_H

#include "res_objectinterpolation.h"

namespace dmObjectInterpolation
{
    enum ObjectInterpolationApplyTransform
    {
        APPLY_TRANSFORM_NONE = 0,
        APPLY_TRANSFORM_TARGET = 1,
        APPLY_TRANSFORM_RENDER = 2,
    };

    void SetInterpolationEnabled(bool enabled);
    bool IsInterpolationEnabled();

    struct ObjectInterpolationComponent
    {
        ObjectInterpolationResource*      m_Resource;
        ObjectInterpolationApplyTransform m_ApplyTransform;
        dmGameObject::HInstance           m_Instance;

        // Time values used for interpolation
        float m_Time;
        float m_FixedTime;
        float m_FixedUpdateDT;
        float m_UpdateDT;

        // Target object to interpolate into if APPLY_TRANSFORM_TARGET is set
        dmGameObject::HInstance m_TargetInstance;

        // TODO: update the comments!!!
        // Interpolation reference point: position and rotation at the moment of the last FixedUpdate
        // In APPLY_TRANSFORM_RENDER contains the previous interpolated value (m_NextPosition)
        // In APPLY_TRANSFORM_TARGET is obtained from the current value of the target object
        dmVMath::Point3 m_FromPosition;
        dmVMath::Quat   m_FromRotation;

        // Snapshot of the actual position and rotation of the object at render time (before interpolation)
        // Used in PostUpdate to restore the original value in APPLY_TRANSFORM_RENDER mode
        dmVMath::Point3 m_PreRenderPosition;
        dmVMath::Quat   m_PreRenderRotation;

        // Calculated interpolated position and rotation for the current frame
        // Result of lerp/slerp between m_FromPosition and the current position/rotation of the object
        dmVMath::Point3 m_NextPosition;
        dmVMath::Quat   m_NextRotation;

        // Flags
        uint8_t m_Enabled : 1;
        uint8_t m_AddedToUpdate : 1;
    };
} // namespace dmObjectInterpolation

#endif // DM_GAMESYS_COMP_OBJECT_INTERPOLATION_H
