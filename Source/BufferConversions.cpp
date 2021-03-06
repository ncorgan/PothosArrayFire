// Copyright (c) 2019-2020 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include "BufferConversions.hpp"
#include "SharedBufferAllocator.hpp"
#include "Utility.hpp"

#include <Pothos/Exception.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/Util/TypeInfo.hpp>

#include <Poco/Format.h>
#include <Poco/Logger.h>
#include <Poco/NumberFormatter.h>

#include <arrayfire.h>

#include <memory>
#include <vector>

//
// Pothos::BufferChunk <-> af::array
//

template <typename AfArrayType>
Pothos::BufferChunk afArrayTypeToBufferChunk(const AfArrayType& afArray)
{
    auto sharedBuffer = allocateSharedBuffer(
                            af::getBackendId(afArray),
                            afArray.bytes());
    afArray.host(reinterpret_cast<void*>(sharedBuffer.getAddress()));

    auto bufferChunk = Pothos::BufferChunk(sharedBuffer);
    bufferChunk.dtype = Pothos::Object(afArray.type()).convert<Pothos::DType>();

    return bufferChunk;
}

static af::array bufferChunkToAfArray(const Pothos::BufferChunk& bufferChunk)
{
    af::array ret(
        bufferChunk.elements(),
        Pothos::Object(bufferChunk.dtype).convert<af::dtype>());
    ret.write<unsigned char>(
        reinterpret_cast<const unsigned char*>(bufferChunk.address),
        bufferChunk.length,
        ::afHost);

    return ret;
}

//
// std::vector <-> af::array
//

template <typename T>
static af::array convertStdVectorToAfArray(const std::vector<T>& vec)
{
    // If this is ever used for types where type and PothosToAF::type are
    // different, we need two versions.
    static_assert(
        sizeof(T) == sizeof(typename PothosToAF<T>::type),
        "sizeof(T) != sizeof(typename PothosToAF<T>::type");

    return af::array(
               static_cast<dim_t>(vec.size()),
               reinterpret_cast<const typename PothosToAF<T>::type*>(vec.data()),
               ::afHost);
}

template <typename Num, typename Arr>
static std::vector<Num> convertAfArrayToStdVector(const Arr& arr)
{
    assert(Pothos::DType(typeid(Num)) == Pothos::Object(arr.type()).convert<Pothos::DType>());

    std::vector<Num> ret(arr.elements());
    arr.host(ret.data());
    return ret;
}

template <typename T>
static void registerStdVectorConversion(const std::string& typeName)
{
    static const std::string convertPluginSubpath("/object/convert/gpu");

    const std::string stdVectorToAFArrayPluginPath =
        Poco::format(
            "%s/vec%s_to_af_array",
            convertPluginSubpath,
            typeName);
    const std::string afArrayToStdVectorPluginPath =
        Poco::format(
            "%s/af_array_to_vec%s",
            convertPluginSubpath,
            typeName);
    const std::string afArrayProxyToStdVectorPluginPath =
        Poco::format(
            "%s/af_arrayproxy_to_vec%s",
            convertPluginSubpath,
            typeName);

    Pothos::PluginRegistry::add(
        stdVectorToAFArrayPluginPath,
        Pothos::Callable(convertStdVectorToAfArray<T>));
    Pothos::PluginRegistry::add(
        afArrayToStdVectorPluginPath,
        Pothos::Callable(convertAfArrayToStdVector<T,af::array>));
    Pothos::PluginRegistry::add(
        afArrayProxyToStdVectorPluginPath,
        Pothos::Callable(convertAfArrayToStdVector<T,af::array::array_proxy>));
}

pothos_static_block(registerArrayFireBufferConversions)
{
    Pothos::PluginRegistry::add(
        "/object/convert/gpu/afarray_to_bufferchunk",
        Pothos::Callable(&afArrayTypeToBufferChunk<af::array>));
    Pothos::PluginRegistry::add(
        "/object/convert/gpu/afarrayproxy_to_bufferchunk",
        Pothos::Callable(&afArrayTypeToBufferChunk<af::array::array_proxy>));
    Pothos::PluginRegistry::add(
        "/object/convert/gpu/bufferchunk_to_afarray",
        Pothos::Callable(&bufferChunkToAfArray));

    registerStdVectorConversion<float>("float");
    registerStdVectorConversion<double>("double");

    registerStdVectorConversion<std::complex<float>>("cfloat");
    registerStdVectorConversion<std::complex<double>>("cdouble");
}
