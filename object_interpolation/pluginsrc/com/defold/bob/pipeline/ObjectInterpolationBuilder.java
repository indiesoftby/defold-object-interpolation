// Copyright 2025 Indiesoft LLC
// Licensed under the MIT License.

package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.BuilderUtil;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.objectinterpolation.proto.ObjectInterpolation.ObjectInterpolationDesc;

@ProtoParams(srcClass = ObjectInterpolationDesc.class, messageClass = ObjectInterpolationDesc.class)
@BuilderParams(name="ObjectInterpolation", inExts=".objectinterpolation", outExt=".objectinterpolationc")
public class ObjectInterpolationBuilder extends ProtoBuilder<ObjectInterpolationDesc.Builder> {
    @Override
    protected ObjectInterpolationDesc.Builder transform(Task task, IResource resource, ObjectInterpolationDesc.Builder builder) throws CompileExceptionError {
        // Nothing to do here
        return builder;
    }
}
