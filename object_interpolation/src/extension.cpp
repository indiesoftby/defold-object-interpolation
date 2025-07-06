
#include <dmsdk/sdk.h>

#include "comp_object_interpolation.h"

static int SetInterpolationEnabled(lua_State* L)
{
    bool enabled = lua_toboolean(L, 1);
    dmObjectInterpolation::SetInterpolationEnabled(enabled);
    return 0;
}

static int IsInterpolationEnabled(lua_State* L)
{
    bool enabled = dmObjectInterpolation::IsInterpolationEnabled();
    lua_pushboolean(L, enabled);
    return 1;
}

static dmExtension::Result AppInitializeObjectInterpolation(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static const luaL_reg OBJECTINTERPOLATION_COMP_FUNCTIONS[] = {
    { "set_enabled", SetInterpolationEnabled },
    { "is_enabled", IsInterpolationEnabled },
    { 0, 0 }
};

static dmExtension::Result InitializeObjectInterpolation(dmExtension::Params* params)
{
    int top = lua_gettop(params->m_L);
    luaL_register(params->m_L, "object_interpolation", OBJECTINTERPOLATION_COMP_FUNCTIONS);

#define SETCONSTANT(name, val) \
    lua_pushnumber(params->m_L, (lua_Number)val); \
    lua_setfield(params->m_L, -2, #name);

    SETCONSTANT(APPLY_TRANSFORM_NONE, dmObjectInterpolation::APPLY_TRANSFORM_NONE);
    SETCONSTANT(APPLY_TRANSFORM_TARGET, dmObjectInterpolation::APPLY_TRANSFORM_TARGET);

    lua_pop(params->m_L, 1);
    assert(top == lua_gettop(params->m_L));

#undef SETCONSTANT

    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeObjectInterpolation(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeObjectInterpolation(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ObjectInterpolationExt, "ObjectInterpolationExt", AppInitializeObjectInterpolation, AppFinalizeObjectInterpolation, InitializeObjectInterpolation, 0, 0, FinalizeObjectInterpolation);
