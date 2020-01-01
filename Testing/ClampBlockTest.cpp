// Copyright (c) 2019 Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

#include <arrayfire.h>

#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Testing.hpp>

#if AF_API_VERSION_CURRENT >= 34

#include "BlockExecutionTests.hpp"
#include "TestUtility.hpp"
#include "Utility.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <typeinfo>
#include <vector>

#define GET_MINMAX_OBJECTS(typeStr, cType) \
    if(type == typeStr) \
    { \
        const auto sortedInputs = getTestInputs<cType>(false /*shuffle*/); \
        assert(sortedInputs.size() >= 6); \
 \
        (*pMinObjectOut) = Pothos::Object(sortedInputs[2]); \
        (*pMaxObjectOut) = Pothos::Object(sortedInputs[sortedInputs.size()-3]); \
    }

void getMinMaxObjects(
    const std::string& type,
    Pothos::Object* pMinObjectOut,
    Pothos::Object* pMaxObjectOut)
{
    assert(nullptr != pMinObjectOut);
    assert(nullptr != pMaxObjectOut);

    // ArrayFire doesn't support int8
    GET_MINMAX_OBJECTS("int16", std::int16_t)
    GET_MINMAX_OBJECTS("int32", std::int32_t)
    GET_MINMAX_OBJECTS("int64", std::int64_t)
    GET_MINMAX_OBJECTS("uint8", std::uint8_t)
    GET_MINMAX_OBJECTS("uint16", std::uint16_t)
    GET_MINMAX_OBJECTS("uint32", std::uint32_t)
    GET_MINMAX_OBJECTS("uint64", std::uint64_t)
    GET_MINMAX_OBJECTS("float32", float)
    GET_MINMAX_OBJECTS("float64", double)
    // This block has no complex implementation.
}

static void testClampBlock(
    const std::string& type,
    size_t numChannels)
{
    static constexpr const char* blockRegistryPath = "/arrayfire/arith/clamp";

    std::cout << "Testing " << blockRegistryPath
              << " (type: " << type
              << ", chans: " << numChannels << ")" << std::endl;

    Pothos::Object minObject, maxObject;
    getMinMaxObjects(type, &minObject, &maxObject);

    Pothos::DType dtype(type);
    if(isDTypeComplexFloat(dtype))
    {
        POTHOS_TEST_THROWS(
            Pothos::BlockRegistry::make(
                blockRegistryPath,
                type,
                minObject,
                maxObject,
                numChannels),
        Pothos::ProxyExceptionMessage);
    }
    else
    {
        auto block = Pothos::BlockRegistry::make(
                         blockRegistryPath,
                         type,
                         minObject,
                         maxObject,
                         numChannels);

        std::vector<Pothos::BufferChunk> testInputs;
        std::vector<Pothos::Proxy> feederSources;
        std::vector<Pothos::Proxy> collectorSinks;

        for(size_t chan = 0; chan < numChannels; ++chan)
        {
            testInputs.emplace_back(getTestInputs(type));

            feederSources.emplace_back(
                Pothos::BlockRegistry::make(
                    "/blocks/feeder_source",
                    dtype));
            feederSources.back().call(
                "feedBuffer",
                testInputs[chan]);

            collectorSinks.emplace_back(
                Pothos::BlockRegistry::make(
                    "/blocks/collector_sink",
                    dtype));
        }

        // Execute the topology.
        {
            Pothos::Topology topology;
            for(size_t chan = 0; chan < numChannels; ++chan)
            {
                topology.connect(
                    feederSources[chan],
                    0,
                    block,
                    chan);
                topology.connect(
                    block,
                    chan,
                    collectorSinks[chan],
                    0);
            }

            topology.commit();
            POTHOS_TEST_TRUE(topology.waitInactive(0.05));
        }

        // Make sure the blocks output data.
        // TODO: output verification
        for(size_t chan = 0; chan < numChannels; ++chan)
        {
            const auto& chanInputs = testInputs[chan];
            const size_t numInputs = chanInputs.elements();

            auto chanOutputs = collectorSinks[chan].call<Pothos::BufferChunk>("getBuffer");
            POTHOS_TEST_EQUAL(
                numInputs,
                chanOutputs.elements());
        }
    }
}

void testClampBlockForType(const std::string& type)
{
    testClampBlock(type, 1);
    testClampBlock(type, 3);
}

#else
// TODO: make sure block isn't in registry
#endif