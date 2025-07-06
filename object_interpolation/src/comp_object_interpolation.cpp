#include <string.h> // memset

#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/object_pool.h>

#include <dmsdk/gameobject/component.h>
#include <dmsdk/gamesys/property.h>

#include <gameobject/gameobject_ddf.h>

#include "comp_object_interpolation.h"
#include "objectinterpolation_ddf.h" // generated from the objectinterpolation_ddf.proto
#include "res_objectinterpolation.h"

namespace dmGameObject
{
    /*
     * Allows for updating transforms an extra time
     */
    void UpdateTransforms(dmGameObject::HCollection hcollection);

    /**
     * Get absolute identifier relative to #instance. The returned identifier is the
     * representation of the qualified name, i.e. the path from root-collection to the sub-collection which the #instance belongs to.
     * Example: if #instance is part of a sub-collection in the root-collection named "sub" and id == "a" the returned identifier represents the path "sub.a"
     * @param instance Instance to absolute identifier to
     * @param id Identifier relative to #instance
     * @param id_size Lenght of the id
     * @return Absolute identifier
     */
    dmhash_t GetAbsoluteIdentifier(dmGameObject::HInstance instance, const char* id, uint32_t id_size);
} // namespace dmGameObject

namespace dmObjectInterpolation
{
    using namespace dmVMath;

    static const dmhash_t PROP_TARGET_OBJECT = dmHashString64("target_object");
    static const dmhash_t PROP_APPLY_TRANSFORM = dmHashString64("apply_transform");
    static const dmhash_t PROP_POSITION = dmHashString64("position");
    static const dmhash_t PROP_ROTATION = dmHashString64("rotation");

    static const char*    PROJECT_PROPERTY_MAX_COUNT = "object_interpolation.max_count";
    static const char*    ID_SEPARATOR = "/";

    static bool           g_InterpolationEnabled = true;

    // One context for the life time of the app
    struct ObjectInterpolationContext
    {
        ObjectInterpolationContext()
        {
            memset(this, 0, sizeof(*this));
        }
        // ...
        uint32_t m_MaxComponentsPerWorld;
    };

    // One world per loaded collection
    struct ObjectInterpolationWorld
    {
        ObjectInterpolationContext*                m_Context;
        dmObjectPool<ObjectInterpolationComponent> m_Components;
    };

    static inline float Clamp01(float value)
    {
        return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f :
                                                    value;
    }

    void SetInterpolationEnabled(bool enabled)
    {
        g_InterpolationEnabled = enabled;
    }

    bool IsInterpolationEnabled()
    {
        return g_InterpolationEnabled;
    }

    dmGameObject::CreateResult CompObjectInterpolationNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ObjectInterpolationContext* context = (ObjectInterpolationContext*)params.m_Context;
        ObjectInterpolationWorld*   world = new ObjectInterpolationWorld();
        world->m_Context = context;

        uint32_t comp_count = dmMath::Min(context->m_MaxComponentsPerWorld, params.m_MaxComponentInstances);
        world->m_Components.SetCapacity(comp_count);
        memset(world->m_Components.GetRawObjects().Begin(), 0, sizeof(ObjectInterpolationComponent) * comp_count);
        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompObjectInterpolationDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ObjectInterpolationWorld* world = (ObjectInterpolationWorld*)params.m_World;
        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static bool UpdateApplyTransform(ObjectInterpolationComponent* component, dmObjectInterpolationDDF::ObjectInterpolationDesc::ApplyTransform apply_transform, dmhash_t target_object_hash)
    {
        dmGameObject::HCollection collection = dmGameObject::GetCollection(component->m_Instance);

        if (apply_transform == dmObjectInterpolationDDF::ObjectInterpolationDesc::APPLY_TRANSFORM_NONE)
        {
            component->m_ApplyTransform = APPLY_TRANSFORM_NONE;
            component->m_TargetInstance = 0;
            return true;
        }
        else if (apply_transform == dmObjectInterpolationDDF::ObjectInterpolationDesc::APPLY_TRANSFORM_TARGET)
        {
            component->m_TargetInstance = dmGameObject::GetInstanceFromIdentifier(collection, target_object_hash);
            if (!component->m_TargetInstance)
            {
                component->m_ApplyTransform = APPLY_TRANSFORM_NONE;
                dmLogError("Target object %s is not found. Transform mode is set to APPLY_TRANSFORM_NONE.", dmHashReverseSafe64(target_object_hash));
                return false;
            }
            else
            {
                if (component->m_TargetInstance == component->m_Instance)
                {
                    component->m_ApplyTransform = APPLY_TRANSFORM_NONE;
                    component->m_TargetInstance = 0;
                    dmLogError("Target object is the same as the current object. Transform mode is set to APPLY_TRANSFORM_NONE.");
                    return false;
                }

                component->m_ApplyTransform = APPLY_TRANSFORM_TARGET;
                dmGameObject::SetPosition(component->m_TargetInstance, component->m_FromPosition);
                dmGameObject::SetRotation(component->m_TargetInstance, component->m_FromRotation);
            }

            return true;
        }

        component->m_ApplyTransform = APPLY_TRANSFORM_NONE;
        dmLogError("Unknown transform mode: %d. Transform mode is set to APPLY_TRANSFORM_NONE.", (int)apply_transform);
        return false;
    }

    dmGameObject::CreateResult CompObjectInterpolationCreate(const dmGameObject::ComponentCreateParams& params)
    {
        ObjectInterpolationWorld* world = (ObjectInterpolationWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            dmLogError("Component could not be created since the buffer is full (%d). See '%s' in game.project", world->m_Components.Capacity(), PROJECT_PROPERTY_MAX_COUNT);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        uint32_t                      index = world->m_Components.Alloc();
        ObjectInterpolationComponent* component = &world->m_Components.Get(index);
        memset(component, 0, sizeof(ObjectInterpolationComponent)); // Don't memset if it contains non-pod types
        component->m_Resource = (ObjectInterpolationResource*)params.m_Resource;
        component->m_Instance = params.m_Instance;
        component->m_Enabled = 1;

        // Initialize the component with the current position and rotation of the game object instance
        const Point3& position = dmGameObject::GetPosition(component->m_Instance);
        const Quat&   rotation = dmGameObject::GetRotation(component->m_Instance);
        component->m_FromPosition = position;
        component->m_FromRotation = rotation;
        component->m_NextPosition = position;
        component->m_NextRotation = rotation;

        // DEBUG
        // dmLogInfo("     Create - Object position: %f, %f, %f", position.getX(), position.getY(), position.getZ());

        dmhash_t target_object_hash = 0;
        if (component->m_Resource->m_DDF->m_TargetObject[0] != '\0')
        {
            // dmLogInfo("Target object: %s", component->m_Resource->m_DDF->m_TargetObject);
            target_object_hash = dmGameObject::GetAbsoluteIdentifier(component->m_Instance, component->m_Resource->m_DDF->m_TargetObject, strlen(component->m_Resource->m_DDF->m_TargetObject));
            // dmLogInfo("-> Target object hash: %lu", (unsigned long)target_object_hash);
        }

        if (!UpdateApplyTransform(component, component->m_Resource->m_DDF->m_ApplyTransform, target_object_hash))
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        *params.m_UserData = index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline ObjectInterpolationComponent* GetComponentFromIndex(ObjectInterpolationWorld* world, int index)
    {
        return &world->m_Components.Get(index);
    }

    static void* CompObjectInterpolationGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        ObjectInterpolationWorld* world = (ObjectInterpolationWorld*)params.m_World;
        uint32_t                  index = (uint32_t)params.m_UserData;
        return GetComponentFromIndex(world, index);
    }

    static void DestroyComponent(ObjectInterpolationWorld* world, uint32_t index)
    {
        world->m_Components.Free(index, true);
    }

    dmGameObject::CreateResult CompObjectInterpolationDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ObjectInterpolationWorld* world = (ObjectInterpolationWorld*)params.m_World;
        uint32_t                  index = *params.m_UserData;
        DestroyComponent(world, index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompObjectInterpolationAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        ObjectInterpolationWorld*     world = (ObjectInterpolationWorld*)params.m_World;
        uint32_t                      index = (uint32_t)*params.m_UserData;
        ObjectInterpolationComponent* component = GetComponentFromIndex(world, index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompObjectInterpolationUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        ObjectInterpolationWorld*              world = (ObjectInterpolationWorld*)params.m_World;
        float                                  dt = params.m_UpdateContext->m_DT;
        dmArray<ObjectInterpolationComponent>& components = world->m_Components.GetRawObjects();
        const uint32_t                         count = components.Size();
        bool                                   update_transforms = false;

        for (uint32_t i = 0; i < count; ++i)
        {
            ObjectInterpolationComponent& component = components[i];
            if (!component.m_AddedToUpdate)
                continue;

            component.m_UpdateDT = dt;
            component.m_Time += dt;

            if (!component.m_Enabled)
                continue;

            const Point3& to_position = dmGameObject::GetPosition(component.m_Instance);
            const Quat&   to_rotation = dmGameObject::GetRotation(component.m_Instance);

            // DEBUG
            // dmLogInfo("     Update - From position: %f, %f, %f", component.m_FromPosition.getX(), component.m_FromPosition.getY(), component.m_FromPosition.getZ());
            // dmLogInfo("                    Lerp to: %f, %f, %f", to_position.getX(), to_position.getY(), to_position.getZ());

            const float interpolation_factor = Clamp01(component.m_Time / component.m_FixedTime);
            if (g_InterpolationEnabled)
            {
                component.m_NextPosition = dmVMath::Point3(dmVMath::Lerp(interpolation_factor, dmVMath::Vector3(component.m_FromPosition), dmVMath::Vector3(to_position)));
                component.m_NextRotation = dmVMath::Slerp(interpolation_factor, component.m_FromRotation, to_rotation);
            }
            else
            {
                component.m_NextPosition = to_position;
                component.m_NextRotation = to_rotation;
            }

            if (component.m_ApplyTransform == APPLY_TRANSFORM_TARGET)
            {
                dmGameObject::SetPosition(component.m_TargetInstance, component.m_NextPosition);
                dmGameObject::SetRotation(component.m_TargetInstance, component.m_NextRotation);
                update_transforms = true;
            }

            // DEBUG
            // dmLogInfo("              Factor: %f, Current Position: %f, %f, %f", interpolation_factor, component.m_NextPosition.getX(), component.m_NextPosition.getY(), component.m_NextPosition.getZ());
        }

        if (update_transforms)
        {
            update_result.m_TransformsUpdated = true;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompObjectInterpolationFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        ObjectInterpolationWorld*              world = (ObjectInterpolationWorld*)params.m_World;
        float                                  dt = params.m_UpdateContext->m_DT;
        float                                  accumFrameTime = params.m_UpdateContext->m_AccumFrameTime;
        dmArray<ObjectInterpolationComponent>& components = world->m_Components.GetRawObjects();
        const uint32_t                         count = components.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            ObjectInterpolationComponent& component = components[i];
            if (!component.m_AddedToUpdate)
                continue;

            component.m_Time = 0.0f;
            component.m_FixedTime = dt;
            component.m_FixedUpdateDT = dt;

            if (!component.m_Enabled)
                continue;

            // DEBUG
            // const Point3& to_position = dmGameObject::GetPosition(component.m_Instance);
            // const Quat&   to_rotation = dmGameObject::GetRotation(component.m_Instance);
            // dmLogInfo("FixedUpdate - To position: %f, %f, %f - DT: %f, AccumFrameTime: %f, TimeScale: %f", to_position.getX(), to_position.getY(), to_position.getZ(), dt, accumFrameTime, params.m_UpdateContext->m_TimeScale);

            component.m_FromPosition = component.m_NextPosition;
            component.m_FromRotation = component.m_NextRotation;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompObjectInterpolationOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        ObjectInterpolationWorld*     world = (ObjectInterpolationWorld*)params.m_World;
        ObjectInterpolationComponent* component = GetComponentFromIndex(world, *params.m_UserData);

        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;

            const Point3& position = dmGameObject::GetPosition(component->m_Instance);
            const Quat&   rotation = dmGameObject::GetRotation(component->m_Instance);
            component->m_FromPosition = position;
            component->m_FromRotation = rotation;
            component->m_NextPosition = position;
            component->m_NextRotation = rotation;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
        }
        else if (params.m_Message->m_Id == dmObjectInterpolationDDF::SetApplyTransform::m_DDFDescriptor->m_NameHash)
        {
            dmObjectInterpolationDDF::SetApplyTransform* ddf = (dmObjectInterpolationDDF::SetApplyTransform*)params.m_Message->m_Data;
            if (!UpdateApplyTransform(component, ddf->m_ApplyTransform, ddf->m_TargetObject))
            {
                return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        else if (params.m_Message->m_Descriptor != 0x0)
        {
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool OnResourceReloaded(ObjectInterpolationWorld* world, ObjectInterpolationComponent* component, int index)
    {
        dmhash_t target_object_hash = 0;
        if (component->m_Resource->m_DDF->m_TargetObject[0] != '\0')
        {
            target_object_hash = dmGameObject::GetAbsoluteIdentifier(component->m_Instance, component->m_Resource->m_DDF->m_TargetObject, strlen(component->m_Resource->m_DDF->m_TargetObject));
        }

        UpdateApplyTransform(component, component->m_Resource->m_DDF->m_ApplyTransform, target_object_hash);
        return true;
    }

    void CompObjectInterpolationOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        ObjectInterpolationWorld*     world = (ObjectInterpolationWorld*)params.m_World;
        int                           index = *params.m_UserData;
        ObjectInterpolationComponent* component = GetComponentFromIndex(world, index);
        component->m_Resource = (ObjectInterpolationResource*)params.m_Resource;
        (void)OnResourceReloaded(world, component, index);
    }

    dmGameObject::PropertyResult CompObjectInterpolationGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        ObjectInterpolationWorld*                          world = (ObjectInterpolationWorld*)params.m_World;
        ObjectInterpolationComponent*                      component = GetComponentFromIndex(world, *params.m_UserData);
        dmObjectInterpolationDDF::ObjectInterpolationDesc* ddf = component->m_Resource->m_DDF;

#define HANDLE_PROP(NAME, VALUE) \
    if (params.m_PropertyId == NAME) \
    { \
        out_value.m_Variant = dmGameObject::PropertyVar((VALUE)); \
        return dmGameObject::PROPERTY_RESULT_OK; \
    }

#define PROP_VAR_NAME(prefix, suffix) prefix##suffix
#define HANDLE_PROP_HASH(NAME, VALUE) \
    if (params.m_PropertyId == NAME) \
    { \
        if (VALUE[0] != '\0') \
            out_value.m_Variant = dmGameObject::PropertyVar(dmHashString64(VALUE)); \
        else \
            out_value.m_Variant = dmGameObject::PropertyVar(PROP_VAR_NAME(VALUE, Hash)); \
        return dmGameObject::PROPERTY_RESULT_OK; \
    }

        HANDLE_PROP(PROP_POSITION, dmVMath::Vector3(component->m_NextPosition));
        HANDLE_PROP(PROP_ROTATION, component->m_NextRotation);
        // HANDLE_PROP_HASH(PROP_TARGET_OBJECT, ddf->m_TargetObject);
        HANDLE_PROP(PROP_APPLY_TRANSFORM, (double)component->m_ApplyTransform);

#undef HANDLE_PROP
#undef PROP_VAR_NAME
#undef HANDLE_PROP_HASH
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult CompObjectInterpolationSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        ObjectInterpolationWorld*     world = (ObjectInterpolationWorld*)params.m_World;
        ObjectInterpolationComponent* component = GetComponentFromIndex(world, *params.m_UserData);

        if (params.m_PropertyId == PROP_POSITION)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR3)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            component->m_FromPosition.setX(params.m_Value.m_V4[0]);
            component->m_FromPosition.setY(params.m_Value.m_V4[1]);
            component->m_FromPosition.setZ(params.m_Value.m_V4[2]);
            component->m_NextPosition = component->m_FromPosition;

            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == PROP_ROTATION)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_QUAT)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            component->m_FromRotation.setX(params.m_Value.m_V4[0]);
            component->m_FromRotation.setY(params.m_Value.m_V4[1]);
            component->m_FromRotation.setZ(params.m_Value.m_V4[2]);
            component->m_FromRotation.setW(params.m_Value.m_V4[3]);
            component->m_NextRotation = component->m_FromRotation;

            return dmGameObject::PROPERTY_RESULT_OK;
        }

        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    static dmGameObject::Result CompTypeObjectInterpolationCreate(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        ObjectInterpolationContext* component_context = new ObjectInterpolationContext;

        component_context->m_MaxComponentsPerWorld = dmConfigFile::GetInt(ctx->m_Config, PROJECT_PROPERTY_MAX_COUNT, 1024);

        // Order of systems in Defold:
        // > 50 objectinterpolation <!>
        // > 100 collectionproxy
        // > 200 scripts
        // > 300 gui
        // > 350 spine
        // > 350 rive
        // > 400 collisionobject
        // > 500 camera
        // > 600 sound
        // > 700 model
        // > 725 mesh
        // > 800 particlefx
        // > 900 factory
        // > 950 collectionfactory
        // > 1000 light
        // > 1100 sprite
        // > 1200 tilemap
        // > 1400 label
        ComponentTypeSetPrio(type, 50);

        // Component type setup
        ComponentTypeSetContext(type, component_context);
        ComponentTypeSetHasUserData(type, true);
        ComponentTypeSetReadsTransforms(type, true);

        ComponentTypeSetNewWorldFn(type, CompObjectInterpolationNewWorld);
        ComponentTypeSetDeleteWorldFn(type, CompObjectInterpolationDeleteWorld);
        ComponentTypeSetCreateFn(type, CompObjectInterpolationCreate);
        ComponentTypeSetDestroyFn(type, CompObjectInterpolationDestroy);
        ComponentTypeSetAddToUpdateFn(type, CompObjectInterpolationAddToUpdate);
        ComponentTypeSetUpdateFn(type, CompObjectInterpolationUpdate);
        ComponentTypeSetFixedUpdateFn(type, CompObjectInterpolationFixedUpdate);
        ComponentTypeSetOnMessageFn(type, CompObjectInterpolationOnMessage);
        ComponentTypeSetOnReloadFn(type, CompObjectInterpolationOnReload);
        ComponentTypeSetGetPropertyFn(type, CompObjectInterpolationGetProperty);
        ComponentTypeSetSetPropertyFn(type, CompObjectInterpolationSetProperty);
        ComponentTypeSetGetFn(type, CompObjectInterpolationGetComponent);

        return dmGameObject::RESULT_OK;
    }

    static dmGameObject::Result CompTypeObjectInterpolationDestroy(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        ObjectInterpolationContext* component_context = (ObjectInterpolationContext*)ComponentTypeGetContext(type);
        delete component_context;
        return dmGameObject::RESULT_OK;
    }
} // namespace dmObjectInterpolation

DM_DECLARE_COMPONENT_TYPE(ComponentTypeObjectInterpolationExt, "objectinterpolationc", dmObjectInterpolation::CompTypeObjectInterpolationCreate, dmObjectInterpolation::CompTypeObjectInterpolationDestroy);
