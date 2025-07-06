#include <memory.h>

#include <dmsdk/dlib/log.h>
#include <dmsdk/resource/resource.h>

#include "objectinterpolation_ddf.h" // generated from the objectinterpolation_ddf.proto
#include "res_objectinterpolation.h"

namespace dmObjectInterpolation
{
    static dmResource::Result ResourceCreate(const dmResource::ResourceCreateParams* params)
    {
        dmObjectInterpolationDDF::ObjectInterpolationDesc* desc;
        dmDDF::Result                                      e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &desc);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        // We hand out a pointer to a pointer, in order to be able to recreate the assets
        // when hot reloading
        ObjectInterpolationResource* resource = new ObjectInterpolationResource;
        resource->m_DDF = desc;
        dmResource::SetResource(params->m_Resource, resource);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceDestroy(const dmResource::ResourceDestroyParams* params)
    {
        ObjectInterpolationResource* resource = (ObjectInterpolationResource*)dmResource::GetResource(params->m_Resource);
        dmDDF::FreeMessage(resource->m_DDF);
        delete resource;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResourceRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmObjectInterpolationDDF::ObjectInterpolationDesc* desc;
        dmDDF::Result                                      e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &desc);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        ObjectInterpolationResource* resource = (ObjectInterpolationResource*)dmResource::GetResource(params->m_Resource);
        dmDDF::FreeMessage(resource->m_DDF);
        resource->m_DDF = desc;
        return dmResource::RESULT_OK;
    }

    static ResourceResult ResourceType_Register(HResourceTypeContext ctx, HResourceType type)
    {
        return (ResourceResult)dmResource::SetupType(ctx,
                                                     type,
                                                     0, // context
                                                     0, // preload
                                                     ResourceCreate,
                                                     0, // post create
                                                     ResourceDestroy,
                                                     ResourceRecreate);
    }
} // namespace dmObjectInterpolation

DM_DECLARE_RESOURCE_TYPE(ResourceTypeObjectInterpolationExt, "objectinterpolationc", dmObjectInterpolation::ResourceType_Register, 0);
